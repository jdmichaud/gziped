#include "gziped.h"

#define FAIL() { \
  ++totalres; \
  fprintf(stderr, "%s failed at %s(%i)\n", __func__, __FILE__, __LINE__); \
}

uint8_t test_count_by_code_length() {
  uint8_t totalres = 0;

  uint8_t code_lengths[8] = { 3, 3, 3, 3, 3, 2, 4, 4 };
  uint8_t bit_counts[32];
  count_by_code_length(code_lengths, 8, bit_counts);
  if (bit_counts[1] != 0 || bit_counts[2] != 1 ||
    bit_counts[3] != 5 || bit_counts[4] != 2 || bit_counts[5] != 0) FAIL();

  return totalres;
}

uint8_t test_generate_next_codes() {
  uint8_t totalres = 0;

  uint8_t bit_counts[32];
  memset(bit_counts, 0, 32);
  bit_counts[2] = 1; bit_counts[3] = 5; bit_counts[4] = 2;
  uint32_t next_codes[32];
  memset(next_codes, 0, 32 * sizeof (uint32_t));
  generate_next_codes(bit_counts, next_codes);
  if (next_codes[0] != 0 || next_codes[1] != 0 ||
    next_codes[2] != 0 || next_codes[3] != 2 || next_codes[4] != 14) FAIL();

  return totalres;
}

uint8_t test_tobin() {
  uint8_t totalres = 0;

  if (tobin(0, 0) != NULL) FAIL();
  if (strcmp(tobin(0, 1), "0") != 0) FAIL();
  if (strcmp(tobin(1, 1), "1") != 0) FAIL();
  if (strcmp(tobin(16, 1), "0") != 0) FAIL();
  if (strcmp(tobin(16, 5), "10000") != 0) FAIL();
  if (strcmp(tobin(255, 8), "11111111") != 0) FAIL();
  if (strcmp(tobin(48, 8), "00110000") != 0) FAIL();

  return totalres;
}

uint8_t test_generate_dict() {
  uint8_t totalres = 0;

  uint8_t code_lengths[8] = { 3, 3, 3, 3, 3, 2, 4, 4 };
  uint32_t next_codes[32];
  memset(next_codes, 0, 32 * sizeof (uint32_t));
  next_codes[3] = 2; next_codes[4] = 14;
  char *dict[8];
  generate_dict(code_lengths, 8, next_codes, dict);
  if (strcmp(dict[0], "010") != 0) FAIL();
  if (strcmp(dict[1], "011") != 0) FAIL();
  if (strcmp(dict[2], "100") != 0) FAIL();
  if (strcmp(dict[3], "101") != 0) FAIL();
  if (strcmp(dict[4], "110") != 0) FAIL();
  if (strcmp(dict[5], "00") != 0) FAIL();
  if (strcmp(dict[6], "1110") != 0) FAIL();
  if (strcmp(dict[7], "1111") != 0) FAIL();

  free_dict(dict, 8);
  return totalres;
}

uint8_t test_static_dict() {
  uint8_t totalres = 0;

  uint8_t bit_counts[32];
  count_by_code_length(static_huffman_params.code_lengths,
    DEFLATE_ALPHABET_SIZE, bit_counts);

  uint32_t next_codes[32];
  memset(next_codes, 0, 32 * sizeof (uint32_t));
  generate_next_codes(bit_counts, next_codes);

  for (int i = 0; i < 10; ++i) {
    if (next_codes[i] != static_huffman_params.next_codes[i]) FAIL()
  }

  char *static_dict[DEFLATE_ALPHABET_SIZE];
  generate_dict(static_huffman_params.code_lengths, DEFLATE_ALPHABET_SIZE,
    static_huffman_params.next_codes, static_dict);

  if (strcmp(static_dict[0], "00110000") != 0) FAIL();
  if (strcmp(static_dict[143], "10111111") != 0) FAIL();
  if (strcmp(static_dict[144], "110010000") != 0) FAIL();
  if (strcmp(static_dict[255], "111111111") != 0) FAIL();
  if (strcmp(static_dict[256], "0000000") != 0) FAIL();
  if (strcmp(static_dict[279], "0010111") != 0) FAIL();
  if (strcmp(static_dict[280], "11000000") != 0) FAIL();
  if (strcmp(static_dict[287], "11000111") != 0) FAIL();

  free_dict(static_dict, DEFLATE_ALPHABET_SIZE);
  return totalres;
}

int main(int argc, char **argv) {
  uint8_t totalres = 0;

  totalres += test_count_by_code_length();
  totalres += test_generate_next_codes();
  totalres += test_tobin();
  totalres += test_generate_dict();
  totalres += test_static_dict();

  return totalres;
}