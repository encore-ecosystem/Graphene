/* Exact, locale-independent JSON/editor scalar conversion. Keep lexical policy
 * here: no whitespace, hexadecimal literals, NaN, infinity or trailing bytes.
 */
#include <locale.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
static float graphene_json_strtof(const char *text, char **end) {
  _locale_t locale = _create_locale(LC_NUMERIC, "C");
  if (!locale)
    return NAN;
  float value = _strtof_l(text, end, locale);
  _free_locale(locale);
  return value;
}
#else
#include <pthread.h>
/* Some native builds include libc headers before enabling GNU declarations. */
extern float strtof_l(const char *, char **, locale_t);
static pthread_once_t graphene_json_once = PTHREAD_ONCE_INIT;
static locale_t graphene_json_locale;
static void graphene_json_initialize(void) {
  graphene_json_locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
}
static float graphene_json_strtof(const char *text, char **end) {
  pthread_once(&graphene_json_once, graphene_json_initialize);
  return graphene_json_locale ? strtof_l(text, end, graphene_json_locale) : NAN;
}
#endif
float graphene_json_number(encore_str source) {
  size_t n = source.object ? source.object->len : 0;
  if (!n || n > 512)
    return NAN;
  const char *s = source.object->data;
  size_t i = 0, digits = 0;
  if (s[i] == '-')
    i++;
  while (i < n && s[i] >= '0' && s[i] <= '9') {
    i++;
    digits++;
  }
  if (i < n && s[i] == '.') {
    i++;
    while (i < n && s[i] >= '0' && s[i] <= '9') {
      i++;
      digits++;
    }
  }
  if (!digits)
    return NAN;
  if (i < n && (s[i] == 'e' || s[i] == 'E')) {
    i++;
    if (i < n && (s[i] == '-' || s[i] == '+'))
      i++;
    size_t exponent_digits = 0;
    unsigned exponent = 0;
    while (i < n && s[i] >= '0' && s[i] <= '9') {
      exponent = exponent * 10 + (unsigned)(s[i] - '0');
      if (exponent > 1000)
        return NAN;
      i++;
      exponent_digits++;
    }
    if (!exponent_digits)
      return NAN;
  }
  if (i != n)
    return NAN;
  char text[513];
  memcpy(text, s, n);
  text[n] = 0;
  char *end = NULL;
  float value = graphene_json_strtof(text, &end);
  return end == text + n && isfinite(value) ? value : NAN;
}
