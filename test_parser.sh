echo "========== 1. Basic GET =========="
curl -s -v http://localhost:2020/ 2>&1

echo
echo "========== 2. GET different path =========="
curl -s -v http://localhost:2020/hello/world 2>&1

echo
echo "========== 3. POST with body =========="
curl -s -v -X POST http://localhost:2020/ -d "hello" 2>&1

echo
echo "========== 4. POST with longer body =========="
curl -s -v -X POST http://localhost:2020/ -d "hello world this is a longer body" 2>&1

echo
echo "========== 5. Explicit Content-Length =========="
curl -s -v -X POST http://localhost:2020/ \
  -H "Content-Length: 10" \
  --data-raw "helloworld" 2>&1

echo
echo "========== 6. Invalid Connection =========="
curl -s -v http://localhost:2020/ \
  -H "Connection: banana" 2>&1

echo
echo "========== 7. Missing Host =========="
curl -s -v http://localhost:2020/ \
  -H "Host:" 2>&1

echo
echo "========== 8. Invalid Host =========="
curl -s -v http://localhost:2020/ \
  -H "Host: :2020" 2>&1

echo
echo "========== 9. Invalid Expect =========="
curl -s -v http://localhost:2020/ \
  -H "Expect: banana" 2>&1

echo
echo "========== 10. Valid Expect =========="
curl -s -v http://localhost:2020/ \
  -H "Expect: 100-continue" 2>&1

echo
echo "========== 11. Invalid Transfer-Encoding =========="
curl -s -v http://localhost:2020/ \
  -H "Transfer-Encoding: banana" 2>&1

echo
echo "========== 12. Duplicate header =========="
curl -s -v http://localhost:2020/ \
  -H "X-Test: one" \
  -H "X-Test: two" 2>&1

echo
echo "========== ALL TESTS FINISHED =========="