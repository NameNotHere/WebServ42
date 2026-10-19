# `test_http_parser.sh`

#!/bin/bash

# ============================================================
# 42 Webserv - HTTP Parser Test Suite
# ============================================================
# Usage:
#   chmod +x test_http_parser.sh
#   ./test_http_parser.sh
#   ./test_http_parser.sh 8080
#
# Assumptions:
#   - Run from your Webserv project root.
#   - Server listens on 127.0.0.1.
#   - Server sends HTTP status lines for parser errors.
#   - Unknown/unimplemented methods return 501.
#   - Unsupported HTTP versions return 505.
# ============================================================

HOST="127.0.0.1"
PORT="${1:-2020}"
TIMEOUT=3

PASS=0
FAIL=0
TOTAL=0

TMP_DIR="./.http_parser_test_tmp_$$"
FIXTURE="__http_parser_test_$$.txt"
FIXTURE_PATH="./www/$FIXTURE"

mkdir -p "$TMP_DIR"

# ------------------------------------------------------------
# Cleanup
# ------------------------------------------------------------

cleanup()
{
    rm -f "$FIXTURE_PATH"
    rm -rf "$TMP_DIR"
}

trap cleanup EXIT

# ------------------------------------------------------------
# Check prerequisites
# ------------------------------------------------------------

if ! command -v nc >/dev/null 2>&1; then
    echo "ERROR: nc (netcat) is required."
    exit 1
fi

if ! command -v timeout >/dev/null 2>&1; then
    echo "ERROR: timeout is required."
    exit 1
fi

if [ ! -d "./www" ]; then
    echo "ERROR: ./www does not exist."
    echo "Run this script from your Webserv project root."
    exit 1
fi

if ! nc -z -w 1 "$HOST" "$PORT" >/dev/null 2>&1; then
    echo "ERROR: Cannot connect to $HOST:$PORT"
    echo "Start Webserv first, then run this script again."
    exit 1
fi

# Create a known file so a valid GET can be tested reliably.
printf 'HTTP parser test fixture\n' > "$FIXTURE_PATH"

# ------------------------------------------------------------
# Helpers
# ------------------------------------------------------------

show_input()
{
    local input="$1"

    # Display special characters visibly, including CR and LF.
	printf '%b' "$input" |
    	od -An -tc |
    	sed 's/^/      /'
}

run_test()
{
    local name="$1"
    local expected="$2"
    local input="$3"

    local response=""
    local actual=""
    local result="FAIL"

    TOTAL=$((TOTAL + 1))

    # Send the exact bytes in the request string.
    # -N closes the sending side after stdin ends.
    response=$(
		printf '%b' "$input" |
    		timeout "$TIMEOUT" nc -N "$HOST" "$PORT" 2>/dev/null
    )
    local nc_status=$?

    # Extract the HTTP status code from the first response line.
    actual=$(printf '%s\n' "$response" |
        sed -n '1s|^HTTP/[^ ]* \([0-9][0-9][0-9]\).*|\1|p')

    # A timeout or empty response is reported separately.
    if [ -z "$actual" ]; then
        actual="NO HTTP RESPONSE"
    fi

    if [ "$actual" = "$expected" ]; then
        result="PASS"
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
    fi

    echo
    echo "============================================================"
    echo "TEST $TOTAL: $name"
    echo "============================================================"
    echo "  EXPECTED: HTTP $expected"
    echo "  ACTUAL  : HTTP $actual"
    echo
    echo "  INPUT (visible control characters):"
    show_input "$input"

    echo
    echo "  RESPONSE:"
    if [ -n "$response" ]; then
        printf '%s\n' "$response" | sed 's/^/      /'
    else
        echo "      [empty response]"
    fi

    echo
    echo "  RESULT: $result"

    if [ "$result" = "FAIL" ]; then
        echo "  REASON: Expected HTTP $expected, received HTTP $actual."
        echo "  Netcat exit status: $nc_status"
    fi
}

# ------------------------------------------------------------
# 1. Valid request tests
# ------------------------------------------------------------

run_test \
    "Valid GET request" \
    "200" \
    "GET /$FIXTURE HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"

run_test \
    "Lowercase host header name" \
    "200" \
    "GET /$FIXTURE HTTP/1.1\r\nhost: localhost\r\nConnection: close\r\n\r\n"

run_test \
    "Mixed-case method GET" \
    "200" \
    "gEt /$FIXTURE HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"

# ------------------------------------------------------------
# 2. Request-line validation
# ------------------------------------------------------------

run_test \
    "Missing HTTP version" \
    "400" \
    "GET /\r\nHost: localhost\r\n\r\n"

run_test \
    "Missing target" \
    "400" \
    "GET  HTTP/1.1\r\nHost: localhost\r\n\r\n"

run_test \
    "Extra field in request line" \
    "400" \
    "GET / HTTP/1.1 extra\r\nHost: localhost\r\n\r\n"

run_test \
    "Invalid method characters" \
    "400" \
    "GE(T / HTTP/1.1\r\nHost: localhost\r\n\r\n"

# ------------------------------------------------------------
# 3. Method handling
# ------------------------------------------------------------

run_test \
    "Unknown FOO method" \
    "501" \
    "FOO /$FIXTURE HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"

run_test \
    "PUT method not implemented" \
    "501" \
    "PUT /$FIXTURE HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"

# ------------------------------------------------------------
# 4. HTTP version validation
# ------------------------------------------------------------

run_test \
    "Unsupported HTTP/9.9" \
    "505" \
    "GET / HTTP/9.9\r\nHost: localhost\r\n\r\n"

run_test \
    "Unsupported HTTP/2 text request line" \
    "505" \
    "GET / HTTP/2\r\nHost: localhost\r\n\r\n"

run_test \
    "Malformed HTTP version" \
    "400" \
    "GET / HTTP/banana\r\nHost: localhost\r\n\r\n"

# ------------------------------------------------------------
# 5. Header validation
# ------------------------------------------------------------

run_test \
    "Missing Host header" \
    "400" \
    "GET / HTTP/1.1\r\nConnection: close\r\n\r\n"

run_test \
    "Header without colon" \
    "400" \
    "GET / HTTP/1.1\r\nHost: localhost\r\nBrokenHeader\r\n\r\n"

run_test \
    "Empty header name" \
    "400" \
    "GET / HTTP/1.1\r\nHost: localhost\r\n: invalid\r\n\r\n"

run_test \
    "Duplicate Host with different capitalization" \
    "400" \
    "GET / HTTP/1.1\r\nHost: localhost\r\nhost: localhost\r\n\r\n"

# ------------------------------------------------------------
# 6. Line-ending validation
# ------------------------------------------------------------

run_test \
    "LF-only request" \
    "400" \
    "GET / HTTP/1.1\nHost: localhost\n\n"

run_test \
    "CR-only request" \
    "400" \
    "GET / HTTP/1.1\rHost: localhost\r\r"

# ------------------------------------------------------------
# Final summary
# ------------------------------------------------------------

echo
echo
echo "############################################################"
echo "                 HTTP PARSER TEST SUMMARY"
echo "############################################################"
echo "  Server:   $HOST:$PORT"
echo "  Total:    $TOTAL"
echo "  Passed:   $PASS"
echo "  Failed:   $FAIL"
echo "############################################################"

if [ "$FAIL" -eq 0 ]; then
    echo "ALL TESTS PASSED"
    exit 0
else
    echo "SOME TESTS FAILED"
    echo "Review the individual test results above."
    exit 1
fi
