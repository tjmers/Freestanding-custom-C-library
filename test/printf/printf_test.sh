#!/bin/bash
# Runs the printf test suite.
#
# Every case in test/printf/printf_cases.inc runs in its own process under a timeout,
# and its stdout is compared byte for byte with the expected output. The same
# driver is also built against the host glibc. If glibc disagrees with an
# expected string, the case is reported as BAD-EXPECT: the table is wrong, not
# the library.
#
# Usage: test/printf/printf_test.sh [-v] [name-prefix]
#   -v           show a byte dump of the expected and actual output on failure
#   name-prefix  only run cases whose name starts with this (e.g. "d_", "f_inf")

set -u
cd "$(dirname "$0")/../.." || exit 2

# A missing tool would otherwise show up as every case failing (e.g. cmp, which
# comes from diffutils and is not installed by default on Arch).
missing=()
for tool in make gcc cmp od timeout mktemp; do
  command -v "$tool" > /dev/null || missing+=("$tool")
done
if [ ${#missing[@]} -ne 0 ]; then
  echo "printf_test.sh: missing required tools: ${missing[*]}" >&2
  echo "  (cmp is in the diffutils package; od and timeout are in coreutils)" >&2
  exit 2
fi

verbose=0
if [ "${1:-}" = "-v" ]; then
  verbose=1
  shift
fi
filter="${1:-}"

bin=build/printf_test/printf_mylibc
ref=build/printf_test/printf_glibc
timeout_secs=2

make -s "$bin" || exit 2
gcc -w -O0 -DPRINTF_TEST_HOST_LIBC -o "$ref" test/printf/printf.c || exit 2

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

dump() {
  # Show at most the first 256 bytes, with escapes for non-printable bytes.
  head -c 256 "$1" | od -An -c | sed 's/^/      /'
  local size
  size=$(wc -c < "$1")
  if [ "$size" -gt 256 ]; then
    echo "      ... ($size bytes total)"
  fi
}

declare -A cat_pass cat_total
categories=()
total=0
passed=0
bad_expect=0

# The case list and expected output come from the glibc build, so a broken
# mylibc cannot hide cases or corrupt the expectations.
for name in $("$ref" --list); do
  [[ "$name" == "$filter"* ]] || continue

  category=${name%%_*}
  if [ -z "${cat_total[$category]+x}" ]; then
    categories+=("$category")
    cat_total[$category]=0
    cat_pass[$category]=0
  fi
  cat_total[$category]=$((cat_total[$category] + 1))
  total=$((total + 1))

  "$ref" --expect "$name" > "$tmp/expected"

  "$ref" "$name" > "$tmp/glibc" 2>/dev/null
  if ! cmp -s "$tmp/expected" "$tmp/glibc"; then
    bad_expect=$((bad_expect + 1))
    echo "BAD-EXPECT $name (glibc disagrees with the expected output)"
    if [ $verbose -eq 1 ]; then
      echo "    expected:"; dump "$tmp/expected"
      echo "    glibc:";    dump "$tmp/glibc"
    fi
  fi

  timeout "$timeout_secs" "$bin" "$name" > "$tmp/actual" 2> "$tmp/stderr"
  rc=$?

  if [ $rc -eq 124 ]; then
    echo "TIMEOUT    $name (no exit after ${timeout_secs}s)"
  elif [ $rc -ge 128 ]; then
    echo "CRASH      $name (SIG$(kill -l $((rc - 128)) 2>/dev/null || echo $((rc - 128))))"
  elif [ $rc -ne 0 ]; then
    echo "ERROR      $name (exit status $rc)"
  elif cmp -s "$tmp/expected" "$tmp/actual"; then
    passed=$((passed + 1))
    cat_pass[$category]=$((cat_pass[$category] + 1))
    continue
  else
    echo "FAIL       $name"
    if [ $verbose -eq 1 ]; then
      echo "    $(cmp "$tmp/expected" "$tmp/actual" 2>&1 | sed "s|$tmp/||g")"
      echo "    expected:"; dump "$tmp/expected"
      echo "    actual:";   dump "$tmp/actual"
    fi
  fi

  if [ $verbose -eq 1 ] && [ -s "$tmp/stderr" ]; then
    echo "    stderr:"
    head -n 5 "$tmp/stderr" | sed 's/^/      /'
  fi
done

if [ $total -eq 0 ]; then
  echo "No cases match '$filter'"
  exit 2
fi

echo
echo "Category   Passed"
for category in "${categories[@]}"; do
  printf "%-10s %d/%d\n" "$category" "${cat_pass[$category]}" "${cat_total[$category]}"
done
echo
echo "Total: $passed/$total passed"
if [ $bad_expect -ne 0 ]; then
  echo "$bad_expect case(s) have expected output that glibc disagrees with"
fi

[ $passed -eq $total ] && [ $bad_expect -eq 0 ]
