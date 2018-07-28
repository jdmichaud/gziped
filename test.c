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
  bzero(next_codes, 32 * sizeof (uint32_t));
  next_codes[3] = 2; next_codes[4] = 14;

  uint16_t dict[32];
  memset(dict, -1, 32 * sizeof (uint16_t));

  generate_dict(code_lengths, 8, next_codes, dict);

  if (dict[3]  != 5) FAIL(); // F: "00"
  if (dict[9]  != 0) FAIL(); // A: "010"
  if (dict[10] != 1) FAIL(); // B: "011"
  if (dict[11] != 2) FAIL(); // C: "100"
  if (dict[12] != 3) FAIL(); // D: "101"
  if (dict[13] != 4) FAIL(); // E: "110"
  if (dict[29] != 6) FAIL(); // G: "1110"
  if (dict[30] != 7) FAIL(); // H: "1111"

  return totalres;
}

uint8_t test_static_dict() {
  uint8_t totalres = 0;

  uint8_t bit_counts[32];
  count_by_code_length(static_huffman_params.code_lengths,
    DEFLATE_ALPHABET_SIZE, bit_counts);

  uint32_t next_codes[32];
  bzero(next_codes, 32 * sizeof (uint32_t));
  generate_next_codes(bit_counts, next_codes);

  for (int i = 0; i < 10; ++i) {
    if (next_codes[i] != static_huffman_params.next_codes[i]) FAIL()
  }

  uint16_t static_dict[512];
  memset(static_dict, -1, 512 * sizeof (uint16_t));
  generate_dict(static_huffman_params.code_lengths, DEFLATE_ALPHABET_SIZE,
    static_huffman_params.next_codes, static_dict);

  // Python to generate index in binary heap based on huffman code
  // def c(a):
  //   res = 0
  //   for y in a:
  //     res <<= 1
  //     res += 1 if int(y) == 0 else 2
  //   return str(res)
  if (static_dict[303] != 0) FAIL();   // 0:   "00110000"
  if (static_dict[446] != 143) FAIL(); // 143: "10111111"
  if (static_dict[911] != 144) FAIL(); // 144: "110010000"
  if (static_dict[1022] != 255) FAIL(); // 255: "111111111"
  if (static_dict[127] != 256) FAIL(); // 256: "0000000"
  if (static_dict[150] != 279) FAIL(); // 279: "0010111"
  if (static_dict[447] != 280) FAIL(); // 280: "11000000"
  if (static_dict[454] != 287) FAIL(); // 287: "11000111"

  return totalres;
}

int main(int argc, char **argv) {
  uint8_t totalres = 0;

  // totalres += test_count_by_code_length();
  // totalres += test_generate_next_codes();
  // totalres += test_tobin();
  // totalres += test_generate_dict();
  totalres += test_static_dict();

  return totalres;
}