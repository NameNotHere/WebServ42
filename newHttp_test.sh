#!/bin/bash

HOST="localhost"
PORT="2020"
TIMEOUT=2

passed=0
failed=0

# Each test opens a fresh connection.
test_request()
{
    local name="$1"
    local request="$2"
    local expected="$3"

    local result
    result=$(printf '%b' "$request" | nc -w "$TIMEOUT" "$HOST" "$PORT" 2>/dev/null)

    local status_line
    status_line=$(printf '%s\n' "$result" | grep -m 1 -E '^HTTP/[0-9.]+ [0-9]{3}' || true)

    if [ "$expected" = "NO RESPONSE" ]; then
        if [ -z "$result" ]; then
            status="[PASS]"
            ((passed += 1))
        else
            status="[FAIL]"
            ((failed += 1))
        fi
    elif printf '%s\n' "$result" | grep -Fq "$expected"; then
        status="[PASS]"
        ((passed += 1))
    else
        status="[FAIL]"
        ((failed += 1))
    fi

    printf '%s %s\n' "$status" "$name"
    if [ "$status" = "[FAIL]" ]; then
        printf '  Expected: %s\n' "$expected"
        printf '  Output:   %s\n' "${status_line:-NO RESPONSE}"
        printf '  Response headers:\n'
        printf '%s\n' "$result" | head -n 8
        printf '\n'
    fi
}

# Send multiple requests on one connection and count HTTP status lines.
test_pipeline()
{
    local name="$1"
    local request="$2"
    local expected_count="$3"

    local result
    result=$(printf '%b' "$request" | nc -w "$TIMEOUT" "$HOST" "$PORT" 2>/dev/null)

    local actual_count
    actual_count=$(printf '%s' "$result" |
        grep -aoE 'HTTP/[0-9.]+ [0-9]{3}' |
        wc -l)

    if [ "$actual_count" -eq "$expected_count" ]; then
        status="[PASS]"
        ((passed += 1))
    else
        status="[FAIL]"
        ((failed += 1))
    fi

    printf '%s %s\n' "$status" "$name"
    if [ "$status" = "[FAIL]" ]; then
        printf '  Expected %s HTTP responses, got %s\n' "$expected_count" "$actual_count"
        printf '  Status lines:\n'
        printf '%s' "$result" | grep -aoE 'HTTP/[0-9.]+ [0-9]{3}' || true
        printf '\n'
    fi
}

# Make sure the server is running before testing.
if ! (echo >/dev/tcp/"$HOST"/"$PORT") >/dev/null 2>&1; then
    echo "ERROR: Cannot connect to $HOST:$PORT"
    echo "Start Webserv first, then run this script again."
    exit 1
fi

echo "======================================"
echo "       WEBSERV CORE HTTP TESTS"
echo "======================================"
echo

# 1. Request-line parsing and method/version errors
test_request "Malformed request line" \
    "GET  / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "400 Bad Request"

test_request "Unsupported method" \
    "BANANA / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "501 Method not Implemented"

test_request "Unsupported HTTP version" \
    "GET / HTTP/2.0\r\nHost: localhost\r\n\r\n" \
    "505 HTTP Version Not Supported"

# 2. Basic GET, query handling, missing files, and path traversal
test_request "GET root" \
    "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK"

test_request "GET with query string" \
    "GET /test?a=1&b=2 HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK"

test_request "GET missing file" \
    "GET /definitely_missing_webserv_test_847291.txt HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "404 Not Found"

test_request "Encoded directory traversal rejected" \
    "GET /%2e%2e/etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "400 Bad Request"

# 3. Header validation
test_request "Header without colon" \
    "GET / HTTP/1.1\r\nHost: localhost\r\nBrokenHeader\r\n\r\n" \
    "400 Bad Request"

test_request "Missing Host header" \
    "GET / HTTP/1.1\r\n\r\n" \
    "400 Bad Request"

test_request "Duplicate Host header" \
    "GET / HTTP/1.1\r\nHost: localhost\r\nHost: example.com\r\n\r\n" \
    "400 Bad Request"

# 4. Request-body framing
test_request "POST with Content-Length" \
    "POST /upload/core_post.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nHello" \
    "200 OK"

test_request "Invalid Content-Length" \
    "POST /upload/core_bad_length.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5abc\r\n\r\nHello" \
    "400 Bad Request"

test_request "One valid chunked body" \
    "POST /upload/core_chunk.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n" \
    "200 OK"

test_request "Invalid chunk size" \
    "POST /upload/core_bad_chunk.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\nGG\r\nHello\r\n0\r\n\r\n" \
    "400 Bad Request"

test_request "Content-Length and Transfer-Encoding conflict" \
    "POST /upload/core_cl_te.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n" \
    "400 Bad Request"

# 5. Route method restrictions and connection behavior
test_request "GET forbidden on POST-only upload location" \
    "GET /upload/core_post.txt HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "405 Method Not Allowed"

test_request "Connection: close returns a response" \
    "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n" \
    "200 OK"

test_pipeline "Two pipelined GET requests" \
    "GET / HTTP/1.1\r\nHost: localhost\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    2

test_pipeline "Connection: close stops pipelined requests" \
    "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    1

test_pipeline "HTTP-looking text stays inside POST body" \
    "POST /upload/core_body_boundary.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 14\r\n\r\nGET / HTTP/1.1" \
    1

# 6. Reject incorrect line endings
test_request "LF-only line endings rejected" \
    "GET / HTTP/1.1\nHost: localhost\n\n" \
    "400 Bad Request"

echo
echo "======================================"
echo "Passed: $passed"
echo "Failed: $failed"
echo "======================================"

if [ "$failed" -gt 0 ]; then
    exit 1
fi

exit 0
