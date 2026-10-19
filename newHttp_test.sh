
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
    local exit_code=$?

    local status_line
    status_line=$(printf '%s\n' "$result" | grep -m 1 -E '^HTTP/[0-9.]+ [0-9]{3}' || true)

    local status
    if [ "$expected" = "NO RESPONSE" ]; then
        if [ -z "$result" ]; then
            status="[PASS]"
            ((passed++))
        else
            status="[FAIL]"
            ((failed++))
        fi
    elif printf '%s\n' "$result" | grep -Fq "$expected"; then
        status="[PASS]"
        ((passed++))
    else
        status="[FAIL]"
        ((failed++))
    fi

    printf '%s %s\n' "$status" "$name"
    printf '  Expected: %s\n' "$expected"
    printf '  Got:      %s\n' "${status_line:-NO RESPONSE}"

    if [ "$status" = "[FAIL]" ]; then
        printf '  Request:\n'
        printf '%b' "$request" | cat -v
        printf '\n  Response headers:\n'
        printf '%s\n' "$result" | head -n 8
        printf '\n'
    fi
}

# Tests that send multiple requests on ONE connection.
# Checks the number of HTTP status lines returned.
test_pipeline()
{
    local name="$1"
    local request="$2"
    local expected_count="$3"

    local result
    result=$(printf '%b' "$request" | nc -w "$TIMEOUT" "$HOST" "$PORT" 2>/dev/null)

    local actual_count
    actual_count=$(printf '%s\n' "$result" |
        grep -c -E '^HTTP/[0-9.]+ [0-9]{3}' || true)

    if [ "$actual_count" -eq "$expected_count" ]; then
        printf '[PASS] %s (%s responses)\n' "$name" "$actual_count"
        ((passed++))
    else
        printf '[FAIL] %s\n' "$name"
        printf '  Expected responses: %s\n' "$expected_count"
        printf '  Actual responses:   %s\n' "$actual_count"
        printf '  Status lines:\n'
        printf '%s\n' "$result" | grep -E '^HTTP/[0-9.]+ [0-9]{3}' || true
        printf '\n'
        ((failed++))
    fi
}

# Confirm the server is reachable before running tests.
if ! (echo >/dev/tcp/"$HOST"/"$PORT") >/dev/null 2>&1; then
    echo "ERROR: Cannot connect to $HOST:$PORT"
    echo "Start Webserv first, then run this script again."
    exit 1
fi

echo "======================================"
echo "       WEBSERV HTTP EDGE TESTS"
echo "======================================"
echo

# ============================================================
# REQUEST LINE
# ============================================================

test_request "Spaces before method" \
    " GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "400 Bad Request"

test_request "Extra space before target" \
    "GET  / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "400 Bad Request"

test_request "Extra space before version" \
    "GET /  HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "400 Bad Request"

test_request "Tab in request line" \
    "GET\t/ HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "400 Bad Request"

test_request "Method containing number" \
    "G3T / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "501 Method not implemented"

test_request "Method containing special character" \
    "G@T / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "400 Bad Request"

test_request "Unknown method" \
    "BANANA / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "501 Method not implemented"

# ============================================================
# GET TARGETS
# ============================================================

test_request "Root target" \
    "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK"

test_request "Nested target" \
    "GET /foo/bar/baz.html HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK"

test_request "Root with query" \
    "GET /?foo=bar HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK"

test_request "Multiple query parameters" \
    "GET /test?a=1&b=2&c=3 HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK"

test_request "Fragment in target" \
    "GET /test#hello HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK"

# ============================================================
# HEADER FORMATTING
# ============================================================

test_request "Host with leading spaces" \
    "GET / HTTP/1.1\r\nHost:    localhost\r\n\r\n" \
    "200 OK"

test_request "Host with trailing spaces" \
    "GET / HTTP/1.1\r\nHost: localhost   \r\n\r\n" \
    "200 OK"

test_request "Host with tab whitespace" \
    "GET / HTTP/1.1\r\nHost:\tlocalhost\r\n\r\n" \
    "200 OK"

test_request "Header without colon" \
    "GET / HTTP/1.1\r\nHost: localhost\r\nBrokenHeader\r\n\r\n" \
    "400 Bad Request"

test_request "Empty header name" \
    "GET / HTTP/1.1\r\nHost: localhost\r\n: something\r\n\r\n" \
    "400 Bad Request"

test_request "Header with empty value" \
    "GET / HTTP/1.1\r\nHost: localhost\r\nX-Test:\r\n\r\n" \
    "200 OK"

# ============================================================
# HOST VALIDATION
# ============================================================

test_request "Host with port" \
    "GET / HTTP/1.1\r\nHost: localhost:2020\r\n\r\n" \
    "200 OK"

test_request "Empty Host" \
    "GET / HTTP/1.1\r\nHost:\r\n\r\n" \
    "400 Bad Request"

test_request "Missing Host" \
    "GET / HTTP/1.1\r\n\r\n" \
    "400 Bad Request"

test_request "Duplicate Host" \
    "GET / HTTP/1.1\r\nHost: localhost\r\nHost: example.com\r\n\r\n" \
    "400 Bad Request"

test_request "Host with invalid characters" \
    "GET / HTTP/1.1\r\nHost: local@host\r\n\r\n" \
    "400 Bad Request"

# ============================================================
# CONTENT-LENGTH
# Uploads target /upload, not /, because this server's POST
# handler is designed for file uploads.
# ============================================================

test_request "Content-Length zero" \
    "POST /upload/cl_zero.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n" \
    "200 OK"

test_request "Content-Length one" \
    "POST /upload/cl_one.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1\r\n\r\nA" \
    "200 OK"

test_request "Content-Length too large" \
    "POST /upload/cl_large.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n\r\nHello" \
    "NO RESPONSE"

test_request "Content-Length with whitespace" \
    "POST /upload/cl_spaces.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length:  5\r\n\r\nHello" \
    "200 OK"

test_request "Content-Length decimal garbage" \
    "POST /upload/cl_garbage.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5abc\r\n\r\nHello" \
    "400 Bad Request"

test_request "Duplicate Content-Length" \
    "POST /upload/cl_duplicate.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nContent-Length: 5\r\n\r\nHello" \
    "400 Bad Request"

# ============================================================
# CHUNKED TRANSFER CODING
# ============================================================

test_request "One chunk" \
    "POST /upload/chunk_one.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n" \
    "200 OK"

test_request "Multiple chunks" \
    "POST /upload/chunk_multi.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n" \
    "200 OK"

test_request "Small chunks" \
    "POST /upload/chunk_small.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n1\r\nA\r\n1\r\nB\r\n1\r\nC\r\n0\r\n\r\n" \
    "200 OK"

test_request "Zero length chunk" \
    "POST /upload/chunk_zero.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n" \
    "200 OK"

test_request "Invalid hexadecimal chunk size" \
    "POST /upload/chunk_invalid.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\nGG\r\nHello\r\n0\r\n\r\n" \
    "400 Bad Request"

test_request "Chunk missing CRLF" \
    "POST /upload/chunk_bad_crlf.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHelloX0\r\n\r\n" \
    "400 Bad Request"

# ============================================================
# TRANSFER-ENCODING HEADER CASE
# HTTP header names are case-insensitive.
# ============================================================

test_request "Lowercase Transfer-Encoding" \
    "POST /upload/chunk_lower.txt HTTP/1.1\r\nHost: localhost\r\ntransfer-encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n" \
    "200 OK"

test_request "Mixed-case Transfer-Encoding" \
    "POST /upload/chunk_mixed.txt HTTP/1.1\r\nHost: localhost\r\nTrAnSfEr-EnCoDiNg: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n" \
    "200 OK"

test_request "Invalid Transfer-Encoding" \
    "POST /upload/chunk_bad_encoding.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: banana\r\n\r\n" \
    "400 Bad Request"

# ============================================================
# REQUEST BOUNDARIES AND PIPELINING
# Each pipeline test verifies the number of HTTP responses.
# ============================================================

test_pipeline "Two GET requests" \
    "GET / HTTP/1.1\r\nHost: localhost\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    2

test_pipeline "GET followed by POST" \
    "GET / HTTP/1.1\r\nHost: localhost\r\n\r\nPOST /upload/pipeline_get_post.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nHello" \
    2

test_pipeline "POST followed by GET" \
    "POST /upload/pipeline_post_get.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nHelloGET / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    2

# ============================================================
# LINE ENDINGS
# ============================================================

test_request "LF only" \
    "GET / HTTP/1.1\nHost: localhost\n\n" \
    "400 Bad Request"

test_request "CR only" \
    "GET / HTTP/1.1\rHost: localhost\r\r" \
    "400 Bad Request"

test_request "Extra blank line" \
    "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n\r\n" \
    "200 OK"

# ============================================================
# SUMMARY
# ============================================================

echo
echo "======================================"
echo "Passed: $passed"
echo "Failed: $failed"
echo "======================================"

if [ "$failed" -gt 0 ]; then
    exit 1
fi

exit 0