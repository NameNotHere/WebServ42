#!/bin/bash

HOST="localhost"
PORT="2020"

passed=0
failed=0

test_request()
{
    name="$1"
    request="$2"
    expected="$3"

    result=$(printf "$request" | nc -w 2 $HOST $PORT)

    if echo "$result" | grep -q "$expected"; then
        echo "[PASS] $name"
    else
        echo "[FAIL] $name"
    fi

    echo "  Input:"
    printf '%b' "$request" | cat -v
    echo "  Expected: $expected"
    echo "  Got:"
    echo "$result" | head -n 1
    echo

    if echo "$result" | grep -q "$expected"; then
        ((passed++))
    else
        ((failed++))
    fi
}


echo "=============================="
echo "      WEBSERV HTTP TESTS"
echo "=============================="
echo

test_request \
    "Basic GET" \
    "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK"

test_request \
    "GET with query" \
    "GET /test?hello=world HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK"

test_request \
    "Unsupported PUT" \
    "PUT / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "405 Method Not Allowed"

test_request \
    "Unsupported PATCH" \
    "PATCH / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "405 Method Not Allowed"

test_request \
    "Unsupported OPTIONS" \
    "OPTIONS / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "405 Method Not Allowed"

test_request \
    "HTTP 2.0" \
    "GET / HTTP/2.0\r\nHost: localhost\r\n\r\n" \
    "505 HTTP Version Not Supported"

test_request \
    "Malformed request line" \
    "GET garbage garbage\r\nHost: localhost\r\n\r\n" \
    "400 Bad Request"

test_request \
    "Completely garbage" \
    "this is not HTTP\r\n\r\n" \
    "400 Bad Request"

test_request \
    "Missing Host" \
    "GET / HTTP/1.1\r\n\r\n" \
    "400 Bad Request"

test_request \
    "Duplicate Host" \
    "GET / HTTP/1.1\r\nHost: localhost\r\nHost: localhost\r\n\r\n" \
    "400 Bad Request"

test_request \
    "Invalid Content-Length" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: abc\r\n\r\n" \
    "400 Bad Request"

test_request \
    "Negative Content-Length" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: -5\r\n\r\n" \
    "400 Bad Request"

test_request \
    "POST complete" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nHello" \
    "200 OK"

test_request \
    "POST incomplete" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10\r\n\r\nHello" \
    ""

test_request \
    "Chunked POST" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n" \
    "200 OK"

test_request \
    "Invalid chunk size" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\nZZZ\r\nHello\r\n0\r\n\r\n" \
    "400 Bad Request"

test_request \
    "Lowercase Host" \
    "GET / HTTP/1.1\r\nhost: localhost\r\n\r\n" \
    "200 OK"

test_request \
    "Mixed-case Host" \
    "GET / HTTP/1.1\r\nHoSt: localhost\r\n\r\n" \
    "200 OK"

echo "=============================="
echo "Passed: $passed"
echo "Failed: $failed"
echo "=============================="