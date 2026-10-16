
#!/usr/bin/env bash

# 42 Webserv: GET / POST / DELETE test suite
# Defaults: localhost:2020, ./www, ./uploads
# Run from the Webserv project directory.

set -u

BASE_URL="${BASE_URL:-http://127.0.0.1:2020}"
WWW_DIR="${WWW_DIR:-./www}"
UPLOAD_DIR="${UPLOAD_DIR:-./uploads}"

HOST="127.0.0.1"
PORT="${BASE_URL##*:}"
PORT="${PORT%%/*}"

TMP="$(mktemp -d)"
ID="webserv_$$"

PASS=0
FAIL=0
TOTAL=0
SKIP=0

GET_FILE="${ID}_get.txt"
SPACE_FILE="${ID}_space name.txt"
POST_FILE="${ID}_post.txt"
OVERWRITE_FILE="${ID}_overwrite.txt"
EMPTY_FILE="${ID}_empty.txt"
BINARY_FILE="${ID}_binary.bin"
LARGE_FILE="${ID}_large.bin"
QUERY_FILE="${ID}_query.txt"
DELETE_FILE="${ID}_delete.txt"
TRAVERSAL_FILE="${ID}_traversal.txt"

cleanup() {
    # Only remove uniquely named files created by this run.
    rm -f \
        "$WWW_DIR/$GET_FILE" \
        "$WWW_DIR/$SPACE_FILE" \
        "$WWW_DIR/$DELETE_FILE" \
        "$UPLOAD_DIR/$POST_FILE" \
        "$UPLOAD_DIR/$OVERWRITE_FILE" \
        "$UPLOAD_DIR/$EMPTY_FILE" \
        "$UPLOAD_DIR/$BINARY_FILE" \
        "$UPLOAD_DIR/$LARGE_FILE" \
        "$UPLOAD_DIR/$QUERY_FILE" \
        "./$TRAVERSAL_FILE"

    rm -rf "$TMP"
}
trap cleanup EXIT

line() {
    printf '%s\n' "------------------------------------------------------------"
}

section() {
    printf '\n'
    line
    printf '  %s\n' "$1"
    line
}

# Prepare test bodies.
printf 'GET test content\n' > "$TMP/get_body"
printf 'POST test content\n' > "$TMP/post_body"
printf 'Original content\n' > "$TMP/original"
printf 'Replacement content\n' > "$TMP/replacement"
printf 'Query string content\n' > "$TMP/query_body"
printf 'A\0B\0C' > "$TMP/binary_body"
: > "$TMP/empty_body"
head -c 262144 /dev/zero > "$TMP/large_body"

HTTP_STATUS=""
HTTP_BODY=""
HTTP_ERROR=""

request() {
    local method="$1"
    local path="$2"
    local body_file="${3:-}"
    local url="$BASE_URL$path"

    : > "$TMP/response"
    : > "$TMP/headers"
    : > "$TMP/status"
    : > "$TMP/curl_error"

    local args=()

    if [[ "$method" == "POST" ]]; then
        args=(-X POST
              -H 'Content-Type: application/octet-stream'
              --data-binary "@$body_file")
    else
        args=(-X "$method")
    fi

    curl -sS --max-time 8 --path-as-is \
        -D "$TMP/headers" \
        -o "$TMP/response" \
        -w '%{http_code}' \
        "${args[@]}" \
        "$url" \
        > "$TMP/status" 2> "$TMP/curl_error" || true

    HTTP_STATUS="$(cat "$TMP/status")"
    HTTP_BODY="$(cat "$TMP/response")"
    HTTP_ERROR="$(cat "$TMP/curl_error")"

    [[ -n "$HTTP_STATUS" ]] || HTTP_STATUS="000"
}

test_http() {
    local name="$1"
    local method="$2"
    local path="$3"
    local expected="$4"
    local body_file="${5:-}"
    local file_check="${6:-}"
    local expected_file="${7:-}"
    local absent_file="${8:-}"
    local reason="$9"

    TOTAL=$((TOTAL + 1))

    printf '\n'
    line
    printf '  TEST %02d: %s\n' "$TOTAL" "$name"
    line
    printf '  INPUT   : %s %s\n' "$method" "$path"

    if [[ "$method" == "POST" && -n "$body_file" ]]; then
        if [[ "$body_file" == "$TMP/large_body" ]]; then
            printf '  BODY    : 262144 bytes (256 KiB)\n'
        elif [[ ! -s "$body_file" ]]; then
            printf '  BODY    : [empty]\n'
        else
            printf '  BODY    : '
            head -c 100 "$body_file"
            printf '\n'
        fi
    fi

    printf '  EXPECTED: HTTP %s\n' "$expected"

    if [[ "$method" == "POST" ]]; then
        request "$method" "$path" "$body_file"
    else
        request "$method" "$path"
    fi

    printf '\n  ACTUAL  : HTTP %s\n' "$HTTP_STATUS"

    if [[ -n "$HTTP_BODY" ]]; then
        printf '  RESPONSE: '
        printf '%s' "$HTTP_BODY" | head -c 250
        printf '\n'
    else
        printf '  RESPONSE: [empty body]\n'
    fi

    local ok=1

    if [[ "$HTTP_STATUS" != "$expected" ]]; then
        ok=0
        reason="Expected HTTP $expected, received HTTP $HTTP_STATUS. $reason"
        if [[ "$HTTP_STATUS" == "000" ]]; then
            reason="No HTTP response. Is Webserv running at $BASE_URL?"
        fi
    fi

    if [[ -n "$file_check" ]]; then
        if [[ ! -f "$file_check" ]]; then
            ok=0
            reason="Expected file was not created: $file_check"
        elif [[ -n "$expected_file" ]] &&
             ! cmp -s "$expected_file" "$file_check"; then
            ok=0
            reason="File exists, but its contents differ from the submitted body."
        fi
    fi

    if [[ -n "$absent_file" && -e "$absent_file" ]]; then
        ok=0
        reason="File still exists, but it should have been removed: $absent_file"
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

# Raw requests are useful for malformed HTTP cases that curl fixes for us.
raw_test() {
    local name="$1"
    local expected="$2"
    local raw_request="$3"
    local reason="$4"

    TOTAL=$((TOTAL + 1))

    printf '\n'
    line
    printf '  TEST %02d: %s\n' "$TOTAL" "$name"
    line
    printf '  INPUT   : Raw HTTP request\n'
    printf '  EXPECTED: HTTP %s\n' "$expected"

    if ! command -v nc >/dev/null 2>&1; then
        printf '  RESULT  : SKIP\n'
        printf '  REASON  : nc (netcat) is not installed.\n'
        SKIP=$((SKIP + 1))
        return
    fi

    printf '%b' "$raw_request" |
        timeout 4 nc -w 2 "$HOST" "$PORT" \
        > "$TMP/raw_response" 2> "$TMP/raw_error" || true

    printf '  RESPONSE:\n'
    head -n 8 "$TMP/raw_response" | sed 's/^/    /'

    if grep -qE "^HTTP/1\.[01] $expected([[:space:]]|$)" "$TMP/raw_response"; then
        printf '\n  RESULT  : PASS\n'
        PASS=$((PASS + 1))
    else
        printf '\n  RESULT  : FAIL\n'
        printf '  REASON  : %s\n' "$reason"
        printf '  DETAIL  : Expected HTTP %s in the response status line.\n' "$expected"
        FAIL=$((FAIL + 1))
    fi
}

# ------------------------------------------------------------
# SETUP
# ------------------------------------------------------------

section "42 WEBSERV HTTP METHOD TEST SUITE"

printf '  Server      : %s\n' "$BASE_URL"
printf '  Web root    : %s\n' "$WWW_DIR"
printf '  Upload root : %s\n' "$UPLOAD_DIR"
printf '  Test ID     : %s\n' "$ID"

if ! command -v curl >/dev/null 2>&1; then
    printf '\nERROR: curl is not installed.\n'
    exit 1
fi

if [[ ! -d "$WWW_DIR" || ! -d "$UPLOAD_DIR" ]]; then
    printf '\nERROR: www or uploads directory is missing.\n'
    printf 'Create them with: mkdir -p www uploads\n'
    exit 1
fi

printf 'GET test content\n' > "$WWW_DIR/$GET_FILE"
printf 'File with a space in its name\n' > "$WWW_DIR/$SPACE_FILE"
printf 'Temporary file for DELETE\n' > "$WWW_DIR/$DELETE_FILE"
printf 'Original content\n' > "$UPLOAD_DIR/$OVERWRITE_FILE"

# ------------------------------------------------------------
# GET TESTS
# ------------------------------------------------------------

section "GET TESTS"

test_http \
    "GET an existing file" \
    GET "/$GET_FILE" 200 "" \
    "$WWW_DIR/$GET_FILE" "$TMP/get_body" "" \
    "Existing static files should return 200 and the correct contents."

test_http \
    "GET with query parameters" \
    GET "/$GET_FILE?a=1&b=two" 200 "" \
    "$WWW_DIR/$GET_FILE" "$TMP/get_body" "" \
    "Query parameters should not be treated as part of the filesystem filename."

test_http \
    "GET a nonexistent file" \
    GET "/${ID}_missing.txt" 404 "" "" "" "" \
    "A missing resource should return 404."

test_http \
    "GET a filename containing a space (URL encoded)" \
    GET "/${ID}_space%20name.txt" 200 "" \
    "$WWW_DIR/$SPACE_FILE" "" "" \
    "The server should correctly resolve percent-encoded spaces in a URI."

test_http \
    "GET traversal attempt" \
    GET "/../etc/passwd" 400 "" "" "" "" \
    "Traversal outside the configured root must be rejected."

test_http \
    "GET the POST-only upload location" \
    GET "/upload/$POST_FILE" 405 "" "" "" "" \
    "A location configured for POST only should reject GET with 405."

# ------------------------------------------------------------
# POST TESTS
# ------------------------------------------------------------

section "POST TESTS"

test_http \
    "POST creates a new file" \
    POST "/upload/$POST_FILE" 201 "$TMP/post_body" \
    "$UPLOAD_DIR/$POST_FILE" "$TMP/post_body" "" \
    "A successful upload to a new filename should return 201."

test_http \
    "POST overwrites an existing file" \
    POST "/upload/$OVERWRITE_FILE" 200 "$TMP/replacement" \
    "$UPLOAD_DIR/$OVERWRITE_FILE" "$TMP/replacement" "" \
    "Replacing an existing file should return 200 and replace its contents."

test_http \
    "POST with an empty body" \
    POST "/upload/$EMPTY_FILE" 201 "$TMP/empty_body" \
    "$UPLOAD_DIR/$EMPTY_FILE" "$TMP/empty_body" "" \
    "An empty request body should create a zero-byte file if allowed."

test_http \
    "POST with query parameters" \
    POST "/upload/$QUERY_FILE?source=test&n=42" 201 "$TMP/query_body" \
    "$UPLOAD_DIR/$QUERY_FILE" "$TMP/query_body" "" \
    "The query string must not become part of the uploaded filename."

test_http \
    "POST binary data containing null bytes" \
    POST "/upload/$BINARY_FILE" 201 "$TMP/binary_body" \
    "$UPLOAD_DIR/$BINARY_FILE" "$TMP/binary_body" "" \
    "The request body must be handled as bytes, not as a null-terminated string."

test_http \
    "POST a larger 256 KiB body" \
    POST "/upload/$LARGE_FILE" 201 "$TMP/large_body" \
    "$UPLOAD_DIR/$LARGE_FILE" "$TMP/large_body" "" \
    "The complete body should be saved without truncation."

test_http \
    "POST without a filename" \
    POST "/upload" 400 "$TMP/post_body" "" "" "" \
    "An empty upload filename should be rejected by the upload handler."

test_http \
    "POST to the root location" \
    POST "/" 405 "$TMP/post_body" "" "" "" \
    "The root location should reject POST when it does not allow the method."

test_http \
    "POST path traversal attempt" \
    POST "/upload/../$TRAVERSAL_FILE" 400 "$TMP/post_body" "" "" "" \
    "A filename attempting to escape the upload directory must be rejected."

# Verify traversal did not create a file in the project root.
if [[ -e "./$TRAVERSAL_FILE" ]]; then
    printf '\n  EXTRA CHECK: FAIL\n'
    printf '  REASON: Traversal file appeared outside the upload directory.\n'
    FAIL=$((FAIL + 1))
else
    printf '\n  EXTRA CHECK: PASS\n'
    printf '  REASON: No traversal file was created in the project root.\n'
    PASS=$((PASS + 1))
fi
TOTAL=$((TOTAL + 1))

# ------------------------------------------------------------
# DELETE TESTS
# ------------------------------------------------------------

section "DELETE TESTS"

test_http \
    "DELETE an existing file" \
    DELETE "/$DELETE_FILE" 204 "" "" "" \
    "$WWW_DIR/$DELETE_FILE" \
    "Successful deletion should return 204 and remove the file."

test_http \
    "DELETE a nonexistent file" \
    DELETE "/${ID}_delete_missing.txt" 404 "" "" "" "" \
    "Deleting a missing file should return 404."

test_http \
    "DELETE a file in the POST-only upload location" \
    DELETE "/upload/$POST_FILE" 405 "" "" "" "" \
    "A POST-only location should reject DELETE with 405."

# ------------------------------------------------------------
# METHOD AND PARSER TESTS
# ------------------------------------------------------------

section "METHOD AND HTTP PARSER TESTS"

test_http \
    "Unsupported PUT method" \
    PUT "/$GET_FILE" 405 "" "" "" "" \
    "PUT is not one of the three required methods in this project."

test_http \
    "Unknown FOO method" \
    FOO "/$GET_FILE" 405 "" "" "" "" \
    "An unsupported method should be rejected rather than treated as GET."

raw_test \
    "Malformed request line" \
    400 \
    'GET / HTTP/1.1 extra\r\nHost: localhost\r\n\r\n' \
    "Malformed request lines should produce 400 Bad Request."

raw_test \
    "LF-only line endings" \
    400 \
    'GET / HTTP/1.1\nHost: localhost\n\n' \
    "Invalid line endings should be rejected with 400."

raw_test \
    "Unsupported HTTP version" \
    505 \
    'GET / HTTP/9.9\r\nHost: localhost\r\n\r\n' \
    "An unsupported HTTP version should be rejected with 505."

# ------------------------------------------------------------
# SUMMARY
# ------------------------------------------------------------

section "FINAL SUMMARY"

printf '  Total checks : %d\n' "$TOTAL"
printf '  Passed       : %d\n' "$PASS"
printf '  Failed       : %d\n' "$FAIL"
printf '  Skipped      : %d\n' "$SKIP"

if [[ "$FAIL" -eq 0 ]]; then
    printf '\n  ALL EXECUTED TESTS PASSED\n'
else
    printf '\n  SOME TESTS FAILED. Review each REASON above.\n'
fi

line

[[ "$FAIL" -eq 0 ]]
