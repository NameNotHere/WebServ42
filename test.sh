#!/bin/bash

HOST="localhost"
PORT="2020"

echo "===== 400: Bad Request ====="
printf 'GET / HTTP/1.1\r\nHost\r\n\r\n' | nc $HOST $PORT
echo

echo "===== 405: Method Not Allowed ====="
printf 'PUT / HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc $HOST $PORT
echo

echo "===== 505: HTTP Version Not Supported ====="
printf 'GET / HTTP/2.0\r\nHost: localhost\r\n\r\n' | nc $HOST $PORT
echo

echo "===== 400: Missing Host ====="
printf 'GET / HTTP/1.1\r\n\r\n' | nc $HOST $PORT
echo

echo "===== 400: Invalid Content-Length ====="
printf 'POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: abc\r\n\r\nhello' | nc $HOST $PORT
echo

echo "===== 400: Invalid Chunked Body ====="
printf 'POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\nG\r\nhello\r\n0\r\n\r\n' | nc $HOST $PORT
echo

echo "===== 400: Invalid Header ====="
printf 'GET / HTTP/1.1\r\nHost localhost:2020\r\n\r\n' | nc $HOST $PORT
echo