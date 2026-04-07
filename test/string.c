#include "../intf/string.h"

#include "../intf/assert.h"
#include "../intf/stddef.h"
#include "../intf/stdio.h"
#include "../intf/stdlib.h"

void test_strtokc_given_strcmp();
void test_strtokc_given_strcmp_different_delimiters();
void test_strdup();

int main() {
  test_strtokc_given_strcmp();
  test_strtokc_given_strcmp_different_delimiters();
  test_strdup();
  return 0;
}


void test_strtokc_given_strcmp() {
  char str[] = "Hello, this is a test case.";
  char* s = strtokc(str, ' ');
  assert(strcmp(s, "Hello,") == 0);
  assert(!strtok_end());

  s = strtokc(NULL, ' ');
  assert(strcmp(s, " this") == 0);
  assert(!strtok_end());

  s = strtokc(NULL, ' ');
  assert(strcmp(s, " is") == 0);
  assert(!strtok_end());

  s = strtokc(NULL, ' ');
  assert(strcmp(s, " a") == 0);
  assert(!strtok_end());

  s = strtokc(NULL, ' ');
  assert(strcmp(s, " test") == 0);
  assert(!strtok_end());

  s = strtokc(NULL, ' ');
  assert(strcmp(s, " case.") == 0);
  assert(strtok_end());
}


void test_strtokc_given_strcmp_different_delimiters() {
  char str[] = "The temperature today is %f degrees celcuis (%f degrees F).\n";
  char* s = strtokc(str, '%');
  assert(strcmp(s, "The temperature today is ") == 0);
  assert(!strtok_end());

  s = strtokc(NULL, ' ');
  assert(strcmp(s, "%f") == 0);
  assert(!strtok_end());

  s = strtokc(NULL, '%');
  assert(strcmp(s, " degrees celcuis (") == 0);
  assert(!strtok_end());

  s = strtokc(NULL, ' ');
  assert(strcmp(s, "%f") == 0);
  assert(!strtok_end());

  s = strtokc(NULL, '%');
  assert(strcmp(s, " degrees F).\n") == 0);
  assert(strtok_end());
}


void test_strdup() {
  const char str1[] = "Hello, World!";
  const char str2[] = "";
  const char str3[] = "a";

  char* copy1 = strdup(str1);
  char* copy2 = strdup(str2);
  char* copy3 = strdup(str3);

  assert(strcmp(str1, copy1) == 0);
  assert(strcmp(str2, copy2) == 0);
  assert(strcmp(str3, copy3) == 0);

  free(copy1);
  free(copy2);
  free(copy3);
}