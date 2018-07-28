#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>
#include <stdint.h>

#define DEBUG(str) fprintf(stdout, #str"\n");
#define DVAR(var) fprintf(stdout, #var": %i\n", var);

void binprint(uint8_t u) {
  fprintf(stdout, "%i", u & 128 ? 1 : 0);
  fprintf(stdout, "%i", u & 64 ? 1 : 0);
  fprintf(stdout, "%i", u & 32 ? 1 : 0);
  fprintf(stdout, "%i", u & 16 ? 1 : 0);
  fprintf(stdout, "%i", u & 8 ? 1 : 0);
  fprintf(stdout, "%i", u & 4 ? 1 : 0);
  fprintf(stdout, "%i", u & 2 ? 1 : 0);
  fprintf(stdout, "%i", u & 1 ? 1 : 0);
  fprintf(stdout, "\n");
}

char* itoa(int value, char* str, int radix) {
  static char dig[] =
    "0123456789"
    "abcdefghijklmnopqrstuvwxyz";
  int n = 0, neg = 0;
  unsigned int v;
  char* p, *q;
  char c;
  if (radix == 10 && value < 0) {
    value = -value;
    neg = 1;
  }
  v = value;
  do {
    str[n++] = dig[v%radix];
    v /= radix;
  } while (v);
  if (neg)
    str[n++] = '-';
  str[n] = '\0';
  for (p = str, q = p + (n-1); p < q; ++p, --q)
    c = *p, *p = *q, *q = c;
  return str;
}

char *tobin(uint32_t code, uint8_t length) {
  if (length == 0) return NULL;
  char *s = (char *) malloc(sizeof (char) * length + 1);
  s[length] = 0;
  for (uint8_t i = 0; i < length; ++i) {
    s[length - i - 1] = code & (1 << i) ? '1' : '0';
  }
  return s;
}

#endif // __DEBUG_H__