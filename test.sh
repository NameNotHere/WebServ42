#!/bin/bash

HOST="localhost"
PORT="2020"

run_test()
{
    echo
    echo "===== $1 ====="
	printf "%b" "$2" | nc -q 0 "$HOST" "$PORT"    echo
}

run_test "1. Valid chunked request" \
'POST / HTTP/1.1\r\nHost: localhost:2020\r\nTransfer-Encoding: chunked\r\n\r\n4\r\ntest\r\n0\r\n\r\n'

run_test "2. Valid hexadecimal chunk size" \
'POST / HTTP/1.1\r\nHost: localhost:2020\r\nTransfer-Encoding: chunked\r\n\r\nA\r\n1234567890\r\n0\r\n\r\n'

run_test "3. Invalid hex character" \
'POST / HTTP/1.1\r\nHost: localhost:2020\r\nTransfer-Encoding: chunked\r\n\r\nG\r\ntest\r\n0\r\n\r\n'

run_test "4. Incomplete chunk" \
'POST / HTTP/1.1\r\nHost: localhost:2020\r\nTransfer-Encoding: chunked\r\n\r\n6\r\ntest'

run_test "5. Missing final CRLF" \
'POST / HTTP/1.1\r\nHost: localhost:2020\r\nTransfer-Encoding: chunked\r\n\r\n4\r\ntest\r\n0\r\n'

run_test "6. Data after zero chunk" \
'POST / HTTP/1.1\r\nHost: localhost:2020\r\nTransfer-Encoding: chunked\r\n\r\n4\r\ntest\r\n0\r\nhello\r\n'