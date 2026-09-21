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

    result=$(printf '%b' "$request" | nc -w 2 "$HOST" "$PORT")

    if [ -z "$expected" ]; then
        if [ -z "$result" ]; then
            status="[PASS]"
            ((passed++))
        else
            status="[FAIL]"
            ((failed++))
        fi
    elif echo "$result" | grep -q "$expected"; then
        status="[PASS]"
        ((passed++))
    else
        status="[FAIL]"
        ((failed++))
    fi

    echo "$status $name"
    echo "  Input:"
    printf '%b' "$request" | cat -v
    echo "  Expected: ${expected:-NO RESPONSE}"
    echo "  Got:"
    if [ -n "$result" ]; then
        echo "$result" | head -n 3
    else
        echo "(nothing)"
    fi
    echo
}


echo "======================================"
echo "       WEBSERV COMPLEX EDGE TESTS"
echo "======================================"
echo


# ============================================================
# REQUEST LINE EDGE CASES
# ============================================================

test_request \
    "Spaces before method" \
    " GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "400 Bad Request"

test_request \
    "Space after method before target" \
    "GET  / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "400 Bad Request"

test_request \
    "Space before HTTP version" \
    "GET /  HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "400 Bad Request"

test_request \
    "Tab in request line" \
    "GET\t/ HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "400 Bad Request"

test_request \
    "Method containing number" \
    "G3T / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "400 Bad Request"

test_request \
    "Method containing special character" \
    "G@T / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "400 Bad Request"

test_request \
    "Unknown method" \
    "BANANA / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "400 Bad Request"


# ============================================================
# TARGET EDGE CASES
# ============================================================

test_request \
    "Root target" \
    "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK"

test_request \
    "Nested target" \
    "GET /foo/bar/baz HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK"

test_request \
    "Query only" \
    "GET /?foo=bar HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK"

test_request \
    "Multiple query parameters" \
    "GET /test?a=1&b=2&c=3 HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK"

test_request \
    "Fragment in target" \
    "GET /test#hello HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK"


# ============================================================
# HEADER WHITESPACE
# ============================================================

test_request \
    "Host with leading spaces" \
    "GET / HTTP/1.1\r\nHost:    localhost\r\n\r\n" \
    "200 OK"

test_request \
    "Host with trailing spaces" \
    "GET / HTTP/1.1\r\nHost: localhost   \r\n\r\n" \
    "200 OK"

test_request \
    "Header value with tabs" \
    "GET / HTTP/1.1\r\nHost:\tlocalhost\r\n\r\n" \
    "200 OK"

test_request \
    "Header without colon" \
    "GET / HTTP/1.1\r\nHost: localhost\r\nThisIsBroken\r\n\r\n" \
    "400 Bad Request"

test_request \
    "Header colon with no name" \
    "GET / HTTP/1.1\r\nHost: localhost\r\n: something\r\n\r\n" \
    "400 Bad Request"


# ============================================================
# HOST EDGE CASES
# ============================================================

test_request \
    "Host with port" \
    "GET / HTTP/1.1\r\nHost: localhost:2020\r\n\r\n" \
    "200 OK"

test_request \
    "Empty Host" \
    "GET / HTTP/1.1\r\nHost:\r\n\r\n" \
    "400 Bad Request"

test_request \
    "Duplicate Host" \
    "GET / HTTP/1.1\r\nHost: localhost\r\nHost: example.com\r\n\r\n" \
    "400 Bad Request"

test_request \
    "Host with invalid characters" \
    "GET / HTTP/1.1\r\nHost: local@host\r\n\r\n" \
    "400 Bad Request"


# ============================================================
# CONTENT LENGTH EDGE CASES
# ============================================================

test_request \
    "Content-Length 0" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n" \
    "200 OK"

test_request \
    "Content-Length 1" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1\r\n\r\nA" \
    "200 OK"

test_request \
    "Content-Length too large" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n\r\nHello" \
    ""

test_request \
    "Content-Length with spaces" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length:  5\r\n\r\nHello" \
    "200 OK"

test_request \
    "Content-Length decimal garbage" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5abc\r\n\r\nHello" \
    "400 Bad Request"

test_request \
    "Duplicate Content-Length" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nContent-Length: 5\r\n\r\nHello" \
    "400 Bad Request"


# ============================================================
# CHUNKED EDGE CASES
# ============================================================

test_request \
    "One chunk" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n" \
    "200 OK"

test_request \
    "Multiple chunks" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n" \
    "200 OK"

test_request \
    "Small chunks" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n1\r\nA\r\n1\r\nB\r\n1\r\nC\r\n0\r\n\r\n" \
    "200 OK"

test_request \
    "Zero length chunk" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n" \
    "200 OK"

test_request \
    "Invalid hex chunk size" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\nGG\r\nHello\r\n0\r\n\r\n" \
    "400 Bad Request"

test_request \
    "Chunk missing CRLF" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHelloX0\r\n\r\n" \
    "400 Bad Request"


# ============================================================
# TRANSFER ENCODING
# ============================================================

test_request \
    "Lowercase transfer encoding" \
    "POST / HTTP/1.1\r\nHost: localhost\r\ntransfer-encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n" \
    "200 OK"

test_request \
    "Mixed transfer encoding" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nTrAnSfEr-EnCoDiNg: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n" \
    "200 OK"

test_request \
    "Invalid transfer encoding" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: banana\r\n\r\n" \
    "400 Bad Request"


# ============================================================
# REQUEST BOUNDARIES / PIPELINING
# ============================================================

test_request \
    "Two GET requests" \
    "GET / HTTP/1.1\r\nHost: localhost\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK"

test_request \
    "GET followed by POST" \
    "GET / HTTP/1.1\r\nHost: localhost\r\n\r\nPOST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nHello" \
    "200 OK"

test_request \
    "POST followed by GET" \
    "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nHelloGET / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
    "200 OK"


# ============================================================
# CRLF EDGE CASES
# ============================================================

test_request \
    "LF only" \
    "GET / HTTP/1.1\nHost: localhost\n\n" \
    "400 Bad Request"

test_request \
    "CR only" \
    "GET / HTTP/1.1\rHost: localhost\r\r" \
    "400 Bad Request"

test_request \
    "Extra blank line" \
    "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n\r\n" \
    "200 OK"


echo "======================================"
echo "Passed: $passed"
echo "Failed: $failed"
echo "======================================"

# #!/bin/bash

# HOST="localhost"
# PORT="2020"

# passed=0
# failed=0

# test_request()
# {
#     name="$1"
#     request="$2"
#     expected="$3"

#     result=$(printf '%b' "$request" | nc -w 2 "$HOST" "$PORT")

#     if echo "$result" | grep -q "$expected"; then
#         status="[PASS]"
#         ((passed++))
#     else
#         status="[FAIL]"
#         ((failed++))
#     fi

#     echo "$status $name"
#     echo "  Input:"
#     printf '%b' "$request" | cat -v
#     echo "  Expected: $expected"
#     echo "  Got:"
#     echo "$result" | head -n 1
#     echo
# }


# echo "=============================="
# echo "      WEBSERV EDGE TESTS"
# echo "=============================="
# echo


# # ------------------------------
# # REQUEST LINE
# # ------------------------------

# test_request \
#     "Empty request" \
#     "" \
#     "400 Bad Request"

# test_request \
#     "Missing method" \
#     "/ HTTP/1.1\r\nHost: localhost\r\n\r\n" \
#     "400 Bad Request"

# test_request \
#     "Missing target" \
#     "GET HTTP/1.1\r\nHost: localhost\r\n\r\n" \
#     "400 Bad Request"

# test_request \
#     "Missing HTTP version" \
#     "GET / \r\nHost: localhost\r\n\r\n" \
#     "505 HTTP Version Not Supported"

# test_request \
#     "Garbage method" \
#     "BANANA / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
#     "400 Bad Request"

# test_request \
#     "Lowercase GET" \
#     "get / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
#     "200 OK"

# test_request \
#     "Lowercase POST" \
#     "post / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n" \
#     "200 OK"

# test_request \
#     "HEAD method" \
#     "HEAD / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
#     "405 Method Not Allowed"

# test_request \
#     "DELETE method" \
#     "DELETE / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
#     "200 OK"

# test_request \
#     "Method with numbers" \
#     "GET123 / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
#     "400 Bad Request"

# test_request \
#     "Extra spaces in request line" \
#     "GET  / HTTP/1.1\r\nHost: localhost\r\n\r\n" \
#     "400 Bad Request"

# test_request \
#     "Missing target slash" \
#     "GET hello HTTP/1.1\r\nHost: localhost\r\n\r\n" \
#     "400 Bad Request"


# # ------------------------------
# # HTTP VERSION
# # ------------------------------

# test_request \
#     "HTTP 1.0" \
#     "GET / HTTP/1.0\r\nHost: localhost\r\n\r\n" \
#     "505 HTTP Version Not Supported"

# test_request \
#     "HTTP 2.0" \
#     "GET / HTTP/2.0\r\nHost: localhost\r\n\r\n" \
#     "505 HTTP Version Not Supported"

# test_request \
#     "HTTP 1.10" \
#     "GET / HTTP/1.10\r\nHost: localhost\r\n\r\n" \
#     "505 HTTP Version Not Supported"

# test_request \
#     "Garbage HTTP version" \
#     "GET / HTTP/banana\r\nHost: localhost\r\n\r\n" \
#     "505 HTTP Version Not Supported"


# # ------------------------------
# # HOST HEADER
# # ------------------------------

# test_request \
#     "Missing Host" \
#     "GET / HTTP/1.1\r\n\r\n" \
#     "400 Bad Request"

# test_request \
#     "Empty Host" \
#     "GET / HTTP/1.1\r\nHost:\r\n\r\n" \
#     "400 Bad Request"

# test_request \
#     "Host with port" \
#     "GET / HTTP/1.1\r\nHost: localhost:2020\r\n\r\n" \
#     "200 OK"

# test_request \
#     "Lowercase host" \
#     "GET / HTTP/1.1\r\nhost: localhost\r\n\r\n" \
#     "200 OK"

# test_request \
#     "Mixed-case host" \
#     "GET / HTTP/1.1\r\nHoSt: localhost\r\n\r\n" \
#     "200 OK"

# test_request \
#     "Duplicate Host" \
#     "GET / HTTP/1.1\r\nHost: localhost\r\nHost: localhost\r\n\r\n" \
#     "400 Bad Request"


# # ------------------------------
# # HEADERS
# # ------------------------------

# test_request \
#     "Header without colon" \
#     "GET / HTTP/1.1\r\nHost: localhost\r\nBrokenHeader\r\n\r\n" \
#     "400 Bad Request"

# test_request \
#     "Empty header name" \
#     "GET / HTTP/1.1\r\nHost: localhost\r\n: hello\r\n\r\n" \
#     "400 Bad Request"

# test_request \
#     "Header with empty value" \
#     "GET / HTTP/1.1\r\nHost: localhost\r\nX-Test:\r\n\r\n" \
#     "200 OK"

# test_request \
#     "Lowercase Content-Length" \
#     "POST / HTTP/1.1\r\nHost: localhost\r\ncontent-length: 5\r\n\r\nHello" \
#     "200 OK"

# test_request \
#     "Mixed-case Content-Length" \
#     "POST / HTTP/1.1\r\nHost: localhost\r\nCoNtEnT-LeNgTh: 5\r\n\r\nHello" \
#     "200 OK"


# # ------------------------------
# # CONTENT LENGTH
# # ------------------------------

# test_request \
#     "Content-Length zero" \
#     "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n" \
#     "200 OK"

# test_request \
#     "Content-Length too large" \
#     "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n\r\nHello" \
#     ""

# test_request \
#     "Invalid Content-Length" \
#     "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: abc\r\n\r\n" \
#     "400 Bad Request"

# test_request \
#     "Negative Content-Length" \
#     "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: -1\r\n\r\n" \
#     "400 Bad Request"

# test_request \
#     "Duplicate Content-Length" \
#     "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nContent-Length: 5\r\n\r\nHello" \
#     "400 Bad Request"


# # ------------------------------
# # CHUNKED
# # ------------------------------

# test_request \
#     "Chunked body" \
#     "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n" \
#     "200 OK"

# test_request \
#     "Empty chunked body" \
#     "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n" \
#     "200 OK"

# test_request \
#     "Invalid chunk size" \
#     "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\nZZZ\r\nHello\r\n0\r\n\r\n" \
#     "400 Bad Request"

# test_request \
#     "Chunk missing CRLF" \
#     "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHelloX\r\n0\r\n\r\n" \
#     "400 Bad Request"

# test_request \
#     "Chunk size too large" \
#     "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n10\r\nHello\r\n0\r\n\r\n" \
#     ""

# test_request \
#     "Mixed-case Transfer-Encoding" \
#     "POST / HTTP/1.1\r\nHost: localhost\r\nTrAnSfEr-EnCoDiNg: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n" \
#     "200 OK"


# # ------------------------------
# # QUERY / TARGET
# # ------------------------------

# test_request \
#     "Query string" \
#     "GET /test?foo=bar&hello=world HTTP/1.1\r\nHost: localhost\r\n\r\n" \
#     "200 OK"

# test_request \
#     "Root with query" \
#     "GET /?test=123 HTTP/1.1\r\nHost: localhost\r\n\r\n" \
#     "200 OK"

# test_request \
#     "Long target" \
#     "GET /aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa HTTP/1.1\r\nHost: localhost\r\n\r\n" \
#     "200 OK"


# echo "=============================="
# echo "Passed: $passed"
# echo "Failed: $failed"
# echo "=============================="