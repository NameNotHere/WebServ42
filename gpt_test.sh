#!/bin/bash

HOST="${HOST:-localhost}"
PORT="${PORT:-2020}"

echo "======================================"
echo " Webserv: Two GET Requests Test"
echo "======================================"
echo "Target: $HOST:$PORT"
echo

REQUEST=$'GET / HTTP/1.1\r\nHost: localhost\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\n\r\n'

echo "[Sending two GET requests on one connection]"
echo

printf '%s' "$REQUEST" | nc -w 3 "$HOST" "$PORT" > /tmp/webserv_two_get_response.txt

echo "[Response status lines]"
grep -aE '^HTTP/1\.[01] [0-9]{3}' /tmp/webserv_two_get_response.txt

echo
echo "[Number of HTTP responses received]"
grep -acE '^HTTP/1\.[01] [0-9]{3}' /tmp/webserv_two_get_response.txt

echo
echo "[Full response]"
cat -v /tmp/webserv_two_get_response.txt

echo
echo "======================================"
echo "Check the Webserv terminal for:"
echo "  1. Processing method: GET (expected twice)"
echo "  2. Remaining request buffer after erase"
echo "======================================"