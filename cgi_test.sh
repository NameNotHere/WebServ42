#!/bin/bash

HOST="${HOST:-localhost}"
PORT="${PORT:-2020}"

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
CGI_SCRIPT="$ROOT_DIR/www/cgi/test.py"
CGI_TESTER="/tmp/webserv-cgi-tester"
DIRECT_LOG="/tmp/webserv-cgi-direct.log"

passed=0
failed=0

pass()
{
    echo "[PASS] $1"
    ((passed++))
}

fail()
{
    echo "[FAIL] $1"
    echo "       $2"
    ((failed++))
}

require_command()
{
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing required command: $1"
        exit 2
    fi
}

run_direct_cgi()
{
    name="$1"
    body_size="$2"
    timeout="$3"
    query="$4"
    expected="$5"
    fix="$6"

    "$CGI_TESTER" "$CGI_SCRIPT" "$body_size" "$timeout" "$query" \
        >"$DIRECT_LOG" 2>&1

    if grep -Fq "$expected" "$DIRECT_LOG"; then
        pass "$name"
    else
        fail "$name" "Expected '$expected'. Suggested fix: $fix"
        echo "       Actual output:"
        sed 's/^/       | /' "$DIRECT_LOG"
    fi
}

run_http_cgi()
{
    name="$1"
    request="$2"
    expected="$3"
    fix="$4"

    result=$(printf '%b' "$request" | nc -w 3 "$HOST" "$PORT" 2>&1)

    if printf '%s' "$result" | grep -Fq "$expected"; then
        pass "$name"
    else
        fail "$name" "Expected '$expected'. Suggested fix: $fix"
        echo "       Request:"
        printf '%b' "$request" | cat -v | sed 's/^/       | /'
        echo "       Response:"
        printf '%s\n' "$result" | head -n 10 | cat -v | sed 's/^/       | /'
    fi
}

require_command c++
require_command nc

if [ ! -f "$CGI_SCRIPT" ]; then
    echo "CGI script not found: $CGI_SCRIPT"
    exit 2
fi

echo "======================================"
echo "       WEBSERV + CGI TESTS"
echo "======================================"
echo

echo "[1/3] Running existing HTTP tests"
echo "--------------------------------------"

if "$ROOT_DIR/test.sh"; then
    echo "Existing HTTP tests completed."
else
    echo "The existing HTTP tests failed to run."
    echo "Make sure the server is running:"
    echo "  ./webserv epic.conf"
    exit 1
fi

echo
echo "[2/3] Compiling direct CGI tester"
echo "--------------------------------------"

if ! c++ -Wall -Wextra -Werror -std=c++17 \
    "$ROOT_DIR/www/cgi/test_cgi.cpp" \
    "$ROOT_DIR/www/cgi/CgiHandler.cpp" \
    "$ROOT_DIR/www/cgi/CgiHelper.cpp" \
    "$ROOT_DIR/www/cgi/CgiResponse.cpp" \
    -o "$CGI_TESTER"; then

    echo "Could not compile the CGI tester."
    echo "Fix the compiler errors above."
    exit 1
fi

echo
echo "[3/3] Direct CGI tests"
echo "--------------------------------------"

run_direct_cgi \
    "CGI process starts successfully" \
    0 \
    5 \
    "name=Bob" \
    "exit=0" \
    "check /usr/bin/python3 and the CGI script path"

run_direct_cgi \
    "CGI response contains Content-Type" \
    0 \
    5 \
    "name=Bob" \
    "content-type: text/plain" \
    "check CgiResponse header parsing"

run_direct_cgi \
    "Query string reaches CGI environment" \
    0 \
    5 \
    "name=Query" \
    "Query : name=Query" \
    "check QUERY_STRING construction"

run_direct_cgi \
    "POST body reaches CGI stdin" \
    11 \
    5 \
    "name=Body" \
    "Body  : xxxxxxxxxxx" \
    "check CONTENT_LENGTH and the CGI stdin pipe"

run_direct_cgi \
    "CGI timeout is detected" \
    0 \
    1 \
    "sleep=2" \
    "timed_out=yes" \
    "check timeout handling, SIGTERM, and process cleanup"

echo
echo "HTTP CGI routing tests"
echo "--------------------------------------"

run_http_cgi \
    "HTTP GET executes Python CGI" \
    "GET /cgi/test.py?name=Socket HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK" \
    "check that /cgi/test.py exists below the configured root"

run_http_cgi \
    "HTTP CGI receives query string" \
    "GET /cgi/test.py?name=Socket HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "Query : name=Socket" \
    "check target splitting and QUERY_STRING forwarding"

run_http_cgi \
    "HTTP POST body reaches CGI" \
    "POST /cgi/test.py HTTP/1.1\r\nHost: localhost\r\nContent-Type: text/plain\r\nContent-Length: 5\r\n\r\nHello" \
    "Body  : Hello" \
    "check HTTP body parsing and CGI stdin forwarding"

echo
echo "Advanced HTTP CGI tests"
echo "--------------------------------------"

run_http_cgi \
    "CGI GET method is forwarded" \
    "GET /cgi/test.py?method=check HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "Method: GET" \
    "check REQUEST_METHOD handling"

run_http_cgi \
    "CGI receives an empty POST body" \
    "POST /cgi/test.py HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n" \
    "Body  :" \
    "check zero-length body handling"

run_http_cgi \
    "CGI receives a large POST body" \
    "POST /cgi/test.py HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1000\r\n\r\n$(printf 'A%.0s' {1..1000})" \
    "200 OK" \
    "check pipe buffering and large request bodies"

run_http_cgi \
    "CGI preserves special query characters" \
    "GET /cgi/test.py?name=hello%20world%26value%3D123 HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "Query : name=hello%20world%26value%3D123" \
    "check QUERY_STRING is forwarded without corruption"

run_http_cgi \
    "CGI response contains Content-Type" \
    "GET /cgi/test.py HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "Content-Type: text/plain" \
    "check CGI response header conversion"

run_http_cgi \
    "CGI response contains Content-Length" \
    "GET /cgi/test.py HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "Content-Length:" \
    "check HTTP response framing"

run_http_cgi \
    "Missing CGI script returns an error" \
    "GET /cgi/does-not-exist.py HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "500 Internal Server Error" \
    "check exec failure handling"

run_http_cgi \
    "CGI timeout returns an error" \
    "GET /cgi/test.py?sleep=6 HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n" \
    "500 Internal Server Error" \
    "check CGI timeout handling"

run_http_cgi \
    "CGI handles lowercase headers" \
    "post /cgi/test.py HTTP/1.1\r\nhost: localhost\r\ncontent-type: text/plain\r\ncontent-length: 5\r\n\r\nHello" \
    "Body  : Hello" \
    "check case-insensitive HTTP header handling"

run_http_cgi \
    "CGI handles query-only target" \
    "GET /cgi/test.py? HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK" \
    "check empty QUERY_STRING handling"

run_http_cgi \
    "CGI rejects malformed request framing" \
    "POST /cgi/test.py HTTP/1.1\r\nHost: localhost\r\nContent-Length: abc\r\n\r\nHello" \
    "400 Bad Request" \
    "check Content-Length validation before CGI execution"

run_http_cgi \
    "CGI rejects duplicate Content-Length" \
    "POST /cgi/test.py HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nContent-Length: 5\r\n\r\nHello" \
    "400 Bad Request" \
    "check duplicate request-header validation"

run_http_cgi \
    "CGI rejects invalid transfer encoding" \
    "POST /cgi/test.py HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: banana\r\n\r\n" \
    "400 Bad Request" \
    "check Transfer-Encoding validation"

run_http_cgi \
    "CGI request with keep-alive returns connection header" \
    "GET /cgi/test.py HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n" \
    "Connection:" \
    "check response connection management"

run_http_cgi \
    "HEAD behavior is visible" \
    "HEAD /cgi/test.py HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK" \
    "check whether HEAD incorrectly sends a response body"

run_http_cgi \
    "Unsupported CGI method is handled" \
    "PUT /cgi/test.py HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n" \
    "405 Method Not Allowed" \
    "check unsupported HTTP method handling"



echo
echo "======================================"
echo "Passed: $passed"
echo "Failed: $failed"
echo "======================================"

rm -f "$CGI_TESTER" "$DIRECT_LOG"

if [ "$failed" -ne 0 ]; then
    echo "Some tests failed."
    exit 1
fi

echo "All tests passed."
exit 0