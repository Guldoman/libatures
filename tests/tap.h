#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
  TAP_RUN,
  TAP_SKIP,
  TAP_TODO,
} tap_directive;

typedef struct {
  const char* description;
  bool (*function)(void);
  tap_directive directive;
} tap_test;


#define tap_run_tests(tests)                               \
  do {                                                     \
    printf("TAP version 14\n");                            \
    size_t n_tests = (sizeof(tests)/sizeof(tests[0]));     \
    printf("1..%ld\n", n_tests);                           \
    for (size_t n = 0; n < n_tests; n++) {                 \
      const char* description = tests[n].description;      \
      bool result = true;                                  \
      if (tests[n].directive != TAP_SKIP) {                \
        result = tests[n].function();                      \
      }                                                    \
      if (!result) {                                       \
        printf("not ");                                    \
      }                                                    \
      printf("ok %ld", n+1);                               \
      if (description != NULL && description[0] != '\0') { \
        printf(" - %s", description);                      \
      }                                                    \
      switch (tests[n].directive) {                        \
        case TAP_SKIP:                                     \
          printf(" # SKIP");                               \
          break;                                           \
        case TAP_TODO:                                     \
          printf(" # TODO");                               \
          break;                                           \
        case TAP_RUN:                                      \
          break;                                           \
      }                                                    \
      printf("\n");                                        \
    }                                                      \
  } while (0)

#define tap_comment(...) printf("# " __VA_ARGS__);
