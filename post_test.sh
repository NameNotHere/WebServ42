
#!/usr/bin/env bash

# Webserv POST test suite
# Run from the Webserv project directory.
# Defaults match the current project config.

set -u

BASE_URL="${BASE_URL:-http://127.0.0.1:2020}"
UPLOAD_DIR="${UPLOAD_DIR:-./uploads}"

PASS=0
FAIL=0
TOTAL=0

TMP_DIR="$(mktemp -d)"
TEST_ID="post_test_$$"

NEW_FILE="${TEST_ID}_new.txt"
OVERWRITE_FILE="${TEST_ID}_overwrite.txt"
EMPTY_FILE="${TEST_ID}_empty.txt"
QUERY_FILE="${TEST_ID}_query.txt"
LARGE_FILE="${TEST_ID}_large.bin"
TRAVERSAL_FILE="${TEST_ID}_traversal.txt"

# Track only files belonging to this test run.
cleanup() {
    rm -f \
        "$UPLOAD_DIR/$NEW_FILE" \
        "$UPLOAD_DIR/$OVERWRITE_FILE" \
        "$UPLOAD_DIR/$EMPTY_FILE" \
        "$UPLOAD_DIR/$QUERY_FILE" \
        "$UPLOAD_DIR/$LARGE_FILE" \
        "./$TRAVERSAL_FILE"

    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

line() {
    printf '%s\n' "============================================================"
}

section() {
    printf '\n'
    line
    printf '  %s\n' "$1"
    line
}

# Prepare request bodies.
printf '%s' 'Hello from POST!' > "$TMP_DIR/new_body"
printf '%s' 'ORIGINAL CONTENT' > "$TMP_DIR/original_body"
printf '%s' 'REPLACED CONTENT' > "$TMP_DIR/replacement_body"
printf '%s' 'Query string test' > "$TMP_DIR/query_body"
: > "$TMP_DIR/empty_body"

# A deterministic 256 KiB binary body.
head -c 262144 /dev/zero > "$TMP_DIR/large_body"

HTTP_STATUS=""
HTTP_BODY=""
HTTP_ERROR=""

request_post() {
    local url="$1"
    local body_file="$2"
    local path_mode="${3:-normal}"

    : > "$TMP_DIR/response_body"
    : > "$TMP_DIR/response_headers"
    : > "$TMP_DIR/curl_error"
    : > "$TMP_DIR/status"

    local path_flag=()

    # Preserve ../ in the URL for the traversal test.
    if [[ "$path_mode" == "path-as-is" ]]; then
        path_flag=(--path-as-is)
    fi

    curl -sS --max-time 10 \
        "${path_flag[@]}" \
        -D "$TMP_DIR/response_headers" \
        -o "$TMP_DIR/response_body" \
        -w '%{http_code}' \
        -X POST \
        -H 'Content-Type: application/octet-stream' \
        --data-binary "@$body_file" \
        "$url" \
        > "$TMP_DIR/status" 2> "$TMP_DIR/curl_error" || true

    HTTP_STATUS="$(cat "$TMP_DIR/status")"
    HTTP_BODY="$(cat "$TMP_DIR/response_body")"
    HTTP_ERROR="$(cat "$TMP_DIR/curl_error")"

    [[ -n "$HTTP_STATUS" ]] || HTTP_STATUS="000"
}

run_test() {
    local name="$1"
    local url_path="$2"
    local expected_status="$3"
    local body_file="$4"
    local failure_reason="$5"
    local file_to_check="${6:-}"
    local expected_file_body="${7:-}"
    local path_mode="${8:-normal}"

    TOTAL=$((TOTAL + 1))

    printf '\n'
    line
    printf '  TEST %02d: %s\n' "$TOTAL" "$name"
    line
    printf '  REQUEST : POST %s%s\n' "$BASE_URL" "$url_path"

    if [[ "$body_file" == "$TMP_DIR/large_body" ]]; then
        printf '  BODY    : 262144 bytes (256 KiB binary data)\n'
    elif [[ ! -s "$body_file" ]]; then
        printf '  BODY    : [empty]\n'
    else
        printf '  BODY    : '
        head -c 100 "$body_file"
        printf '\n'
    fi

    printf '  EXPECTED: HTTP %s\n' "$expected_status"

    request_post "$BASE_URL$url_path" "$body_file" "$path_mode"

    printf '\n  RESPONSE: HTTP %s\n' "$HTTP_STATUS"

    if [[ -n "$HTTP_BODY" ]]; then
        printf '  BODY    : '
        printf '%s' "$HTTP_BODY" | head -c 300
        printf '\n'
        if (( ${#HTTP_BODY} > 300 )); then
            printf '            [response body truncated]\n'
        fi
    else
        printf '  BODY    : [empty]\n'
    fi

    local ok=1
    local reason="$failure_reason"

    if [[ "$HTTP_STATUS" != "$expected_status" ]]; then
        ok=0

        if [[ "$HTTP_STATUS" == "000" ]]; then
            reason="No HTTP response. Check whether Webserv is running and listening on the configured port."
        elif [[ "$HTTP_STATUS" == "500" ]]; then
            reason="Server error. Check file opening/writing, directory permissions, and the Webserv terminal log."
        elif [[ "$HTTP_STATUS" == "405" ]]; then
            reason="POST was rejected. Check location matching and allowed_methods."
        elif [[ "$HTTP_STATUS" == "404" ]]; then
            reason="Target was not found or the upload path could not be opened. Check routing and location.root."
        elif [[ "$HTTP_STATUS" == "400" ]]; then
            reason="Request was rejected as invalid. Check target validation and HTTP body parsing."
        else
            reason="Expected HTTP $expected_status but received HTTP $HTTP_STATUS. $failure_reason"
        fi
    fi

    # Optional filesystem verification.
    if [[ -n "$file_to_check" ]]; then
        if [[ ! -f "$file_to_check" ]]; then
            ok=0
            reason="HTTP status may be correct, but the expected file was not created at: $file_to_check"
        elif [[ -n "$expected_file_body" ]]; then
            if ! cmp -s "$expected_file_body" "$file_to_check"; then
                ok=0
                reason="The file exists, but its contents do not match the submitted POST body."
            fi
        fi
    fi

    if [[ "$ok" -eq 1 ]]; then
        printf '\n  RESULT  : PASS\n'
        PASS=$((PASS + 1))
    else
        printf '\n  RESULT  : FAIL\n'
        printf '  REASON  : %s\n' "$reason"

        if [[ "$HTTP_STATUS" == "000" && -n "$HTTP_ERROR" ]]; then
            printf '  DETAIL  : %s\n' "$HTTP_ERROR"
        fi

        FAIL=$((FAIL + 1))
    fi
}

# ------------------------------------------------------------
# Pre-flight
# ------------------------------------------------------------

section "WEBSERV POST TEST SUITE"

printf '  Server      : %s\n' "$BASE_URL"
printf '  Upload root : %s\n' "$UPLOAD_DIR"
printf '  Test ID     : %s\n' "$TEST_ID"

if ! command -v curl >/dev/null 2>&1; then
    printf '\nERROR: curl is not installed.\n'
    exit 1
fi

if ! command -v cmp >/dev/null 2>&1; then
    printf '\nERROR: cmp is not installed.\n'
    exit 1
fi

if [[ ! -d "$UPLOAD_DIR" ]]; then
    printf '\nERROR: Upload directory "%s" does not exist.\n' "$UPLOAD_DIR"
    printf 'Create it with: mkdir -p "%s"\n' "$UPLOAD_DIR"
    exit 1
fi

# ------------------------------------------------------------
# TEST 1: Create a new file
# ------------------------------------------------------------

run_test \
    "Create a new file" \
    "/upload/$NEW_FILE" \
    "201" \
    "$TMP_DIR/new_body" \
    "A new upload should return 201 Created." \
    "$UPLOAD_DIR/$NEW_FILE" \
    "$TMP_DIR/new_body"

# ------------------------------------------------------------
# TEST 2: Replace an existing file
# ------------------------------------------------------------

run_test \
    "Replace an existing file" \
    "/upload/$OVERWRITE_FILE" \
    "201" \
    "$TMP_DIR/original_body" \
    "The first request should create the file with HTTP 201." \
    "$UPLOAD_DIR/$OVERWRITE_FILE" \
    "$TMP_DIR/original_body"

run_test \
    "Overwrite an existing file" \
    "/upload/$OVERWRITE_FILE" \
    "200" \
    "$TMP_DIR/replacement_body" \
    "Replacing an existing file should return HTTP 200." \
    "$UPLOAD_DIR/$OVERWRITE_FILE" \
    "$TMP_DIR/replacement_body"

# ------------------------------------------------------------
# TEST 3: Empty body
# ------------------------------------------------------------

run_test \
    "Create a file with an empty body" \
    "/upload/$EMPTY_FILE" \
    "201" \
    "$TMP_DIR/empty_body" \
    "An empty body should still create a zero-byte file." \
    "$UPLOAD_DIR/$EMPTY_FILE" \
    "$TMP_DIR/empty_body"

# ------------------------------------------------------------
# TEST 4: Query string
# ------------------------------------------------------------

run_test \
    "POST with a query string" \
    "/upload/$QUERY_FILE?source=test&number=123" \
    "201" \
    "$TMP_DIR/query_body" \
    "The query string should not become part of the saved filename." \
    "$UPLOAD_DIR/$QUERY_FILE" \
    "$TMP_DIR/query_body"

# ------------------------------------------------------------
# TEST 5: Missing filename
# ------------------------------------------------------------

run_test \
    "POST to /upload without a filename" \
    "/upload" \
    "400" \
    "$TMP_DIR/new_body" \
    "A request without an upload filename should be rejected."

# ------------------------------------------------------------
# TEST 6: POST to a location that does not allow POST
# ------------------------------------------------------------

run_test \
    "POST to the root location" \
    "/" \
    "405" \
    "$TMP_DIR/new_body" \
    "The root location should reject POST if its configuration does not allow it."

# ------------------------------------------------------------
# TEST 7: Path traversal
# ------------------------------------------------------------

run_test \
    "Reject a path traversal attempt" \
    "/upload/../$TRAVERSAL_FILE" \
    "400" \
    "$TMP_DIR/new_body" \
    "The server should reject a filename containing directory traversal." \
    "" \
    "" \
    "path-as-is"

# Also make sure the traversal target did not create a file
# one directory above ./uploads.
if [[ -e "./$TRAVERSAL_FILE" ]]; then
    printf '\n  EXTRA CHECK FAIL: Traversal file exists outside uploads.\n'
    printf '  PATH: ./%s\n' "$TRAVERSAL_FILE"
    FAIL=$((FAIL + 1))
else
    printf '\n  EXTRA CHECK PASS: No traversal file created outside uploads.\n'
fi

# ------------------------------------------------------------
# TEST 8: Larger body
# ------------------------------------------------------------

run_test \
    "Upload a 256 KiB body" \
    "/upload/$LARGE_FILE" \
    "201" \
    "$TMP_DIR/large_body" \
    "The server should accept the complete body and save all 262144 bytes." \
    "$UPLOAD_DIR/$LARGE_FILE" \
    "$TMP_DIR/large_body"

# ------------------------------------------------------------
# SUMMARY
# ------------------------------------------------------------

section "FINAL SUMMARY"

printf '  Total tests : %d\n' "$TOTAL"
printf '  Passed      : %d\n' "$PASS"
printf '  Failed      : %d\n' "$FAIL"

if [[ "$FAIL" -eq 0 ]]; then
    printf '\n  ALL POST TESTS PASSED\n'
else
    printf '\n  POST TESTS FAILED: review each REASON above.\n'
fi

line

[[ "$FAIL" -eq 0 ]]
