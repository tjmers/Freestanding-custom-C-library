#include "../intf/stdio.h"

// These will print to the console and be verified manually, since the functionality to open a file is not yet available through the library.


void test_ints();

int main() {
  test_ints();
  
  return 0;
}


void test_ints() {
  printf("Today is the %d of the %d day.\n", 12, -11);
  // Should print This is the 12 of the -11 day.
}