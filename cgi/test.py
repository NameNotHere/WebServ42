import os, sys
import time

length = int(os.environ.get("CONTENT_LENGTH", "0") or "0")
body = sys.stdin.read(length) if length > 0 else ""

query = os.environ.get("QUERY_STRING", "")
if query.startswith("sleep="):
	time.sleep(int(query[6:]))

print("Content-Type: text/plain")
print()
print("Hello from CGI!")
print("Method:", os.environ.get("REQUEST_METHOD", "?"))
print("Query :", os.environ.get("QUERY_STRING", "?"))
print("Body  :", body)