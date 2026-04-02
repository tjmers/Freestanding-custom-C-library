#include "../intf/stdlib.h"
#include "../intf/stdint.h"
#include "../intf/syscalls.h"

static void write_str(const char* s, uint64_t n) {
  write(1, s, n);
}

static void report_fail(const char* msg, uint64_t n) {
  write_str("FAIL: ", 6);
  write_str(msg, n);
  write_str("\n", 1);
}

static void report_ok(const char* msg, uint64_t n) {
  write_str("OK: ", 4);
  write_str(msg, n);
  write_str("\n", 1);
}

static int buf_prefix_eq(const char* buf, uint32_t n, const char* expected, uint32_t len) {
  if (n != len) {
    return 0;
  }
  for (uint32_t i = 0; i < len; ++i) {
    if (buf[i] != expected[i]) {
      return 0;
    }
  }
  return 1;
}

static int test_itoa_zero(void) {
  char buf[32];
  uint32_t n = itoa(0, buf, sizeof(buf));
  const char exp[] = "0";
  if (!buf_prefix_eq(buf, n, exp, sizeof(exp) - 1)) {
    const char err[] = "itoa(0) expected \"0\"";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "itoa zero";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_itoa_positive(void) {
  char buf[32];
  uint32_t n = itoa(12345, buf, sizeof(buf));
  const char exp[] = "12345";
  if (!buf_prefix_eq(buf, n, exp, sizeof(exp) - 1)) {
    const char err[] = "itoa(12345) expected \"12345\"";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "itoa positive";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_itoa_negative(void) {
  char buf[32];
  uint32_t n = itoa(-42, buf, sizeof(buf));
  const char exp[] = "-42";
  if (!buf_prefix_eq(buf, n, exp, sizeof(exp) - 1)) {
    const char err[] = "itoa(-42) expected \"-42\"";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "itoa negative";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_itoa_int_min(void) {
  char buf[32];
  uint32_t n = itoa(INT32_MIN, buf, sizeof(buf));
  const char exp[] = "-2147483648";
  if (!buf_prefix_eq(buf, n, exp, sizeof(exp) - 1)) {
    const char err[] = "itoa(INT32_MIN) expected \"-2147483648\"";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "itoa INT32_MIN";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_itoa_buffer_too_small(void) {
  char buf[4];
  uint32_t n = itoa(INT32_MIN, buf, sizeof(buf));
  if (n != 0) {
    const char err[] = "itoa(INT32_MIN) with small buffer should return 0";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "itoa buffer too small for INT32_MIN";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_atoi_zero(void) {
  char s[] = "0";
  if (atoi(s) != 0) {
    const char err[] = "atoi(\"0\") expected 0";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "atoi zero";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_atoi_positive(void) {
  char s[] = "90210";
  if (atoi(s) != 90210) {
    const char err[] = "atoi(\"90210\") expected 90210";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "atoi positive";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_atoi_negative(void) {
  char s[] = "-273";
  if (atoi(s) != -273) {
    const char err[] = "atoi(\"-273\") expected -273";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "atoi negative";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_atoi_leading_whitespace(void) {
  char s[] = "  \t\r\n 7";
  if (atoi(s) != 7) {
    const char err[] = "atoi with leading whitespace expected 7";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "atoi leading whitespace";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_atoi_explicit_plus(void) {
  char s[] = "+99";
  if (atoi(s) != 99) {
    const char err[] = "atoi(\"+99\") expected 99";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "atoi explicit plus";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_atoi_stops_at_non_digit(void) {
  char s[] = "12345abc";
  if (atoi(s) != 12345) {
    const char err[] = "atoi should stop at first non-digit";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "atoi stops at non-digit";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_atoi_empty(void) {
  char s[] = "";
  if (atoi(s) != 0) {
    const char err[] = "atoi(\"\") expected 0";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "atoi empty string";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

int main(void) {
  int rc = 0;

  rc |= test_itoa_zero();
  rc |= test_itoa_positive();
  rc |= test_itoa_negative();
  rc |= test_itoa_int_min();
  rc |= test_itoa_buffer_too_small();

  rc |= test_atoi_zero();
  rc |= test_atoi_positive();
  rc |= test_atoi_negative();
  rc |= test_atoi_leading_whitespace();
  rc |= test_atoi_explicit_plus();
  rc |= test_atoi_stops_at_non_digit();
  rc |= test_atoi_empty();

  return rc;
}
