#include "Responder.hpp"
#include <deque>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
    const size_t MAX_COALESCED_OUTPUT = 64 * 1024;

    struct OutputSegment
    {
        std::string bytes;
        size_t offset;
        int fileFd;
        off_t fileRemaining;
        bool isFile;

        OutputSegment(const std::string& data)
            : bytes(data), offset(0), fileFd(-1), fileRemaining(0), isFile(false) {}

        OutputSegment(std::string&& data)
            : bytes(std::move(data)), offset(0), fileFd(-1), fileRemaining(0), isFile(false) {}

        OutputSegment(int fd, off_t size)
            : offset(0), fileFd(fd), fileRemaining(size), isFile(true) {}
    };

    struct ClientOutput
    {
        std::deque<OutputSegment> segments;
        bool closeWhenDrained;

        ClientOutput() : closeWhenDrained(false) {}
    };

    enum OutputFlushStep
    {
        OUTPUT_FLUSH_ERROR,
        OUTPUT_FLUSH_CONTINUE,
        OUTPUT_FLUSH_PAUSE
    };

    std::map<int, ClientOutput> clientOutputs;

    bool loadFileChunk(OutputSegment& segment)
    {
        if (!segment.isFile || segment.offset != segment.bytes.size()
            || segment.fileRemaining <= 0)
            return true;

        char buffer[16384];
        size_t amount = sizeof(buffer);
        if (segment.fileRemaining < static_cast<off_t>(amount))
            amount = static_cast<size_t>(segment.fileRemaining);

        ssize_t bytesRead = read(segment.fileFd, buffer, amount);
        if (bytesRead <= 0)
            return false;

        segment.bytes.assign(buffer, static_cast<size_t>(bytesRead));
        segment.offset = 0;
        segment.fileRemaining -= bytesRead;
        return true;
    }

    bool getCoalescedBatchSize(const ClientOutput& output, size_t& batchSize, size_t& batchCount)
    {
        batchSize = 0;
        batchCount = 0;

        for (std::deque<OutputSegment>::const_iterator it =
             output.segments.begin();
             it != output.segments.end() && !it->isFile; ++it)
        {
            size_t available = it->bytes.size() - it->offset;
            if (available > MAX_COALESCED_OUTPUT - batchSize)
            {
                batchSize = 0;
                batchCount = 0;
                return false;
            }
            batchSize += available;
            ++batchCount;
        }
        return true;
    }

    std::string makeCoalescedBatch(const ClientOutput& output, size_t batchSize)
    {
        std::string batch;
        batch.reserve(batchSize);
        for (std::deque<OutputSegment>::const_iterator it =
                 output.segments.begin();
             it != output.segments.end() && !it->isFile; ++it)
            batch.append(it->bytes, it->offset, it->bytes.size() - it->offset);
        return batch;
    }

    void consumeCoalescedBytes(ClientOutput& output, size_t sentBytes)
    {
        while (sentBytes > 0 && !output.segments.empty())
        {
            OutputSegment& segment = output.segments.front();
            size_t available = segment.bytes.size() - segment.offset;
            if (sentBytes >= available)
            {
                sentBytes -= available;
                output.segments.pop_front();
            }
            else
            {
                segment.offset += sentBytes;
                sentBytes = 0;
            }
        }
    }

    OutputFlushStep sendCoalescedBatch(int clientFd, ClientOutput& output, size_t batchSize)
    {
        std::string batch = makeCoalescedBatch(output, batchSize);
        ssize_t sent = send(clientFd, batch.data(), batch.size(), 0);
        if (sent < 0)
            return OUTPUT_FLUSH_PAUSE;
        if (sent == 0)
            return OUTPUT_FLUSH_ERROR;

        consumeCoalescedBytes(output, static_cast<size_t>(sent));
        return OUTPUT_FLUSH_PAUSE;
    }

    OutputFlushStep sendFrontSegment(int clientFd, ClientOutput& output)
    {
        OutputSegment& segment = output.segments.front();
        ssize_t sent = send(clientFd, segment.bytes.data() + segment.offset,
                            segment.bytes.size() - segment.offset, 0);
        if (sent < 0)
            return OUTPUT_FLUSH_PAUSE;
        if (sent == 0)
            return OUTPUT_FLUSH_ERROR;

        segment.offset += static_cast<size_t>(sent);
        if (segment.offset < segment.bytes.size())
            return OUTPUT_FLUSH_PAUSE;

        if (segment.isFile)
        {
            if (segment.fileRemaining == 0)
            {
                if (segment.fileFd != -1)
                    close(segment.fileFd);
                output.segments.pop_front();
            }
            return OUTPUT_FLUSH_CONTINUE;
        }

        output.segments.pop_front();
        return OUTPUT_FLUSH_PAUSE;
    }

    void removeEmptyFrontSegment(ClientOutput& output)
    {
        OutputSegment& segment = output.segments.front();
        if (!segment.isFile || segment.fileRemaining == 0)
        {
            if (segment.isFile && segment.fileFd != -1)
                close(segment.fileFd);
            output.segments.pop_front();
        }
    }
}

ssize_t sendAll(int fd, const char* buf, size_t len)
{
    if (len == 0)
        return 0;
    clientOutputs[fd].segments.push_back(
        OutputSegment(std::string(buf, len)));
    return static_cast<ssize_t>(len);
}

bool hasPendingOutput(int clientFd)
{
    std::map<int, ClientOutput>::const_iterator it = clientOutputs.find(clientFd);
    return it != clientOutputs.end() && !it->second.segments.empty();
}

bool shouldCloseAfterOutput(int clientFd)
{
    std::map<int, ClientOutput>::const_iterator it = clientOutputs.find(clientFd);
    return it != clientOutputs.end() && it->second.closeWhenDrained;
}

void closeAfterPendingOutput(int clientFd)
{
    clientOutputs[clientFd].closeWhenDrained = true;
}

void clearPendingOutput(int clientFd)
{
    std::map<int, ClientOutput>::iterator it = clientOutputs.find(clientFd);
    if (it == clientOutputs.end())
        return;
    for (size_t i = 0; i < it->second.segments.size(); ++i)
        if (it->second.segments[i].isFile && it->second.segments[i].fileFd != -1)
            close(it->second.segments[i].fileFd);
    clientOutputs.erase(it);
}

bool flushPendingOutput(int clientFd)
{
    std::map<int, ClientOutput>::iterator output = clientOutputs.find(clientFd);
    if (output == clientOutputs.end())
        return true;

    while (!output->second.segments.empty())
    {
        OutputSegment& segment = output->second.segments.front();
        if (!loadFileChunk(segment))
            return false;

        if (segment.offset < segment.bytes.size())
        {
            if (!segment.isFile)
            {
                size_t batchSize = 0;
                size_t batchCount = 0;
                getCoalescedBatchSize(output->second, batchSize, batchCount);
                if (batchCount > 1)
                {
                    OutputFlushStep step = sendCoalescedBatch(clientFd, output->second, batchSize);
                    return step != OUTPUT_FLUSH_ERROR;
                }
            }

            OutputFlushStep step = sendFrontSegment(clientFd, output->second);
            if (step == OUTPUT_FLUSH_ERROR)
                return false;
            if (step == OUTPUT_FLUSH_PAUSE)
                return true;
            continue;
        }
        removeEmptyFrontSegment(output->second);
    }
    return true;
}

bool sendFileBody(int clientFd, int fd_in, off_t fileSize)
{
    if (fileSize <= 0)
    {
        close(fd_in);
        return true;
    }
    clientOutputs[clientFd].segments.push_back(OutputSegment(fd_in, fileSize));
    return true;
}

bool queueCgiResponse(int clientFd, const CgiJob& job, CGIResult& result, const ServerConfig* config)
{
    if (result.timed_out || result.io_error || result.exit_status != 0)
        return sendErrorResponse(clientFd, job.httpVersion, job.keepAlive, 500, config);

    CgiResponse parsed;
    if (!parsed.parse(result.raw_output))
        return sendErrorResponse(clientFd, job.httpVersion, job.keepAlive, 500, config);

    std::string().swap(result.raw_output);
    std::ostringstream response;

    response << job.httpVersion << " " << parsed.status_code << " "
             << parsed.status_message << "\r\n";
    for (std::map<std::string, std::string>::const_iterator it =
             parsed.headers.begin(); it != parsed.headers.end(); ++it)
    {
        if (it->first == "content-length" || it->first == "connection")
            continue;
        response << it->first << ": " << it->second << "\r\n";
    }
    response << "Content-Length: " << parsed.body.size() << "\r\n";
    if (job.keepAlive)
        response << "Connection: keep-alive\r\n";
    else
        response << "Connection: close\r\n";
    response << "\r\n";

    ClientOutput& output = clientOutputs[clientFd];
    output.segments.push_back(OutputSegment(response.str()));
    output.segments.push_back(OutputSegment(std::move(parsed.body)));
    return true;
}
