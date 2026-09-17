#!/bin/bash

HOST="localhost"
PORT="2020"

pass=0
fail=0

run_test()
{
    name="$1"
    request="$2"
    expected="$3"

    echo "========================================"
    echo "TEST: $name"
    echo "========================================"

    response=$(printf "$request" | nc -w 2 "$HOST" "$PORT" 2>/dev/null)

    echo "$response"
    echo

    if echo "$response" | grep -q "$expected"; then
        echo "✅ PASS"
        ((pass++))
    else
        echo "❌ FAIL"
        echo "Expected: $expected"
        ((fail++))
    fi

    echo
}

# ============================================================
# VALID REQUESTS
# ============================================================

run_test \
"Basic GET" \
'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n' \
"200 OK"

run_test \
"GET with query string" \
'GET /index.html?hello=world HTTP/1.1\r\nHost: localhost\r\n\r\n' \
"200 OK"


# ============================================================
# REQUEST LINE ERRORS
# ============================================================

run_test \
"Unsupported method" \
'PUT / HTTP/1.1\r\nHost: localhost\r\n\r\n' \
"405 Method Not Allowed"

run_test \
"Unsupported HTTP version" \
'GET / HTTP/2.0\r\nHost: localhost\r\n\r\n' \
"505 HTTP Version Not Supported"

run_test \
"Malformed request line" \
'GET garbage garbage\r\nHost: localhost\r\n\r\n' \
"400 Bad Request"

run_test \
"Completely garbage request" \
'this is not HTTP\r\n\r\n' \
"400 Bad Request"


# ============================================================
# HEADER ERRORS
# ============================================================

run_test \
"Missing Host" \
'GET / HTTP/1.1\r\n\r\n' \
"400 Bad Request"

run_test \
"Malformed Host header" \
'GET / HTTP/1.1\r\nHost localhost\r\n\r\n' \
"400 Bad Request"

run_test \
"Empty Host" \
'GET / HTTP/1.1\r\nHost:\r\n\r\n' \
"400 Bad Request"

run_test \
"Duplicate Host" \
'GET / HTTP/1.1\r\nHost: localhost\r\nHost: example.com\r\n\r\n' \
"400 Bad Request"

run_test \
"Invalid Content-Length" \
'POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: abc\r\n\r\nhello' \
"400 Bad Request"

run_test \
"Negative Content-Length" \
'POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: -5\r\n\r\n' \
"400 Bad Request"

run_test \
"Invalid Transfer-Encoding" \
'POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: bananas\r\n\r\n' \
"400 Bad Request"


# ============================================================
# CONTENT-LENGTH BODY TESTS
# ============================================================

run_test \
"POST with complete body" \
'POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello' \
"200 OK"

run_test \
"POST empty body" \
'POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n' \
"200 OK"

run_test \
"POST body shorter than Content-Length" \
'POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10\r\n\r\nhello' \
""

# ============================================================
# CHUNKED TESTS
# ============================================================

run_test \
"Chunked body" \
'POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n4\r\ntest\r\n0\r\n\r\n' \
"200 OK"

run_test \
"Chunked multiple chunks" \
'POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n3\r\ndef\r\n0\r\n\r\n' \
"200 OK"

run_test \
"Chunked empty body" \
'POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n' \
"200 OK"

run_test \
"Invalid chunk size" \
'POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\nG\r\nhello\r\n0\r\n\r\n' \
"400 Bad Request"

run_test \
"Chunk missing CRLF" \
'POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n4\r\ntest0\r\n\r\n' \
"400 Bad Request"


# ============================================================
# HEADER CASE TESTS
# ============================================================

run_test \
"Lowercase host" \
'GET / HTTP/1.1\r\nhost: localhost\r\n\r\n' \
"200 OK"

run_test \
"Mixed case Host" \
'GET / HTTP/1.1\r\nHoSt: localhost\r\n\r\n' \
"200 OK"

run_test \
"Mixed case Transfer-Encoding" \
'POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: ChUnKeD\r\n\r\n4\r\ntest\r\n0\r\n\r\n' \
"200 OK"


# ============================================================
# PIPELINING / MULTIPLE REQUESTS
# ============================================================

echo "========================================"
echo "TEST: Two requests in one connection"
echo "========================================"

{
    printf 'GET /one HTTP/1.1\r\nHost: localhost\r\n\r\n'
    printf 'GET /two HTTP/1.1\r\nHost: localhost\r\n\r\n'
} | nc -w 2 "$HOST" "$PORT"

echo
echo "========================================"
echo "SUMMARY"
echo "========================================"
echo "Passed: $pass"
echo "Failed: $fail"
echo

if [ "$fail" -eq 0 ]; then
    echo "🎉 ALL TESTS PASSED"
else
    echo "⚠️ SOME TESTS FAILED"
fi