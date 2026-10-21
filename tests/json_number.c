#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  size_t ref_count, len;
  char data[513];
} encore_str_object;
typedef struct {
  encore_str_object *object;
} encore_str;
#include "../native/json_number.c"
static float number(const char *s) {
  encore_str_object text = {0};
  text.len = strlen(s);
  memcpy(text.data, s, text.len);
  return graphene_json_number((encore_str){&text});
}
int main(void) {
  uint32_t rng = 17;
  for (unsigned i = 0; i < 200000; i++) {
    rng = rng * 1664525u + 1013904223u;
    float before;
    memcpy(&before, &rng, 4);
    if (!isfinite(before))
      continue;
    char text[64];
    snprintf(text, sizeof text, i%2?"%.9g":"%.17g", before);
    float after = number(text);
    assert(!memcmp(&before, &after, 4));
  }
  const char *bad[] = {"",      "-",   ".",   "e1",       "1e",
                       "1e+",   "1x",  " 1",  "1 ",       "+1",
                       "0x1p2", "nan", "inf", "1e999999", "3.5e38"};
  for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++)
    assert(isnan(number(bad[i])));
  assert(number(".5") == .5f && number("1.") == 1);
  puts("JSON scalar: 200000 bit-exact float round trips and malformed-number "
       "cases passed");
}
