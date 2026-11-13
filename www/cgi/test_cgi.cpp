#include "CgiResponse.hpp"
#include <iostream>
#include <cstdlib>

int main(int argc, char** argv)
{
    CGIRequest req;
    req.method         = "POST";
    req.script_path = "./test.py";
    if (argc > 1)
        req.script_path = argv[1];
    req.script_name    = "/cgi-bin/test.py";
    req.path_info      = "";
    req.query_string   = "name=Bob";
    if (argc > 2)
        req.body.assign(static_cast<size_t>(std::atoi(argv[2])), 'x');
    else
        req.body           = "hello body";
    req.server_name    = "localhost";
    req.server_port    = "8080";
    req.server_protocol= "HTTP/1.1";
    req.server_software= "webserv-test/1.0";
    req.remote_addr    = "127.0.0.1";
    req.headers["content-type"]   = "text/plain";
    req.headers["content-length"] = std::to_string(req.body.size());
    req.headers["user-agent"]     = "test";

    unsigned int timeout = 5;
    if (argc > 3)
        timeout = static_cast<unsigned int>(std::atoi(argv[3]));
    if (argc > 4)
        req.query_string = argv[4];
    CgiHandler handler("/usr/bin/python3", timeout);
    CGIResult  result = handler.execute(req);

    std::cout << "exit=" << result.exit_status << "\n";
    std::cout << "timed_out=";
    if (result.timed_out)
        std::cout << "yes\n";
    else
        std::cout << "no\n";
    std::cout << "io_error=";
    if (result.io_error)
        std::cout << "yes\n";
    else
        std::cout << "no\n";
    if (!result.error_message.empty())
        std::cout << "error=" << result.error_message << "\n";
    std::cout << "---raw---\n" << result.raw_output << "\n";

    CgiResponse resp;
    if (resp.parse(result.raw_output))
    {
        std::cout << "---parsed---\n";
        std::cout << "status=" << resp.status_code << " " << resp.status_message << "\n";
        for (auto& h : resp.headers)
            std::cout << h.first << ": " << h.second << "\n";
        std::cout << "body:\n" << resp.body << "\n";
    }
    else
        std::cout << "CGI response is malformed or missing Content-Type/Location\n";
    return 0;
}
