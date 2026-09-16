#!/bin/bash

HOST="localhost"
PORT="2020"

run_test()
{
    echo
    echo "===== $1 ====="
    printf "%b" "$2" | nc -q 0 "$HOST" "$PORT"
    echo
}

run_test "1. Multiple chunks" \
'POST / HTTP/1.1\r\nHost: localhost:2020\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n3\r\ndef\r\n0\r\n\r\n'

run_test "2. Empty chunked body" \
'POST / HTTP/1.1\r\nHost: localhost:2020\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n'

run_test "3. Chunk extension" \
'POST / HTTP/1.1\r\nHost: localhost:2020\r\nTransfer-Encoding: chunked\r\n\r\n0;foo=bar\r\n\r\n'

run_test "4. Chunked + second request" \
'POST /first HTTP/1.1\r\nHost: localhost:2020\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n0\r\n\r\nGET /second HTTP/1.1\r\nHost: localhost:2020\r\n\r\n'