#!/usr/bin/env bash
# Fast, focused CGI integration suite for the Webserv project.
# Usage: ./cgi_intense_test.sh [host] [port]
set -u

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"
HOST="${1:-${HOST:-127.0.0.1}}"
PORT="${2:-${PORT:-2020}}"

command -v python3 >/dev/null 2>&1 || {
    printf 'Missing required command: python3\n' >&2
    exit 2
}

exec python3 - "$HOST" "$PORT" "$ROOT_DIR" <<'PY'
import os
import socket
import sys
import tempfile
import threading
import time

host, port_text, root = sys.argv[1:4]
try:
    port = int(port_text)
except ValueError:
    print("Port must be a number: " + port_text, file=sys.stderr)
    raise SystemExit(2)

TTY = sys.stdout.isatty() and not os.environ.get("NO_COLOR")
C = {
    "reset": "\033[0m" if TTY else "",
    "bold": "\033[1m" if TTY else "",
    "dim": "\033[2m" if TTY else "",
    "cyan": "\033[96m" if TTY else "",
    "blue": "\033[94m" if TTY else "",
    "green": "\033[92m" if TTY else "",
    "red": "\033[91m" if TTY else "",
    "yellow": "\033[93m" if TTY else "",
    "white": "\033[97m" if TTY else "",
}

def paint(color, value):
    return C[color] + value + C["reset"]

def bar(width=66):
    return paint("blue", "━" * width)

def clip(value, limit=520):
    value = value.decode("utf-8", "replace") if isinstance(value, bytes) else str(value)
    value = value.replace("\r", "\\r").replace("\n", "\\n\n             ")
    if len(value) > limit:
        value = value[:limit] + "… [truncated]"
    return value or "[empty]"

def visible(raw):
    return raw.decode("utf-8", "backslashreplace").replace("\r", "\\r").replace("\n", "\\n\n             ")

def request_bytes(method, target, headers=None, body=b""):
    headers = dict(headers or {})
    headers.setdefault("Host", f"{host}:{port}")
    headers.setdefault("Connection", "close")
    headers.setdefault("Content-Length", str(len(body)))
    head = f"{method} {target} HTTP/1.1\r\n" + "".join(f"{k}: {v}\r\n" for k, v in headers.items()) + "\r\n"
    return head.encode("ascii") + body

def exchange(raw, timeout=7.0):
    started = time.monotonic()
    with socket.create_connection((host, port), timeout=2.0) as sock:
        sock.settimeout(timeout)
        sock.sendall(raw)
        chunks = []
        total = 0
        while total < 20 * 1024 * 1024:
            try:
                data = sock.recv(65536)
            except socket.timeout:
                break
            if not data:
                break
            chunks.append(data)
            total += len(data)
    response = b"".join(chunks)
    status = None
    body = b""
    if response:
        first = response.split(b"\r\n", 1)[0]
        fields = first.split()
        if len(fields) > 1 and fields[1].isdigit():
            status = int(fields[1])
        split = response.find(b"\r\n\r\n")
        body = response[split + 4:] if split >= 0 else b""
    return status, response, body, time.monotonic() - started

def format_status(status):
    return f"HTTP {status}" if status is not None else "no valid HTTP response"

def run(name, raw, expected_status, body_contains=b"", why=""):
    global passed, failed
    try:
        status, response, body, elapsed = exchange(raw)
        problems = []
        if status != expected_status:
            problems.append(f"expected HTTP {expected_status}, received {format_status(status)}")
        if body_contains and body_contains not in body:
            problems.append("response body did not contain the expected CGI output marker")
        if problems:
            failed += 1
            print(f"  {paint('red', '✗')} {name}")
            print(f"    {paint('dim', 'INPUT   :')} {visible(raw)}")
            print(f"    {paint('dim', 'EXPECTED:')} HTTP {expected_status}" + (f" + {clip(body_contains, 100)!r}" if body_contains else ""))
            print(f"    {paint('dim', 'ACTUAL  :')} {format_status(status)}  ({elapsed:.3f}s)")
            print(f"    {paint('dim', 'BODY    :')} {clip(body)}")
            print(f"    {paint('yellow', 'REASON  :')} {why or '; '.join(problems)}")
        else:
            passed += 1
            print(f"  {paint('green', '✓')} {name} {paint('dim', f'· {elapsed:.3f}s')}")
        return status, response, body, elapsed
    except Exception as exc:
        failed += 1
        print(f"  {paint('red', '✗')} {name}")
        print(f"    {paint('dim', 'INPUT   :')} {visible(raw)}")
        print(f"    {paint('dim', 'EXPECTED:')} HTTP {expected_status}")
        print(f"    {paint('dim', 'ACTUAL  :')} request error: {exc}")
        print(f"    {paint('yellow', 'REASON  :')} {why or 'Could not complete the HTTP exchange.'}")
        return None, b"", b"", 0.0

passed = failed = skipped = 0

try:
    with socket.create_connection((host, port), timeout=1.5):
        pass
except OSError as exc:
    print(paint("red", f"✗ Cannot reach Webserv at {host}:{port}: {exc}"))
    print("  Start the server, then rerun this script.")
    raise SystemExit(2)

print()
print(paint("cyan", "╭──────────────────────────────────────────────────────────────────╮"))
print(paint("cyan", "│") + paint("bold", "                 WEBSERV  /  CGI INTEGRATION SUITE                 ") + paint("cyan", "│"))
print(paint("cyan", "╰──────────────────────────────────────────────────────────────────╯"))
print(f"  {paint('dim', 'Target:')} {paint('white', f'{host}:{port}')}   {paint('dim', 'Transport:')} raw HTTP sockets   {paint('dim', 'Mode:')} fast")
print(bar())

# A temporary probe CGI lets the suite verify stdin bytes and CGI environment
# variables on the configured /cgi/ route without changing the teammate's tests.
probe_path = None
probe_name = None
extra_probe_paths = []
cgi_dir = os.path.join(root, "www", "cgi")
try:
    fd, probe_path = tempfile.mkstemp(prefix=".suite_probe_", suffix=".py", dir=cgi_dir)
    probe_name = os.path.basename(probe_path)
    with os.fdopen(fd, "w", encoding="utf-8") as probe:
        probe.write(
            "import os, sys\n"
            "n = int(os.environ.get('CONTENT_LENGTH', '0') or '0')\n"
            "data = sys.stdin.buffer.read(n) if n else b''\n"
            "print('Content-Type: text/plain\\r\\n')\n"
            "print('PROBE_OK')\n"
            "print('METHOD=' + os.environ.get('REQUEST_METHOD', ''))\n"
            "print('QUERY=' + os.environ.get('QUERY_STRING', ''))\n"
            "print('BODY_HEX=' + data.hex())\n"
            "print('BODY_LEN=' + str(len(data)))\n"
            "print('X_PROBE=' + os.environ.get('HTTP_X_PROBE', ''))\n"
        )
except OSError as exc:
    print(paint("yellow", f"⚠ Could not create temporary CGI probe in www/cgi: {exc}"))
    print("  GET tests still run; POST body and environment probe tests will be skipped.")

print(paint("bold", "  CGI routing and environment"))
run("Python CGI executes on GET", request_bytes("GET", "/cgi/test.py?name=Suite"), 200, b"Hello from CGI!", "A CGI GET should run the configured Python script and return its output.")
run("QUERY_STRING is forwarded", request_bytes("GET", "/cgi/test.py?name=Suite%20Test&n=2"), 200, b"Query : name=Suite%20Test&n=2", "The query component should reach the CGI environment unchanged.")
run("REQUEST_METHOD is forwarded", request_bytes("GET", "/cgi/test.py?method=check"), 200, b"Method: GET", "CGI REQUEST_METHOD should match the HTTP request method.")
run("Empty query marker is accepted", request_bytes("GET", "/cgi/test.py?"), 200, b"Query :", "An empty query string is valid and should not prevent CGI execution.")

if probe_name:
    print(paint("bold", "  CGI input, headers, and framing"))
    path = "/cgi/" + probe_name
    body = b"Hello from CGI!"
    raw = request_bytes("POST", path, {"Content-Type": "text/plain", "X-Probe": "header-value"}, body)
    run("POST body reaches CGI stdin intact", raw, 200, b"BODY_HEX=" + body.hex().encode(), "The CGI process must receive exactly the request body on stdin.")
    run("CGI receives CONTENT_TYPE and custom headers", raw, 200, b"X_PROBE=header-value", "Content-Type and HTTP_* request headers should be exposed through CGI environment variables.")
    empty = request_bytes("POST", path, {"Content-Type": "text/plain"}, b"")
    run("Zero-length POST reaches CGI", empty, 200, b"BODY_LEN=0", "A valid Content-Length: 0 request should still be handled by the CGI route.")
    binary_body = bytes(range(256))
    run("Binary POST bytes are preserved", request_bytes("POST", path, {}, binary_body), 200, b"BODY_HEX=" + binary_body.hex().encode(), "CGI stdin is byte-oriented; embedded NUL and high-bit bytes must not be truncated.")
    large_body = b"Z" * (64 * 1024)
    run("64 KiB POST drains through nonblocking CGI pipe", request_bytes("POST", path, {}, large_body), 200, b"BODY_LEN=65536", "Large request bodies must be written asynchronously without blocking or truncation.")

    chunked = (
        f"POST {path} HTTP/1.1\r\nHost: {host}:{port}\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "5\r\nhello\r\n0\r\n\r\n"
    ).encode("ascii")
    run("Valid chunked POST is decoded for CGI", chunked, 200, b"BODY_HEX=68656c6c6f", "A valid chunked request should be decoded before its body is passed to CGI.")

    # CGI output may omit Content-Length; Webserv should frame its HTTP response
    # from the CGI body. A malformed CGI header block should fail cleanly.
    for prefix, source in (
        (".suite_no_length_", "import sys\nsys.stdout.write('Content-Type: text/plain\\n\\nCGI_NO_LENGTH_OK')\n"),
        (".suite_bad_header_", "import sys\nsys.stdout.write('Not-A-Header\\n\\nBAD')\n"),
    ):
        fd, extra_path = tempfile.mkstemp(prefix=prefix, suffix=".py", dir=cgi_dir)
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            f.write(source)
        extra_probe_paths.append(extra_path)
    no_length_name = os.path.basename(extra_probe_paths[0])
    bad_header_name = os.path.basename(extra_probe_paths[1])
    run("CGI output without Content-Length is framed correctly", request_bytes("GET", "/cgi/" + no_length_name), 200, b"CGI_NO_LENGTH_OK", "CGI output does not need to provide Content-Length; the server must produce a complete HTTP response.")
    run("Malformed CGI response headers return HTTP 500", request_bytes("GET", "/cgi/" + bad_header_name), 500, why="Invalid CGI output headers must be rejected without crashing or hanging the server.")

    malformed_cl = (f"POST {path} HTTP/1.1\r\nHost: {host}:{port}\r\nContent-Length: nope\r\nConnection: close\r\n\r\nbody").encode("ascii")
    run("Malformed Content-Length is rejected", malformed_cl, 400, why="Non-numeric Content-Length must be rejected before starting a CGI process.")
    duplicate_cl = (f"POST {path} HTTP/1.1\r\nHost: {host}:{port}\r\nContent-Length: 4\r\nContent-Length: 4\r\nConnection: close\r\n\r\ntest").encode("ascii")
    run("Duplicate Content-Length is rejected", duplicate_cl, 400, why="Ambiguous request framing should be rejected to avoid request-smuggling behavior.")
    bad_te = (f"POST {path} HTTP/1.1\r\nHost: {host}:{port}\r\nTransfer-Encoding: banana\r\nConnection: close\r\n\r\n").encode("ascii")
    run("Unknown Transfer-Encoding is rejected", bad_te, 400, why="Unsupported transfer codings must not be treated as a valid CGI body.")
else:
    skipped += 9

print(paint("bold", "  Failure handling and event-loop responsiveness"))
missing_raw = request_bytes("GET", "/cgi/.suite_missing_script.py")
run("Missing CGI script returns server error", missing_raw, 500, why="A CGI execution failure should become an HTTP 500 response.")
timeout_raw = request_bytes("GET", "/cgi/test.py?sleep=6")
run("CGI timeout is terminated and reported", timeout_raw, 500, why="The configured five-second CGI timeout should terminate this six-second script and return HTTP 500.")

# Keep the slow CGI request active while a second client asks for a normal page.
slow_result = {}
slow_started = threading.Event()
def slow_client():
    try:
        raw = request_bytes("GET", "/cgi/test.py?sleep=2")
        slow_started.set()
        slow_result["result"] = exchange(raw, timeout=5.0)
    except Exception as exc:
        slow_result["error"] = str(exc)
        slow_started.set()
thread = threading.Thread(target=slow_client, daemon=True)
thread.start()
slow_started.wait(1.0)
time.sleep(0.12)
fast_raw = request_bytes("GET", "/")
status, response, body, elapsed = run("Second client responds during a slow CGI", fast_raw, 200, why="An active CGI child must not block the main event loop from serving another client.")
thread.join(timeout=4.0)
if thread.is_alive() or slow_result.get("error") or slow_result.get("result", (None,))[0] != 200:
    failed += 1
    detail = slow_result.get("error", "slow CGI did not finish with HTTP 200")
    print(f"  {paint('red', '✗')} Slow CGI completes independently")
    print(f"    {paint('dim', 'EXPECTED:')} HTTP 200 after the CGI finishes")
    print(f"    {paint('dim', 'ACTUAL  :')} {detail if not thread.is_alive() else 'request still pending'}")
else:
    slow_elapsed = slow_result["result"][3]
    if elapsed > 1.25:
        failed += 1
        print(f"  {paint('red', '✗')} Second client is not delayed by CGI")
        print(f"    {paint('dim', 'EXPECTED:')} response under 1.25s while CGI sleeps")
        print(f"    {paint('dim', 'ACTUAL  :')} {elapsed:.3f}s")
        print(f"    {paint('yellow', 'REASON  :')} The main event loop may be waiting synchronously for CGI completion.")
    else:
        passed += 1
        print(f"  {paint('green', '✓')} Second client is not delayed by CGI {paint('dim', f'· {elapsed:.3f}s fast / {slow_elapsed:.3f}s CGI')}")

# Check persistent HTTP/1.1 behavior by issuing two CGI requests on one socket.
print(paint("bold", "  Connection behavior"))
try:
    with socket.create_connection((host, port), timeout=2.0) as sock:
        sock.settimeout(3.0)
        first = (f"GET /cgi/test.py?name=one HTTP/1.1\r\nHost: {host}:{port}\r\nConnection: keep-alive\r\n\r\n").encode()
        second = (f"GET /cgi/test.py?name=two HTTP/1.1\r\nHost: {host}:{port}\r\nConnection: close\r\n\r\n").encode()
        sock.sendall(first + second)
        chunks = []
        while True:
            data = sock.recv(65536)
            if not data:
                break
            chunks.append(data)
        data = b"".join(chunks)
        count = data.count(b"HTTP/1.1 200")
        if count == 2 and b"Query : name=one" in data and b"Query : name=two" in data:
            passed += 1
            print(f"  {paint('green', '✓')} Two CGI requests work on one keep-alive connection")
        else:
            failed += 1
            print(f"  {paint('red', '✗')} Two CGI requests work on one keep-alive connection")
            print(f"    {paint('dim', 'INPUT   :')} two pipelined GET requests, first keep-alive then close")
            print(f"    {paint('dim', 'EXPECTED:')} 2 × HTTP 200 with both query markers")
            print(f"    {paint('dim', 'ACTUAL  :')} {count} successful response(s); {clip(data)}")
            print(f"    {paint('yellow', 'REASON  :')} Persistent connections must resume parsing after each CGI response.")
except Exception as exc:
    failed += 1
    print(f"  {paint('red', '✗')} Two CGI requests work on one keep-alive connection")
    print(f"    {paint('dim', 'ACTUAL  :')} {exc}")
    print(f"    {paint('yellow', 'REASON  :')} The connection may have closed before both CGI responses were sent.")

if probe_path:
    try:
        os.unlink(probe_path)
    except OSError:
        pass
for path in extra_probe_paths:
    try:
        os.unlink(path)
    except OSError:
        pass

print(bar())
total = passed + failed + skipped
print(f"  {paint('green', f'{passed} passed')}  {paint('red', f'{failed} failed')}  {paint('yellow', f'{skipped} skipped')}  {paint('dim', f'{total} total')}")
if failed:
    print(paint("red", "  CGI suite found issues. Failure details above identify the request and expected behavior."))
    raise SystemExit(1)
print(paint("green", "  All executed CGI checks passed."))
raise SystemExit(0)
PY
