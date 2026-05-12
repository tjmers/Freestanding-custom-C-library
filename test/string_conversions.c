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

static int buf_eq(const char* buf, uint32_t n, const char* expected, uint32_t len) {
  return buf_prefix_eq(buf, n, expected, len);
}

static uint32_t u32_min(uint32_t a, uint32_t b) {
  return a < b ? a : b;
}

// Write an integer in repeated chunks, appending to out.
// If non_contiguous is true, each chunk is written into a temp buffer and then copied into out.
static int write_int_chunks(int64_t value, uint32_t chunk_size, int non_contiguous, char* out, uint32_t out_cap, uint32_t* out_len) {
  uint32_t idx = 0;
  int64_t v = value;

  while (1) {
    if (idx >= out_cap) {
      return 0;
    }
    if (v == 0) {
      break;
    }

    uint32_t avail = out_cap - idx;
    uint32_t use = u32_min(chunk_size, avail);
    if (use == 0) {
      return 0;
    }

    if (!non_contiguous) {
      uint32_t n = itoa(&v, &out[idx], use);
      if (n == 0) {
        return 0;
      }
      idx += n;
    } else {
      char tmp[32];
      if (use > sizeof(tmp)) {
        use = sizeof(tmp);
      }
      uint32_t n = itoa(&v, tmp, use);
      if (n == 0) {
        return 0;
      }
      for (uint32_t i = 0; i < n; ++i) {
        out[idx + i] = tmp[i];
      }
      idx += n;
    }
  }

  *out_len = idx;
  return 1;
}

static int test_itoa_zero(void) {
  char buf[32];
  int64_t v = 0;
  uint32_t n = itoa(&v, buf, sizeof(buf));
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
  int64_t v = 12345;
  uint32_t n = itoa(&v, buf, sizeof(buf));
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
  int64_t v = -42;
  uint32_t n = itoa(&v, buf, sizeof(buf));
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
  int64_t v = (int64_t)INT32_MIN;
  uint32_t n = itoa(&v, buf, sizeof(buf));
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
  int64_t v = (int64_t)INT32_MIN;
  uint32_t n = itoa(&v, buf, sizeof(buf));
  // Resumable behavior: should write as much as fits and leave remainder in v.
  if (n != sizeof(buf)) {
    const char err[] = "itoa partial write should fill provided buffer";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  if (v == 0) {
    const char err[] = "itoa partial write should leave remainder non-zero";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "itoa partial write leaves remainder";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_itoa_int64_min_continuous_chunks(void) {
  char out[64];
  uint32_t n = 0;
  if (!write_int_chunks(INT64_MIN, 3, 0, out, sizeof(out), &n)) {
    const char err[] = "itoa INT64_MIN continuous chunk write failed";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char exp[] = "-9223372036854775808";
  if (!buf_eq(out, n, exp, sizeof(exp) - 1)) {
    const char err[] = "itoa INT64_MIN continuous chunk write mismatch";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "itoa INT64_MIN continuous chunks";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_itoa_int64_min_noncontiguous_chunks(void) {
  char out[64];
  uint32_t n = 0;
  if (!write_int_chunks(INT64_MIN, 4, 1, out, sizeof(out), &n)) {
    const char err[] = "itoa INT64_MIN non-contiguous chunk write failed";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char exp[] = "-9223372036854775808";
  if (!buf_eq(out, n, exp, sizeof(exp) - 1)) {
    const char err[] = "itoa INT64_MIN non-contiguous chunk write mismatch";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "itoa INT64_MIN non-contiguous chunks";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_itoa_resumable_across_flushes(void) {
  // Simulate a buffered stream that flushes every 2 bytes.
  char out[64];
  uint32_t n = 0;
  if (!write_int_chunks(-1234567, 2, 1, out, sizeof(out), &n)) {
    const char err[] = "itoa resumable across flushes failed";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char exp[] = "-1234567";
  if (!buf_eq(out, n, exp, sizeof(exp) - 1)) {
    const char err[] = "itoa resumable across flushes mismatch";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "itoa resumable across flushes";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_itoab_zero(void) {
  char buf[32];
  int64_t v = 0;
  uint32_t n = itoab(&v, buf, sizeof(buf));
  const char exp[] = "0";
  if (!buf_prefix_eq(buf, n, exp, sizeof(exp) - 1)) {
    const char err[] = "itoab(0) expected \"0\"";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "itoab zero";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_itoab_basic(void) {
  char buf[32];
  int64_t v = 5;
  uint32_t n = itoab(&v, buf, sizeof(buf));
  const char exp[] = "101";
  if (!buf_prefix_eq(buf, n, exp, sizeof(exp) - 1)) {
    const char err[] = "itoab(5) expected \"101\"";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "itoab basic";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_itoab_negative(void) {
  char buf[32];
  int64_t v = -5;
  uint32_t n = itoab(&v, buf, sizeof(buf));
  const char exp[] = "-101";
  if (!buf_prefix_eq(buf, n, exp, sizeof(exp) - 1)) {
    const char err[] = "itoab(-5) expected \"-101\"";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "itoab negative";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_itoao_basic(void) {
  char buf[32];
  int64_t v = 64;
  uint32_t n = itoao(&v, buf, sizeof(buf));
  const char exp[] = "100";
  if (!buf_prefix_eq(buf, n, exp, sizeof(exp) - 1)) {
    const char err[] = "itoao(64) expected \"100\"";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "itoao basic";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_itoax_lower_upper(void) {
  char buf[32];

  int64_t v1 = 255;
  uint32_t n1 = itoax(&v1, buf, sizeof(buf), false);
  const char exp1[] = "ff";
  if (!buf_prefix_eq(buf, n1, exp1, sizeof(exp1) - 1)) {
    const char err[] = "itoax(255, lower) expected \"ff\"";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }

  int64_t v2 = 255;
  uint32_t n2 = itoax(&v2, buf, sizeof(buf), true);
  const char exp2[] = "FF";
  if (!buf_prefix_eq(buf, n2, exp2, sizeof(exp2) - 1)) {
    const char err[] = "itoax(255, upper) expected \"FF\"";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }

  const char ok[] = "itoax lower/upper";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_itoax_no_prefix(void) {
  char buf[32];
  int64_t v = 15;
  uint32_t n = itoax(&v, buf, sizeof(buf), true);
  const char exp[] = "F";
  if (!buf_prefix_eq(buf, n, exp, sizeof(exp) - 1)) {
    const char err[] = "itoax(15) expected \"F\" (no 0x prefix)";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "itoax no prefix";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_itoax_small_buffer(void) {
  char buf[1];
  int64_t v = 0xAB;
  uint32_t n = itoax(&v, buf, sizeof(buf), false);
  // Resumable behavior: should write as much as fits and leave remainder in v.
  if (n != sizeof(buf)) {
    const char err[] = "itoax partial write should fill provided buffer";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  if (v == 0) {
    const char err[] = "itoax partial write should leave remainder non-zero";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "itoax partial write leaves remainder";
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

static int test_utoa_basic(void) {
  char buf[32];
  uint64_t v = 12345;
  uint32_t n = utoa(&v, buf, sizeof(buf));
  const char exp[] = "12345";
  if (!buf_prefix_eq(buf, n, exp, sizeof(exp) - 1)) {
    const char err[] = "utoa(12345) expected \"12345\"";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "utoa basic";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_utoax_lower_upper(void) {
  char buf[32];
  uint64_t v1 = 255;
  uint32_t n1 = utoax(&v1, buf, sizeof(buf), false);
  const char exp1[] = "ff";
  if (!buf_prefix_eq(buf, n1, exp1, sizeof(exp1) - 1)) {
    const char err[] = "utoax(255, lower) expected \"ff\"";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  uint64_t v2 = 255;
  uint32_t n2 = utoax(&v2, buf, sizeof(buf), true);
  const char exp2[] = "FF";
  if (!buf_prefix_eq(buf, n2, exp2, sizeof(exp2) - 1)) {
    const char err[] = "utoax(255, upper) expected \"FF\"";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "utoax lower/upper";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_utoab_basic(void) {
  char buf[32];
  uint64_t v = 5;
  uint32_t n = utoab(&v, buf, sizeof(buf));
  const char exp[] = "101";
  if (!buf_prefix_eq(buf, n, exp, sizeof(exp) - 1)) {
    const char err[] = "utoab(5) expected \"101\"";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "utoab basic";
  report_ok(ok, sizeof(ok) - 1);
  return 0;
}

static int test_utoao_basic(void) {
  char buf[32];
  uint64_t v = 64;
  uint32_t n = utoao(&v, buf, sizeof(buf));
  const char exp[] = "100";
  if (!buf_prefix_eq(buf, n, exp, sizeof(exp) - 1)) {
    const char err[] = "utoao(64) expected \"100\"";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  const char ok[] = "utoao basic";
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
  rc |= test_itoa_int64_min_continuous_chunks();
  rc |= test_itoa_int64_min_noncontiguous_chunks();
  rc |= test_itoa_resumable_across_flushes();

  rc |= test_itoab_zero();
  rc |= test_itoab_basic();
  rc |= test_itoab_negative();
  rc |= test_itoao_basic();
  rc |= test_itoax_lower_upper();
  rc |= test_itoax_no_prefix();
  rc |= test_itoax_small_buffer();

  rc |= test_utoa_basic();
  rc |= test_utoab_basic();
  rc |= test_utoao_basic();
  rc |= test_utoax_lower_upper();

  rc |= test_atoi_zero();
  rc |= test_atoi_positive();
  rc |= test_atoi_negative();
  rc |= test_atoi_leading_whitespace();
  rc |= test_atoi_explicit_plus();
  rc |= test_atoi_stops_at_non_digit();
  rc |= test_atoi_empty();

  return rc;
}
