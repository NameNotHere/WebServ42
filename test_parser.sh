#!/bin/bash

echo "========== 1. Valid POST =========="
curl -s -v --max-time 2 -X POST http://localhost:2020/ \
  -d "hello" 2>&1

echo
echo "========== 2. DELETE =========="
curl -s -v --max-time 2 -X DELETE http://localhost:2020/ \
  2>&1

echo
echo "========== 3. Unsupported method =========="
curl -s -v --max-time 2 -X PUT http://localhost:2020/ \
  2>&1

echo
echo "========== 4. OPTIONS method =========="
curl -s -v --max-time 2 -X OPTIONS http://localhost:2020/ \
  2>&1

echo
echo "========== 5. HTTP/1.0 =========="
curl -s -v --max-time 2 --http1.0 http://localhost:2020/ \
  2>&1

echo
echo "========== 6. Invalid HTTP version =========="
curl -s -v --max-time 2 http://localhost:2020/ \
  -H "X-Test: version" 2>&1

echo
echo "========== 7. Host with valid port =========="
curl -s -v --max-time 2 http://localhost:2020/ \
  -H "Host: example.com:8080" 2>&1

echo
echo "========== 8. Host port 65535 =========="
curl -s -v --max-time 2 http://localhost:2020/ \
  -H "Host: example.com:65535" 2>&1

echo
echo "========== 9. Host port too large =========="
curl -s -v --max-time 2 http://localhost:2020/ \
  -H "Host: example.com:65536" 2>&1

echo
echo "========== 10. Host with invalid characters =========="
curl -s -v --max-time 2 http://localhost:2020/ \
  -H "Host: example!.com:2020" 2>&1

echo
echo "========== 11. Empty User-Agent =========="
curl -s -v --max-time 2 http://localhost:2020/ \
  -H "User-Agent:" 2>&1

echo
echo "========== 12. Empty Content-Length =========="
curl -s -v --max-time 2 http://localhost:2020/ \
  -H "Content-Length:" 2>&1

echo
echo "========== 13. Negative Content-Length =========="
curl -s -v --max-time 2 http://localhost:2020/ \
  -H "Content-Length: -5" 2>&1

echo
echo "========== 14. Non-numeric Content-Length =========="
curl -s -v --max-time 2 http://localhost:2020/ \
  -H "Content-Length: abc" 2>&1

echo
echo "========== 15. Transfer-Encoding chunked =========="
curl -s -v --max-time 2 http://localhost:2020/ \
  -H "Transfer-Encoding: chunked" \
  --data-binary "hello" 2>&1

echo
echo "========== 16. Invalid Transfer-Encoding =========="
curl -s -v --max-time 2 http://localhost:2020/ \
  -H "Transfer-Encoding: gzip" 2>&1

echo
echo "========== 17. Connection close =========="
curl -s -v --max-time 2 http://localhost:2020/ \
  -H "Connection: close" 2>&1

echo
echo "========== 18. Connection keep-alive =========="
curl -s -v --max-time 2 http://localhost:2020/ \
  -H "Connection: keep-alive" 2>&1

echo
echo "========== 19. Valid Expect =========="
curl -s -v --max-time 2 http://localhost:2020/ \
  -H "Expect: 100-continue" \
  -d "hello" 2>&1

echo
echo "========== 20. Invalid Expect =========="
curl -s -v --max-time 2 http://localhost:2020/ \
  -H "Expect: something-else" 2>&1

echo
echo "========== ALL TESTS FINISHED =========="