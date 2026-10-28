#!/usr/bin/env bash
# Run the HTTP/Webserv and expanded CGI suites as one command.
# Usage: ./BashScripts/full_project.sh (from the project root)
# Optional target override: BASE_URL=http://127.0.0.1:2020 ./BashScripts/full_project.sh
set -u

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
HTTP_SUITE="$ROOT_DIR/webserv_intense_test.sh"
CGI_SUITE="$ROOT_DIR/cgi_intense_test.sh"
BASE_URL="${BASE_URL:-http://127.0.0.1:2020}"
AUTHORITY="${BASE_URL#*://}"
AUTHORITY="${AUTHORITY%%/*}"
if [[ "$AUTHORITY" == *:* ]]; then
    HOST="${AUTHORITY%:*}"
    PORT="${AUTHORITY##*:}"
else
    HOST="$AUTHORITY"
    PORT=80
fi

if [[ -t 1 && -z "${NO_COLOR:-}" ]]; then
    RED=$'\033[91m'; GREEN=$'\033[92m'; YELLOW=$'\033[93m'; CYAN=$'\033[96m'
    BOLD=$'\033[1m'; DIM=$'\033[2m'; RESET=$'\033[0m'
else
    RED=""; GREEN=""; YELLOW=""; CYAN=""; BOLD=""; DIM=""; RESET=""
fi

if [[ ! -f "$HTTP_SUITE" || ! -f "$CGI_SUITE" ]]; then
    printf 'Required suite not found beside this script.\n' >&2
    printf 'Expected:\n  %s\n  %s\n' "$HTTP_SUITE" "$CGI_SUITE" >&2
    exit 2
fi

HTTP_LOG="$(mktemp)" || exit 2
CGI_LOG="$(mktemp)" || { rm -f "$HTTP_LOG"; exit 2; }
trap 'rm -f "$HTTP_LOG" "$CGI_LOG"' EXIT INT TERM

printf '\n%s%s╭──────────────────────────────────────────────────────────────╮%s\n' "$BOLD" "$CYAN" "$RESET"
printf '%s%s│              WEBSERV FULL INTENSE TEST SUITE                 │%s\n' "$BOLD" "$CYAN" "$RESET"
printf '%s%s╰──────────────────────────────────────────────────────────────╯%s\n' "$BOLD" "$CYAN" "$RESET"
printf '  Target: %s\n' "$BASE_URL"
printf '  Runs both suites; a failure in one will not skip the other.\n'

printf '\n%s%s[1/2] HTTP parser, GET, POST, DELETE, and core behavior%s\n' "$BOLD" "$CYAN" "$RESET"
if command -v script >/dev/null 2>&1; then
    HTTP_SUITE="$HTTP_SUITE" BASE_URL="$BASE_URL" script -q -e -c 'stty cols 200; bash "$HTTP_SUITE"' /dev/null 2>&1 | tee "$HTTP_LOG"
    HTTP_STATUS=${PIPESTATUS[0]}
else
    BASE_URL="$BASE_URL" bash "$HTTP_SUITE" 2>&1 | tee "$HTTP_LOG"
    HTTP_STATUS=${PIPESTATUS[0]}
fi
HTTP_COUNTS="$(sed -E 's/\x1B\[[0-9;]*[[:alpha:]]//g' "$HTTP_LOG" | sed -nE 's/.*Total: ([0-9]+).*Passed: ([0-9]+).*Failed: ([0-9]+).*Skipped: ([0-9]+).*/\1 \2 \3 \4/p' | tail -n 1)"

printf '\n%s%s[2/2] CGI routing, body handling, timeout, and concurrency%s\n' "$BOLD" "$CYAN" "$RESET"
if command -v script >/dev/null 2>&1; then
    CGI_SUITE="$CGI_SUITE" HOST="$HOST" PORT="$PORT" script -q -e -c 'stty cols 200; bash "$CGI_SUITE" "$HOST" "$PORT"' /dev/null 2>&1 | tee "$CGI_LOG"
    CGI_STATUS=${PIPESTATUS[0]}
else
    HOST="$HOST" PORT="$PORT" bash "$CGI_SUITE" 2>&1 | tee "$CGI_LOG"
    CGI_STATUS=${PIPESTATUS[0]}
fi
CGI_COUNTS="$(sed -E 's/\x1B\[[0-9;]*[[:alpha:]]//g' "$CGI_LOG" | sed -nE 's/^[[:space:]]*([0-9]+) passed[[:space:]]+([0-9]+) failed[[:space:]]+([0-9]+) skipped[[:space:]]+([0-9]+) total.*/\1 \2 \3 \4/p' | tail -n 1)"

printf '\n%s%s========================================================================%s\n' "$BOLD" "$CYAN" "$RESET"
printf '%s%s                             RESULTS SUMMARY%s\n' "$BOLD" "$CYAN" "$RESET"
printf '%s%s========================================================================%s\n' "$BOLD" "$CYAN" "$RESET"

HTTP_READY=0
if [[ -n "$HTTP_COUNTS" ]]; then
    read -r HTTP_TOTAL HTTP_PASSED HTTP_FAILED HTTP_SKIPPED <<< "$HTTP_COUNTS"
    HTTP_READY=1
else
    HTTP_TOTAL='?'; HTTP_PASSED='?'; HTTP_FAILED='?'; HTTP_SKIPPED='?'
fi
CGI_READY=0
if [[ -n "$CGI_COUNTS" ]]; then
    read -r CGI_PASSED CGI_FAILED CGI_SKIPPED CGI_TOTAL <<< "$CGI_COUNTS"
    CGI_READY=1
else
    CGI_TOTAL='?'; CGI_PASSED='?'; CGI_FAILED='?'; CGI_SKIPPED='?'
fi

print_result_row() {
    printf '  %-7s %s✓ %3s passed%s   %s✗ %3s failed%s   %s↷ %3s skipped%s   %3s total\n' \
        "$1" \
        "$GREEN" "$2" "$RESET" \
        "$RED" "$3" "$RESET" \
        "$YELLOW" "$4" "$RESET" \
        "$5"
}
if (( HTTP_READY && CGI_READY )); then
    TOTAL_PASSED=$((HTTP_PASSED + CGI_PASSED))
    TOTAL_FAILED=$((HTTP_FAILED + CGI_FAILED))
    TOTAL_SKIPPED=$((HTTP_SKIPPED + CGI_SKIPPED))
    TOTAL_TESTS=$((HTTP_TOTAL + CGI_TOTAL))
fi

ANIMATING=0
clear_result_line() {
    if (( ANIMATING )); then printf '\033[2K'; fi
}
render_result_rows() {
    clear_result_line; print_result_row "HTTP" "$HTTP_PASSED" "$HTTP_FAILED" "$HTTP_SKIPPED" "$HTTP_TOTAL"
    clear_result_line; print_result_row "CGI" "$CGI_PASSED" "$CGI_FAILED" "$CGI_SKIPPED" "$CGI_TOTAL"
    clear_result_line; printf '%s%s------------------------------------------------------------------------%s\n' "$CYAN" "$CYAN" "$RESET"
    clear_result_line; print_result_row "TOTAL" "$TOTAL_PASSED" "$TOTAL_FAILED" "$TOTAL_SKIPPED" "$TOTAL_TESTS"
    clear_result_line; printf '%s%s========================================================================%s\n' "$BOLD" "$CYAN" "$RESET"
}

if (( HTTP_READY && CGI_READY )) && [[ -t 1 && "${TERM:-dumb}" != "dumb" ]]; then
    TARGET_HTTP_PASSED=$HTTP_PASSED; TARGET_HTTP_FAILED=$HTTP_FAILED; TARGET_HTTP_SKIPPED=$HTTP_SKIPPED
    TARGET_CGI_PASSED=$CGI_PASSED; TARGET_CGI_FAILED=$CGI_FAILED; TARGET_CGI_SKIPPED=$CGI_SKIPPED
    TARGET_HTTP_TOTAL=$HTTP_TOTAL; TARGET_CGI_TOTAL=$CGI_TOTAL
    ANIMATING=1
    printf '\033[?25l'
    for FRAME in {0..10}; do
        if (( FRAME > 0 )); then printf '\033[5A'; fi
        HTTP_PASSED=$((TARGET_HTTP_PASSED * FRAME / 10))
        HTTP_FAILED=$((TARGET_HTTP_FAILED * FRAME / 10))
        HTTP_SKIPPED=$((TARGET_HTTP_SKIPPED * FRAME / 10))
        HTTP_TOTAL=$((TARGET_HTTP_TOTAL * FRAME / 10))
        CGI_PASSED=$((TARGET_CGI_PASSED * FRAME / 10))
        CGI_FAILED=$((TARGET_CGI_FAILED * FRAME / 10))
        CGI_SKIPPED=$((TARGET_CGI_SKIPPED * FRAME / 10))
        CGI_TOTAL=$((TARGET_CGI_TOTAL * FRAME / 10))
        TOTAL_PASSED=$((HTTP_PASSED + CGI_PASSED))
        TOTAL_FAILED=$((HTTP_FAILED + CGI_FAILED))
        TOTAL_SKIPPED=$((HTTP_SKIPPED + CGI_SKIPPED))
        TOTAL_TESTS=$((HTTP_TOTAL + CGI_TOTAL))
        render_result_rows
        if (( FRAME < 10 )); then sleep 0.05; fi
    done
    printf '\033[?25h'
else
    if (( ! HTTP_READY || ! CGI_READY )); then
        TOTAL_PASSED='?'; TOTAL_FAILED='?'; TOTAL_SKIPPED='?'; TOTAL_TESTS='?'
    fi
    render_result_rows
fi
printf '%s%s========================================================================%s\n' "$BOLD" "$CYAN" "$RESET"
print_http_failures() {
    sed -E 's/\x1B\[[0-9;]*[[:alpha:]]//g; s/\r$//' "$HTTP_LOG" | awk '
        /TEST [0-9]+:/ {
            if (in_test && failed) printf "%s", block
            block = $0 ORS
            in_test = 1
            failed = 0
            next
        }
        in_test {
            block = block $0 ORS
            if ($0 ~ /RESULT:.*FAIL/) failed = 1
        }
        END {
            if (in_test && failed) printf "%s", block
        }
    '
}

print_cgi_failures() {
    sed -E 's/\x1B\[[0-9;]*[[:alpha:]]//g; s/\r$//' "$CGI_LOG" | awk '
        /^[[:space:]]*✗ / {
            if (active) printf "%s", block
            block = $0 ORS
            active = 1
            next
        }
        active {
            if ($0 ~ /^[[:space:]]*(✓ |━|[0-9]+ passed)/) {
                printf "%s", block
                block = ""
                active = 0
            } else {
                block = block $0 ORS
            }
        }
        END {
            if (active) printf "%s", block
        }
    '
}

HTTP_FAILURE_DETAILS="$(print_http_failures)"
CGI_FAILURE_DETAILS="$(print_cgi_failures)"
if [[ -n "$HTTP_FAILURE_DETAILS" || -n "$CGI_FAILURE_DETAILS" ]]; then
    printf '\n%s%s========================================================================%s\n' "$BOLD" "$CYAN" "$RESET"
    printf '%s%s                         FAILED TEST DETAILS%s\n' "$BOLD" "$CYAN" "$RESET"
    printf '%s%s========================================================================%s\n' "$BOLD" "$CYAN" "$RESET"
    if [[ -n "$HTTP_FAILURE_DETAILS" ]]; then
        printf '%s%sHTTP SUITE%s\n' "$BOLD" "$RED" "$RESET"
        printf '%s\n' "$HTTP_FAILURE_DETAILS"
    fi
    if [[ -n "$CGI_FAILURE_DETAILS" ]]; then
        printf '%s%sCGI SUITE%s\n' "$BOLD" "$RED" "$RESET"
        printf '%s\n' "$CGI_FAILURE_DETAILS"
    fi
    printf '%s%s========================================================================%s\n' "$BOLD" "$CYAN" "$RESET"
fi

BANNER_WIDTH=72
if (( HTTP_STATUS == 0 && CGI_STATUS == 0 && HTTP_READY && CGI_READY && HTTP_FAILED == 0 && CGI_FAILED == 0 && HTTP_SKIPPED == 0 && CGI_SKIPPED == 0 )); then
    PASSED_TEXT='✦ 🔥 ALL TESTS PASSED 🔥 ✦'
    LEGENDS_TEXT='🍆💦 YOU FUCKING LEGENDS 🍆💦'
    LEGENDS_DRY_TEXT='🍆   YOU FUCKING LEGENDS 🍆  '
    PASSED_PADDING=$(( (BANNER_WIDTH - $(printf '%s\n' "$PASSED_TEXT" | wc -L)) / 2 ))
    printf '%*s%s%s✦ 🔥%s %sALL TESTS PASSED%s %s🔥 ✦%s\n' \
        "$PASSED_PADDING" '' "$BOLD" "$YELLOW" "$RESET" "$BOLD$GREEN" "$RESET" "$YELLOW" "$RESET"
    LEGENDS_PADDING=$(( (BANNER_WIDTH - $(printf '%s\n' "$LEGENDS_TEXT" | wc -L)) / 2 ))
    print_legend_frame() {
        local text="$1"
        printf '\r%*s%s%s%s\033[K' \
            "$LEGENDS_PADDING" '' "$BOLD$GREEN" "$text" "$RESET"
    }
    printf '\033[?25l'
    print_legend_frame "$LEGENDS_DRY_TEXT"
    sleep 0.5
    print_legend_frame "$LEGENDS_TEXT"
    sleep 0.5
    print_legend_frame "$LEGENDS_DRY_TEXT"
    sleep 0.5
    print_legend_frame "$LEGENDS_TEXT"
    sleep 0.5
    print_legend_frame "$LEGENDS_DRY_TEXT"
    sleep 0.5
    print_legend_frame "$LEGENDS_TEXT"
    printf '\n\033[?25h'
    printf '%s%s========================================================================%s\n' "$BOLD" "$CYAN" "$RESET"
    exit 0
fi
    FAILURE_LABEL='🩸🔪 FAILURE 🔪🩸'
    FAIL_TEXT='😡 I TOLD YOU NOT TO LET ORHAN TOUCH THE CODE 😡'
    FAILURE_PADDING=$(( (BANNER_WIDTH - $(printf '%s\n' "$FAILURE_LABEL" | wc -L)) / 2 ))
    FAIL_PADDING=$(( (BANNER_WIDTH - $(printf '%s\n' "$FAIL_TEXT" | wc -L)) / 2 ))
    printf '%*s%s%s%s\n' \
        "$FAILURE_PADDING" '' "$BOLD$RED" "$FAILURE_LABEL" "$RESET"
    printf '%*s%s%s%s\n' \
        "$FAIL_PADDING" '' "$BOLD$RED" "$FAIL_TEXT" "$RESET"
    printf '%s%s========================================================================%s\n' "$BOLD" "$CYAN" "$RESET"
if (( HTTP_STATUS == 0 && CGI_STATUS == 0 )); then
    exit 0
fi
exit 1
