#include "unity.h"
#include "fixture.h"
#include <stdio.h>

static void run_all_tests(void) {
  RUN_TEST_GROUP(math);
  RUN_TEST_GROUP(sector);
}

int main(int argc, const char *argv[]) {
  return UnityMain(argc, argv, run_all_tests);
}