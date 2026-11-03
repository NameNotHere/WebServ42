#include "HttpParser.hpp"
#include <vector>
#include <utility>

HttpParser::BodyStatus HttpParser::body(const std::string& request, size_t maxBody)
{
	_body.clear();
	size_t start = request.find("\r\n\r\n");
	if(start == std::string::npos)
   		return BODY_INVALID;

	const size_t bodyStart = start + 4;

	bool hasContentLength = _headers.find("content-length") != _headers.end();
	bool hasTransferEncoding = _headers.find("transfer-encoding") != _headers.end();

	if(!hasContentLength && !hasTransferEncoding)
	{
    	_requestLength = bodyStart;
    	return BODY_VALID;
	}

	if(hasTransferEncoding && hasContentLength)
		return BODY_INVALID;

	if(hasTransferEncoding)
	{
		size_t bodyLength = 0;
		BodyStatus status = transferEncoding(request, bodyStart, bodyLength, maxBody);

		if(status == BODY_INVALID)
			return BODY_INVALID;
		if(status == BODY_INCOMPLETE)
			return BODY_INCOMPLETE;
		if(status == BODY_TOO_LARGE)
			return BODY_TOO_LARGE;

		_requestLength = start + 4 + bodyLength;
		return BODY_VALID;
	}
	else if(hasContentLength)
	{
		std::map<std::string, std::string>::const_iterator it;
		it = _headers.find("content-length");
		// Checked if this fails already in the program
		unsigned long contentLength = stoul(it->second);
		if (maxBody != 0 && contentLength > maxBody)
			return BODY_TOO_LARGE;

		if(contentLength > request.size() - bodyStart)
		{
			return BODY_INCOMPLETE;
		}
		_requestLength = bodyStart + contentLength;
		_body = request.substr(bodyStart, contentLength);
		return BODY_VALID;
	}
	_requestLength = start + 4;
	return BODY_VALID;
}

HttpParser::BodyStatus HttpParser::transferEncoding(
    const std::string& request, size_t bodyStart, size_t& bodyLength, size_t maxBody)
{
    size_t pos = bodyStart;
    size_t decodedSize = 0;
    std::vector<std::pair<size_t, size_t> > chunks;

    while (true)
    {
        size_t bytesPos = request.find("\r\n", pos);
        if (bytesPos == std::string::npos)
            return BODY_INCOMPLETE;

        std::string bytes = request.substr(pos, bytesPos - pos);
        if (bytes.empty())
            return BODY_INVALID;

        size_t digit = 0;
        if (!convertBytes(bytes, digit))
            return BODY_INVALID;

        pos = bytesPos + 2;
        if (digit == 0)
        {
            if (request.size() - pos < 2)
                return BODY_INCOMPLETE;
            if (request.compare(pos, 2, "\r\n") != 0)
                return BODY_INVALID;

            _body.clear();
            _body.reserve(decodedSize);
            for (size_t i = 0; i < chunks.size(); ++i)
                _body.append(request, chunks[i].first, chunks[i].second);

            bodyLength = pos + 2 - bodyStart;
            return BODY_VALID;
        }

        if (maxBody != 0 && (decodedSize > maxBody || digit > maxBody - decodedSize))
            return BODY_TOO_LARGE;
        if (request.size() - pos < digit
            || request.size() - pos - digit < 2)
            return BODY_INCOMPLETE;

        chunks.push_back(std::make_pair(pos, digit));
        decodedSize += digit;
        pos += digit;
        if (request.compare(pos, 2, "\r\n") != 0)
            return BODY_INVALID;
        pos += 2;
    }
}

bool HttpParser::convertBytes(std::string& bytes, size_t& digit)
{
	try
	{
		digit = stoul(bytes, 0, 16);
	}
	catch(const std::exception& e)
	{
		return false;
	}
	return true;
}
