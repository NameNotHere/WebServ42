*This project has been created as part of the 42 curriculum by <jmcgrane>, <skramer>, <orhan>.*

# Webserv

## Description

Webserv is a small HTTP server written in C++. It uses `poll()` to handle
multiple client connections and supports GET, POST, and DELETE requests. The
server can serve static files, accept uploads, run CGI programs, and load
server and route settings from a configuration file.

## Instructions

Build the server from the project root:

make

Start it with a configuration file:

./webserv epic.conf

The supplied `epic.conf` listens on ports 2020 and 8080. Stop the server with
Ctrl-C. Configuration paths are read relative to the directory where the
server is started.

## Resources

- [RFC 9110: HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110.html)
- [RFC 3875: The Common Gateway Interface (CGI)](https://www.rfc-editor.org/rfc/rfc3875.html)
- `poll(2)`, `socket(2)`, `recv(2)`, and `send(2)` manual pages
- The 42 Webserv subject

AI tools were used as a debugging and code-review aid. They helped investigate
the poll-based request and response handling in the server and
large CGI request handling.
