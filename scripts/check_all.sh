#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
for check in check_enter check_retry check_receive check_payload check_tcp check_unipus; do
    python "scripts/${check}.py"
done
shift_check=$(mktemp /tmp/vibe-shift-XXXXXX)
trap 'rm -f "$shift_check"' EXIT
cc -std=c11 -Wall -Wextra -Werror -I main scripts/check_shift.c -o "$shift_check"
"$shift_check"
