#!/usr/bin/env bash
# Intense Webserv integration checks. Run from any directory while Webserv is running.
set -uo pipefail

BASE_URL="${BASE_URL:-http://127.0.0.1:2020}"
AUTHORITY="${BASE_URL#*://}"; AUTHORITY="${AUTHORITY%%/*}"
if [[ "$AUTHORITY" == *:* ]]; then HOST="${AUTHORITY%:*}"; PORT="${AUTHORITY##*:}"; else HOST="$AUTHORITY"; PORT=80; fi
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WWW_DIR="${WWW_DIR:-$ROOT_DIR/www}"
UPLOAD_DIR="${UPLOAD_DIR:-$ROOT_DIR/uploads}"
TMP="$(mktemp -d)"
ID="webserv_intense_$$"
PASS=0; FAIL=0; SKIP=0; TOTAL=0

if [[ -t 1 && -z "${NO_COLOR:-}" ]]; then
  RED=$'\033[31m'; GREEN=$'\033[32m'; CYAN=$'\033[36m'; MAGENTA=$'\033[35m'; YELLOW=$'\033[33m'; BOLD=$'\033[1m'; RESET=$'\033[0m'
else
  RED=""; GREEN=""; CYAN=""; MAGENTA=""; YELLOW=""; BOLD=""; RESET=""
fi

cleanup() {
  rm -f "$WWW_DIR/$ID-get.txt" "$WWW_DIR/$ID-delete.txt" \
        "$UPLOAD_DIR/$ID-post.txt" "$UPLOAD_DIR/$ID-empty.txt" \
        "$UPLOAD_DIR/$ID-overwrite.txt" "$UPLOAD_DIR/$ID-chunked.txt" \
        "$UPLOAD_DIR/$ID-query.txt" "$UPLOAD_DIR/$ID-multichunk.txt" \
        "$WWW_DIR/$ID-space name.txt" \
        "$UPLOAD_DIR/$ID-binary.bin"
  rm -rf "$TMP"
}
trap cleanup EXIT INT TERM

line() { printf '%s%s%s\n' "$BOLD$CYAN" '============================================================' "$RESET"; }

# Raw HTTP over a short-lived TCP connection. Python is used only for reliable
# socket timeouts and byte-preserving sends; test orchestration/output is Bash.
cat > "$TMP/client.py" <<'PY'
import socket, sys
host, port = sys.argv[1], int(sys.argv[2])
data = sys.stdin.buffer.read()
try:
    sock = socket.create_connection((host, port), timeout=1.5)
    sock.settimeout(1.5)
    sock.sendall(data)
    response = bytearray()
    while len(response) < 65536:
        try: block = sock.recv(4096)
        except socket.timeout: break
        if not block: break
        response.extend(block)
    sys.stdout.buffer.write(response)
    sock.close()
except Exception as exc:
    print(f'CONNECTION ERROR: {exc}', file=sys.stderr)
PY

ACTUAL_STATUS=000; ACTUAL_RESPONSE=''; DETAIL=''
run_test() {
  local name="$1" expected="$2" request="$3" reason="$4" body_expected="${5:-}"
  local input_hex
  TOTAL=$((TOTAL + 1))
  printf '\n'; line
  printf '  %sTEST %02d: %s%s\n' "$BOLD$MAGENTA" "$TOTAL" "$name" "$RESET"
  line
  printf '  EXPECTED: %sHTTP %s%s\n' "$YELLOW" "$expected" "$RESET"
  if [[ -n "$body_expected" ]]; then printf '  EXPECTED BODY: %scontains "%s"%s\n' "$YELLOW" "$body_expected" "$RESET"; fi
  printf '\n  INPUT (visible control characters):\n'
  printf '%s' "$request" | od -An -tc -w72 | sed 's/^/     /'
  : > "$TMP/response"; : > "$TMP/detail"
  printf '%s' "$request" | python3 "$TMP/client.py" "$HOST" "$PORT" > "$TMP/response" 2> "$TMP/detail" || true
  ACTUAL_RESPONSE="$(cat "$TMP/response")"
  DETAIL="$(cat "$TMP/detail")"
  ACTUAL_STATUS="$(printf '%s\n' "$ACTUAL_RESPONSE" | head -n1 | awk '{print $2}')"
  [[ -n "$ACTUAL_STATUS" ]] || ACTUAL_STATUS=000
  local status_color="$RED"; [[ "$ACTUAL_STATUS" == "$expected" ]] && status_color="$GREEN"
  printf '  ACTUAL  : %sHTTP %s%s\n' "$status_color" "$ACTUAL_STATUS" "$RESET"
  printf '\n  RESPONSE:\n'
  if [[ -n "$ACTUAL_RESPONSE" ]]; then
    printf '%s\n' "$ACTUAL_RESPONSE" | head -c 700 | sed 's/^/      /'
    [[ ${#ACTUAL_RESPONSE} -gt 700 ]] && printf '\n      [response truncated]'
    printf '\n'
  elif [[ -n "$DETAIL" ]]; then printf '      %s\n' "$DETAIL"
  else printf '      [no response]\n'; fi
  local ok=1
  [[ "$ACTUAL_STATUS" == "$expected" ]] || ok=0
  if [[ -n "$body_expected" && "$ACTUAL_RESPONSE" != *"$body_expected"* ]]; then ok=0; fi
  if (( ok )); then
    printf '\n  RESULT: %s✓ PASS%s\n' "$GREEN" "$RESET"; PASS=$((PASS + 1))
  else
    printf '\n  RESULT: %s✗ FAIL%s\n' "$RED" "$RESET"
    printf '  REASON: Expected HTTP %s, received HTTP %s. %s\n' "$expected" "$ACTUAL_STATUS" "$reason"
    [[ -n "$DETAIL" ]] && printf '  DETAIL: %s\n' "$DETAIL"
    FAIL=$((FAIL + 1))
  fi
}

run_pipeline_test() {
  local name="$1" request="$2" reason="$3"
  TOTAL=$((TOTAL + 1)); printf '\n'; line
  printf '  %sTEST %02d: %s%s\n' "$BOLD$MAGENTA" "$TOTAL" "$name" "$RESET"; line
  printf '  EXPECTED: %s2 HTTP 200 responses on one connection%s\n' "$YELLOW" "$RESET"
  printf '\n  INPUT (visible control characters):\n'
  printf '%s' "$request" | od -An -tc -w72 | sed 's/^/     /'
  printf '%s' "$request" | python3 "$TMP/client.py" "$HOST" "$PORT" > "$TMP/response" 2> "$TMP/detail" || true
  ACTUAL_RESPONSE="$(cat "$TMP/response")"; DETAIL="$(cat "$TMP/detail")"
  local count
  count="$(printf '%s\n' "$ACTUAL_RESPONSE" | grep -c '^HTTP/1.1 200' || true)"
  printf '\n  RESPONSE:\n'; if [[ -n "$ACTUAL_RESPONSE" ]]; then printf '%s\n' "$ACTUAL_RESPONSE" | head -c 700 | sed 's/^/      /'; printf '\n'; else printf '      [no response]\n'; fi
  if [[ "$count" == 2 && "$ACTUAL_RESPONSE" == *'GET fixture body'* && "$ACTUAL_RESPONSE" == *'<html'* ]]; then
    printf '\n  RESULT: %s✓ PASS%s\n' "$GREEN" "$RESET"; PASS=$((PASS + 1))
  else
    printf '\n  RESULT: %s✗ FAIL%s\n  REASON: %s\n' "$RED" "$RESET" "$reason"
    printf '  DETAIL: received %s successful status lines; expected 2.\n' "$count"
    [[ -n "$DETAIL" ]] && printf '  SOCKET: %s\n' "$DETAIL"
    FAIL=$((FAIL + 1))
  fi
}
request() {
  local method="$1" target="$2" headers="${3:-}" body="${4:-}" connection_header=$'Connection: close\r\n'
  if [[ "$headers" =~ [Cc][Oo][Nn][Nn][Ee][Cc][Tt][Ii][Oo][Nn][[:space:]]*: ]]; then
    connection_header=""
  fi
  printf -v REQUEST '%s %s HTTP/1.1\r\nHost: 127.0.0.1:%s\r\n%s%s\r\n%s' \
    "$method" "$target" "$PORT" "$headers" "$connection_header" "$body"
}
check_file() {
  local title="$1" path="$2" expected="$3"
  TOTAL=$((TOTAL + 1)); printf '\n'; line
  printf '  %sTEST %02d: %s%s\n' "$BOLD$MAGENTA" "$TOTAL" "$title" "$RESET"; line
  printf '  INPUT   : compare saved upload with submitted bytes\n'
  printf '  EXPECTED: exact file content match\n'
  if [[ -f "$path" ]] && cmp -s "$expected" "$path"; then
    printf '  ACTUAL  : file exists and bytes match\n  RESULT: %s✓ PASS%s\n' "$GREEN" "$RESET"; PASS=$((PASS + 1))
  else
    printf '  ACTUAL  : file missing or bytes differ\n  RESULT: %s✗ FAIL%s\n  REASON: Check upload path resolution, body framing, and file writing.\n' "$RED" "$RESET"; FAIL=$((FAIL + 1))
  fi
}

check_absent() {
  local title="$1" path="$2"
  TOTAL=$((TOTAL + 1)); printf '\n'; line
  printf '  %sTEST %02d: %s%s\n' "$BOLD$MAGENTA" "$TOTAL" "$title" "$RESET"; line
  printf '  INPUT   : check that %s no longer exists\n' "$path"
  printf '  EXPECTED: file absent\n'
  if [[ ! -e "$path" ]]; then
    printf '  ACTUAL  : file absent\n  RESULT: %s✓ PASS%s\n' "$GREEN" "$RESET"; PASS=$((PASS + 1))
  else
    printf '  ACTUAL  : file still exists\n  RESULT: %s✗ FAIL%s\n  REASON: A successful DELETE must remove the target file.\n' "$RED" "$RESET"; FAIL=$((FAIL + 1))
  fi
}
printf '\n%s%s╔══════════════════════════════════════════════════════════╗%s\n' "$BOLD" "$CYAN" "$RESET"
printf '%s%s║                 WEBSERV INTENSE TEST SUITE                ║%s\n' "$BOLD" "$CYAN" "$RESET"
printf '%s%s╚══════════════════════════════════════════════════════════╝%s\n' "$BOLD" "$CYAN" "$RESET"
printf '  Target: %s\n' "$BASE_URL"
for tool in python3 od; do
  if ! command -v "$tool" >/dev/null 2>&1; then printf 'Missing required command: %s\n' "$tool"; exit 2; fi
done
if ! python3 - "$HOST" "$PORT" <<'PY' >/dev/null 2>&1
import socket, sys
s=socket.create_connection((sys.argv[1],int(sys.argv[2])),timeout=1); s.close()
PY
then printf 'Cannot connect to %s. Start Webserv first.\n' "$BASE_URL"; exit 2; fi

# Parser / request-line cases
line; printf '  %sHTTP PARSER AND REQUEST VALIDATION%s\n' "$BOLD$CYAN" "$RESET"; line
request GET '/index.html' $'Connection: close\r\n' ''
run_test 'Valid HTTP/1.1 request line' 200 "$REQUEST" 'Valid requests should reach the GET handler.'
request BREW / $'Connection: close\r\n' ''
run_test 'Unknown method token' 501 "$REQUEST" 'Unknown methods should return 501.'
request HEAD / $'Connection: close\r\n' ''
run_test 'HEAD method token is not implemented' 501 "$REQUEST" 'This implementation supports GET, POST, and DELETE only.'
request GET / $'Connection: close\r\n' ''
REQUEST="${REQUEST/HTTP\/1.1/HTTP\/1.0}"
run_test 'Unsupported HTTP/1.0 version' 505 "$REQUEST" 'A syntactically valid but unsupported HTTP/1.x version should return 505.'
REQUEST="${REQUEST/HTTP\/1.0/HTTP\/2}"
run_test 'HTTP/2 text request line is malformed' 400 "$REQUEST" 'HTTP/2 uses binary framing, so this text request line is malformed.'
run_test 'Missing Host header' 400 $'GET / HTTP/1.1\r\nConnection: close\r\n\r\n' 'HTTP/1.1 requires a Host header.'
run_test 'Malformed header line' 400 $'GET / HTTP/1.1\r\nHost 127.0.0.1\r\nConnection: close\r\n\r\n' 'Header fields require a colon.'
run_test 'Duplicate Host header' 400 $'GET / HTTP/1.1\r\nHost: localhost\r\nHost: example.com\r\nConnection: close\r\n\r\n' 'Duplicate headers are rejected by this parser.'
run_test 'Invalid non-numeric Content-Length' 400 $'POST /upload/x HTTP/1.1\r\nHost: localhost\r\nContent-Length: abc\r\nConnection: close\r\n\r\n' 'Content-Length must contain decimal digits.'
run_test 'Conflicting Content-Length and Transfer-Encoding' 400 $'POST /upload/x HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n' 'Conflicting body framing must be rejected.'
run_test 'Malformed chunk size' 400 $'POST /upload/x HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\nZZ\r\nhello\r\n0\r\n\r\n' 'A non-hexadecimal chunk size is invalid.'
run_test 'Invalid percent escape in target' 400 $'GET /bad%Q0 HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n' 'Invalid URI escapes should be rejected.'
run_test 'Malformed Host port' 400 $'GET / HTTP/1.1\r\nHost: localhost:abc\r\nConnection: close\r\n\r\n' 'A Host port must contain only digits.'
run_test 'Empty Content-Length' 400 $'POST /upload/x HTTP/1.1\r\nHost: localhost\r\nContent-Length:\r\nConnection: close\r\n\r\n' 'Content-Length cannot be empty.'
run_test 'Duplicate Content-Length headers' 400 $'POST /upload/x HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nContent-Length: 0\r\nConnection: close\r\n\r\n' 'Duplicate Content-Length headers are rejected.'
run_test 'Unsupported transfer encoding' 400 $'POST /upload/x HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip\r\nConnection: close\r\n\r\n' 'This server accepts only chunked transfer encoding.'
run_test 'Malformed chunk terminator' 400 $'POST /upload/x HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n5\r\nhelloX\n0\r\n\r\n' 'Each chunk must end with CRLF.'

# GET cases
line; printf '  %sGET%s\n' "$BOLD$CYAN" "$RESET"; line
printf 'GET fixture body\n' > "$WWW_DIR/$ID-get.txt"
request GET "/$ID-get.txt" $'Connection: close\r\n'
run_test 'GET existing text file' 200 "$REQUEST" 'A regular file under the document root should return 200.' 'GET fixture body'
request GET "/$ID-get.txt?mode=query" $'Connection: close\r\n'
run_test 'GET query string does not affect file lookup' 200 "$REQUEST" 'The query component should not become part of the filesystem path.' 'GET fixture body'
request GET "/$ID-get.txt" $'Connection: keep-alive\r\n'
FIRST_PIPELINED_REQUEST="$REQUEST"
request GET '/index.html' $'Connection: close\r\n'
run_pipeline_test 'Two pipelined GET requests' "$FIRST_PIPELINED_REQUEST$REQUEST" 'The server should process both requests in order and close after the second response.'
request GET "/$ID-missing.txt" $'Connection: close\r\n'
run_test 'GET missing file' 404 "$REQUEST" 'A missing file should return 404.'
request GET '/../etc/passwd' $'Connection: close\r\n'
run_test 'GET path traversal' 400 "$REQUEST" 'Traversal outside the configured root must not be served.'
request GET '/%2fetc%2fpasswd' $'Connection: close\r\n'
run_test 'GET encoded slash traversal attempt' 400 "$REQUEST" 'Encoded path separators should not bypass target validation.'
request GET '/%00' $'Connection: close\r\n'
run_test 'GET encoded NUL byte' 400 "$REQUEST" 'Decoded NUL bytes must be rejected.'
printf -v MIXED_METHOD_REQUEST 'gEt /index.html HTTP/1.1\r\nHost: 127.0.0.1:%s\r\nConnection: close\r\n\r\n' "$PORT"
run_test 'Mixed-case GET method token' 200 "$MIXED_METHOD_REQUEST" 'HTTP method matching should be case-insensitive.'
printf 'encoded filename body\n' > "$WWW_DIR/$ID-space name.txt"
request GET "/$ID-space%20name.txt" $'Connection: close\r\n'
run_test 'GET percent-encoded space in filename' 200 "$REQUEST" 'A valid percent-encoded space should resolve to the existing file.' 'encoded filename body'

# POST cases
line; printf '  %sPOST%s\n' "$BOLD$CYAN" "$RESET"; line
printf 'Hello from intense suite!\n' > "$TMP/post-body"
request POST '/' $'Content-Length: 26\r\n' $'Hello from intense suite!\n'
run_test 'POST to root is disallowed' 405 "$REQUEST" 'Root location has no POST permission.'
request POST "/upload/$ID-post.txt" $'Content-Type: text/plain\r\nContent-Length: 26\r\n' $'Hello from intense suite!\n'
run_test 'POST creates a new upload' 201 "$REQUEST" 'Allowed POST with a filename should create a file.'
check_file 'POST upload bytes are preserved' "$UPLOAD_DIR/$ID-post.txt" "$TMP/post-body"
: > "$TMP/empty-body"
request POST "/upload/$ID-empty.txt" $'Content-Length: 0\r\n'
run_test 'POST zero-length body' 201 "$REQUEST" 'A zero-length body with a valid filename should create an empty file.'
check_file 'POST empty upload is empty' "$UPLOAD_DIR/$ID-empty.txt" "$TMP/empty-body"
printf 'replacement bytes\n' > "$TMP/replace-body"
request POST "/upload/$ID-overwrite.txt" $'Content-Length: 8\r\n' 'old data'
run_test 'POST first write before overwrite' 201 "$REQUEST" 'First upload creates the file.'
request POST "/upload/$ID-overwrite.txt" $'Content-Length: 18\r\n' $'replacement bytes\n'
run_test 'POST overwrites an existing upload' 200 "$REQUEST" 'Overwriting an existing file should return 200.'
check_file 'POST overwrite replaces prior bytes' "$UPLOAD_DIR/$ID-overwrite.txt" "$TMP/replace-body"
request POST '/upload/' $'Content-Length: 1\r\n' 'x'
run_test 'POST location without filename' 400 "$REQUEST" 'The upload handler requires a filename after /upload/.'
request POST "/upload/../$ID-traversal.txt" $'Content-Length: 1\r\n' 'x'
run_test 'POST traversal filename' 400 "$REQUEST" 'Traversal must not create a file outside the upload directory.'
request POST "/upload/$ID-chunked.txt" $'Transfer-Encoding: chunked\r\n' $'5\r\nhello\r\n0\r\n\r\n'
run_test 'POST valid chunked body' 201 "$REQUEST" 'A valid chunked body should be decoded and saved.'
printf 'hello' > "$TMP/chunked-body"
check_file 'Chunked POST decoded bytes are preserved' "$UPLOAD_DIR/$ID-chunked.txt" "$TMP/chunked-body"
request POST "/upload/$ID-query.txt?source=suite" $'Content-Length: 5\r\n' 'query'
run_test 'POST query string is not part of upload filename' 201 "$REQUEST" 'The upload query component should not become part of the saved filename.'
printf 'query' > "$TMP/query-body"
check_file 'POST query upload bytes are preserved' "$UPLOAD_DIR/$ID-query.txt" "$TMP/query-body"
request POST "/upload/$ID/nested.txt" $'Content-Length: 1\r\n' 'x'
run_test 'POST nested upload path is rejected' 400 "$REQUEST" 'The upload handler accepts one filename, not a nested path.'
request POST "/upload/$ID-multichunk.txt" $'Transfer-Encoding: chunked\r\n' $'5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n'
run_test 'POST multiple valid chunks' 201 "$REQUEST" 'All valid chunks should be decoded and saved in order.'
printf 'hello world' > "$TMP/multichunk-body"
check_file 'Multi-chunk upload bytes are preserved' "$UPLOAD_DIR/$ID-multichunk.txt" "$TMP/multichunk-body"

# DELETE: epic.conf does not allow DELETE, so 405 is the configured expectation.
line; printf '  %sDELETE%s\n' "$BOLD$CYAN" "$RESET"; line
printf 'delete fixture\n' > "$WWW_DIR/$ID-delete.txt"
request DELETE "/$ID-delete.txt" $'Connection: close\r\n'
run_test 'DELETE removes an existing file' 204 "$REQUEST" 'With DELETE enabled, an existing writable file should be removed with 204.'
check_absent 'DELETE removes the fixture' "$WWW_DIR/$ID-delete.txt"
request DELETE "/$ID-missing.txt" $'Connection: close\r\n'
run_test 'DELETE missing file' 404 "$REQUEST" 'With DELETE enabled, a missing target should return 404.'

# CGI cases
line; printf '  %sCGI%s\n' "$BOLD$CYAN" "$RESET"; line
request GET '/cgi/test.py?name=intense' $'Connection: close\r\n'
run_test 'CGI GET executes Python script' 200 "$REQUEST" 'Check CGI routing, interpreter path, and response parsing.' 'Hello from CGI!'
request POST '/cgi/test.py?name=intense' $'Content-Type: text/plain\r\nContent-Length: 4\r\n' 'body'
run_test 'CGI POST receives method and body' 200 "$REQUEST" 'Check CGI environment variables and stdin forwarding.' 'Method: POST'
run_test 'CGI receives the submitted POST body' 200 "$REQUEST" 'The CGI script should read the complete body from stdin.' 'Body  : body'

line; printf '  %sSUMMARY%s\n' "$BOLD$CYAN" "$RESET"; line
printf '  Total: %d   %s✓ Passed: %d%s   %s✗ Failed: %d%s   %sSkipped: %d%s\n' "$TOTAL" "$GREEN" "$PASS" "$RESET" "$RED" "$FAIL" "$RESET" "$YELLOW" "$SKIP" "$RESET"
if (( FAIL )); then printf '  Review each failure reason and response above.\n'; exit 1; fi
printf '  All executed tests passed.\n'; exit 0