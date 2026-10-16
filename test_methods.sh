
#!/usr/bin/env bash

# Webserv GET / POST / DELETE test runner
# Run from your Webserv project directory.

set -u

BASE_URL="${BASE_URL:-http://127.0.0.1:2020}"
WWW_DIR="${WWW_DIR:-./www}"
UPLOAD_DIR="${UPLOAD_DIR:-./uploads}"

PASS=0
FAIL=0
TOTAL=0

TMP_DIR="$(mktemp -d)"
TEST_ID="webserv_test_$$"
GET_FILE="${TEST_ID}_get.txt"
DELETE_FILE="${TEST_ID}_delete.txt"
POST_FILE="${TEST_ID}_post.txt"

GET_BODY="GET test file: Webserv is working."
POST_BODY="POST test body: upload successful."

cleanup() {
    rm -rf "$TMP_DIR"
    # Only remove the local test files created by this script.
    rm -f "$WWW_DIR/$GET_FILE" "$WWW_DIR/$DELETE_FILE"
    # Do not remove the POST upload automatically. It is useful for
    # checking the uploaded file after the test.
}
trap cleanup EXIT

# ---------- Formatting ----------

line() {
    printf '%s\n' "============================================================"
}

section() {
    printf '\n'
    line
    printf '  %s\n' "$1"
    line
}

# ---------- HTTP request helper ----------

request() {
    local method="$1"
    local url="$2"
    local data="${3:-}"
    local body_file="$TMP_DIR/body"
    local headers_file="$TMP_DIR/headers"
    local status="000"
    local curl_error=""

    : > "$body_file"
    : > "$headers_file"

    if [[ "$method" == "POST" ]]; then
        curl -sS --max-time 5 \
            -D "$headers_file" \
            -o "$body_file" \
            -w '%{http_code}' \
            -X POST \
            -H 'Content-Type: text/plain' \
            --data-binary "$data" \
            "$url" > "$TMP_DIR/status" 2>"$TMP_DIR/curl_error" || true
    else
        curl -sS --max-time 5 \
            -D "$headers_file" \
            -o "$body_file" \
            -w '%{http_code}' \
            -X "$method" \
            "$url" > "$TMP_DIR/status" 2>"$TMP_DIR/curl_error" || true
    fi

    status="$(cat "$TMP_DIR/status" 2>/dev/null || true)"
    curl_error="$(cat "$TMP_DIR/curl_error" 2>/dev/null || true)"

    [[ -n "$status" ]] || status="000"

    HTTP_STATUS="$status"
    HTTP_BODY="$(cat "$body_file")"
    HTTP_ERROR="$curl_error"
}

# ---------- Test reporting ----------

run_test() {
    local name="$1"
    local method="$2"
    local url="$3"
    local expected="$4"
    local data="${5:-}"
    local reason_if_failed="$6"
    local extra_check="${7:-}"

    TOTAL=$((TOTAL + 1))

    printf '\n'
    line
    printf '  TEST %02d: %s\n' "$TOTAL" "$name"
    line
    printf '  REQUEST : %s %s\n' "$method" "$url"

    if [[ "$method" == "POST" ]]; then
        printf '  BODY    : %s\n' "$data"
    fi

    printf '  EXPECTED: HTTP %s\n' "$expected"

    request "$method" "$url" "$data"

    printf '\n  RESPONSE: HTTP %s\n' "$HTTP_STATUS"

    if [[ -n "$HTTP_BODY" ]]; then
        printf '  BODY    : '
        printf '%s\n' "$HTTP_BODY" | head -c 500
        printf '\n'
        if (( ${#HTTP_BODY} > 500 )); then
            printf '            [body truncated for display]\n'
        fi
    else
        printf '  BODY    : [empty]\n'
    fi

    local ok=1

    if [[ "$HTTP_STATUS" != "$expected" ]]; then
        ok=0
    fi

    # Optional file/content check.
    if [[ "$extra_check" == "GET_BODY" ]] &&
       [[ "$HTTP_BODY" != "$GET_BODY" ]]; then
        ok=0
        reason_if_failed="GET returned the expected status but the file contents did not match."
    elif [[ "$extra_check" == "POST_FILE" ]] &&
         { [[ ! -f "$UPLOAD_DIR/$POST_FILE" ]] ||
           [[ "$(cat "$UPLOAD_DIR/$POST_FILE" 2>/dev/null)" != "$POST_BODY" ]]; }; then
        ok=0
        reason_if_failed="POST returned the expected status, but the uploaded file is missing or its contents are wrong. Check location routing, permissions, and the upload directory."
    elif [[ "$extra_check" == "DELETE_FILE" ]] &&
         [[ -e "$WWW_DIR/$DELETE_FILE" ]]; then
        ok=0
        reason_if_failed="DELETE returned the expected status, but the file still exists under WWW_DIR. Check the DELETE handler's filesystem path."
    fi

    if [[ "$ok" -eq 1 ]]; then
        printf '\n  RESULT  : PASS\n'
        PASS=$((PASS + 1))
    else
        printf '\n  RESULT  : FAIL\n'
        printf '  REASON  : %s\n' "$reason_if_failed"

        if [[ "$HTTP_STATUS" == "000" ]]; then
            printf '  DETAIL  : %s\n' "${HTTP_ERROR:-No HTTP response.}"
            printf '  HINT    : Is Webserv running at %s?\n' "$BASE_URL"
        fi

        FAIL=$((FAIL + 1))
    fi
}

# ---------- Pre-flight checks ----------

section "WEBSERV HTTP METHOD TEST SUITE"

printf '  Server      : %s\n' "$BASE_URL"
printf '  Web root    : %s\n' "$WWW_DIR"
printf '  Upload root : %s\n' "$UPLOAD_DIR"

if ! command -v curl >/dev/null 2>&1; then
    printf '\nERROR: curl is not installed.\n'
    exit 1
fi

if [[ ! -d "$WWW_DIR" ]]; then
    printf '\nERROR: Web root "%s" does not exist.\n' "$WWW_DIR"
    printf 'Run this script from your project directory or set WWW_DIR.\n'
    exit 1
fi

if [[ ! -d "$UPLOAD_DIR" ]]; then
    printf '\nERROR: Upload root "%s" does not exist.\n' "$UPLOAD_DIR"
    printf 'Create the directory and ensure Webserv can write to it.\n'
    exit 1
fi

# Create unique files for GET and DELETE.
printf '%s' "$GET_BODY" > "$WWW_DIR/$GET_FILE"
printf 'This file should be deleted by the DELETE test.\n' \
    > "$WWW_DIR/$DELETE_FILE"

# ---------- GET test ----------

run_test \
    "GET an existing file" \
    "GET" \
    "$BASE_URL/$GET_FILE" \
    "200" \
    "" \
    "Expected HTTP 200. Check GET routing, root path, file lookup, and response generation." \
    "GET_BODY"

# ---------- POST test ----------

run_test \
    "POST creates an uploaded file" \
    "POST" \
    "$BASE_URL/upload/$POST_FILE" \
    "201" \
    "$POST_BODY" \
    "Expected HTTP 201. Check the /upload location, allowed_methods POST, location.root, and file-writing code." \
    "POST_FILE"

# ---------- DELETE test ----------

run_test \
    "DELETE removes an existing file" \
    "DELETE" \
    "$BASE_URL/$DELETE_FILE" \
    "204" \
    "" \
    "Expected HTTP 200. Check DELETE routing, filesystem path, permissions, and whether the handler actually removes the file." \
    "DELETE_FILE"

# ---------- Summary ----------

section "FINAL SUMMARY"

printf '  Total tests : %d\n' "$TOTAL"
printf '  Passed      : %d\n' "$PASS"
printf '  Failed      : %d\n' "$FAIL"

if [[ "$FAIL" -eq 0 ]]; then
    printf '\n  ALL TESTS PASSED\n'
else
    printf '\n  SOME TESTS FAILED. Read each REASON above.\n'
fi

line

[[ "$FAIL" -eq 0 ]]
