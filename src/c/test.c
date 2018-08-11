#include "gziped.h"
#include "debug.h"

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

  generate_dict(code_lengths, 8, next_codes, dict, 32);

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

  uint16_t static_dict[1024];
  memset(static_dict, -1, 1024 * sizeof (uint16_t));
  generate_dict(static_huffman_params.code_lengths, DEFLATE_ALPHABET_SIZE,
    static_huffman_params.next_codes, static_dict, 1024);

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

// Base on an example from
// http://www.infinitepartitions.com/art001.html?_sm_nck=1
uint8_t test_code_length_dict() {
  uint8_t totalres = 0;

  uint8_t code_lengths[19] = {
    3, 0, 0, 0, 4, 4, 3, 2, 3, 3, 4, 5, 0, 0, 0, 0, 6, 7, 7
  };
  uint8_t length_counts[32];
  bzero(length_counts, 32);
  count_by_code_length(code_lengths, 19, length_counts);

  uint32_t next_codes[32];
  bzero(next_codes, 32 * sizeof (uint32_t));
  generate_next_codes(length_counts, next_codes);

  uint16_t code_length_dict[256];
  memset(code_length_dict, -1, 256 * sizeof (uint16_t));
  generate_dict(code_lengths, 19, next_codes, code_length_dict, 256);

  // 010: 0
  // 1100: 4
  // 1101: 5
  // 011: 6
  // 00: 7
  // 100: 8
  // 101: 9
  // 1110: 10
  // 11110: 11
  // 111110: 16
  // 1111110: 17
  // 1111111: 18

  if (code_length_dict[9] != 0) FAIL();
  if (code_length_dict[27] != 4) FAIL();
  if (code_length_dict[28] != 5) FAIL();
  if (code_length_dict[10] != 6) FAIL();
  if (code_length_dict[3] != 7) FAIL();
  if (code_length_dict[11] != 8) FAIL();
  if (code_length_dict[12] != 9) FAIL();
  if (code_length_dict[29] != 10) FAIL();
  if (code_length_dict[61] != 11) FAIL();
  if (code_length_dict[125] != 16) FAIL();
  if (code_length_dict[253] != 17) FAIL();
  if (code_length_dict[254] != 18) FAIL();

  return totalres;
}

uint8_t test_READ_INV() {
  uint8_t totalres = 0;

  // buffer: 11101010 11000011 10100010
  // order:  76543210    ...98
  uint8_t buffer[3] = { 234, 195, 162 };
  uint8_t *ptr = buffer;
  uint8_t mask = 1;

  uint8_t dest = 255;

  READ_INV(dest, mask, ptr, 4);
  if (dest != 0b0101) FAIL();

  READ_INV(dest, mask, ptr, 4);
  if (dest != 0b0111) FAIL();

  READ_INV(dest, mask, ptr, 2);
  if (dest != 0b11) FAIL();

  READ_INV(dest, mask, ptr, 4);
  if (dest != 0b0000) FAIL();

  READ_INV(dest, mask, ptr, 2);
  if (dest != 0b11) FAIL();

  READ_INV(dest, mask, ptr, 3);
  if (dest != 0b010) FAIL();

  READ_INV(dest, mask, ptr, 1);
  if (dest != 0b0) FAIL();

  READ_INV(dest, mask, ptr, 1);
  if (dest != 0b0) FAIL();

  READ_INV(dest, mask, ptr, 3);
  if (dest != 0b101) FAIL();


  return totalres;
}

uint8_t test_READ() {
  uint8_t totalres = 0;

  // buffer: 11101010 11000011 10100010
  // order:  76543210    ...98
  uint8_t buffer[3] = { 234, 195, 162 };
  uint8_t *ptr = buffer;
  uint8_t mask = 1;

  uint8_t dest = 255;

  READ(dest, mask, ptr, 4);
  if (dest != 0b1010) FAIL();

  READ(dest, mask, ptr, 8);
  if (dest != 0b00111110) FAIL();

  READ(dest, mask, ptr, 2);
  if (dest != 0b00) FAIL();

  READ(dest, mask, ptr, 2);
  if (dest != 0b11) FAIL();

  READ(dest, mask, ptr, 4);
  if (dest != 0b0010) FAIL();

  READ(dest, mask, ptr, 1);
  if (dest != 0b0) FAIL();

  READ(dest, mask, ptr, 3);
  if (dest != 0b101) FAIL();

  return totalres;
}

uint8_t test_distance_static_dictionary() {
  uint8_t totalres = 0;

  uint16_t distance_static_dict[64];
  memset(distance_static_dict, -1, 64 * sizeof (uint16_t));
  generate_dict_from_code_length(static_huffman_params_distance_code_lengths,
    DEFLATE_STATIC_DISTANCE_CODE_LENGTHS_SIZE, distance_static_dict, 32);

  if (distance_static_dict[31 + 0] != 0) FAIL();
  if (distance_static_dict[31 + 1] != 1) FAIL();
  if (distance_static_dict[31 + 2] != 2) FAIL();
  if (distance_static_dict[31 + 10] != 10) FAIL();
  if (distance_static_dict[31 + 13] != 13) FAIL();
  if (distance_static_dict[31 + 31] != 31) FAIL();

  return totalres;
}

uint8_t test_ndecode() {
  uint8_t totalres = 0;

  // 00101100 01101001 10100111 00100010
  // 00 (3) -> 42
  // 110 (13) -> 66
  // 100 (11) -> 7
  // 101 (12) -> 88
  // 01 (4) -> 5
  uint8_t *input = malloc(4 * sizeof (uint8_t));
  input[0] = 44; input[1] =  105; input[2] = 167; input[3] = 34;
  uint8_t mask = 1;
  uint8_t input_size = 13;
  uint16_t dict[16] = {
  // 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
    -1, -1, -1, 42,  5, -1, -1, -1, -1, -1, -1,  7, 88, 66, -1, -1
  };
  uint8_t output[13];

  ndecode(&input, &mask, input_size, dict, output);

  if (output[0 ] != 42) FAIL();
  if (output[1 ] != 66) FAIL();
  if (output[2 ] != 7) FAIL();
  if (output[3 ] != 7) FAIL();
  if (output[4 ] != 88) FAIL();
  if (output[5 ] != 88) FAIL();
  if (output[6 ] != 66) FAIL();
  if (output[7 ] != 5) FAIL();
  if (output[8 ] != 5) FAIL();
  if (output[9 ] != 5) FAIL();
  if (output[10] != 42) FAIL();
  if (output[11] != 5) FAIL();
  if (output[12] != 42) FAIL();

  return totalres;
}

int main(int argc, char **argv) {
  uint8_t totalres = 0;

  totalres += test_count_by_code_length();
  totalres += test_generate_next_codes();
  totalres += test_tobin();
  totalres += test_generate_dict();
  totalres += test_static_dict();
  totalres += test_code_length_dict();
  totalres += test_READ_INV();
  totalres += test_READ();
  totalres += test_distance_static_dictionary();
  totalres += test_ndecode();

  return totalres;
}