/**
 * Copyright 2018 Jean-Daniel Michaud
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * Reference:
 * gzip container: https://www.ietf.org/rfc/rfc1952.txt
 * DEFLATE compression method: https://www.ietf.org/rfc/rfc1951.txt
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <strings.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>

#include "debug.h"

// To quiet the pesky compiler
char *strndup(const char *s, size_t n);

#define BUFFER_SIZE 1024
#define NO_VALUE USHRT_MAX

#define GZIP_HEADER_SIZE  10
#define GZIP_MAGIC        0x8B1F
#define GZIP_DEFLATE_CM   0x08

#define DEFLATE_LITERAL_BLOCK_TYPE 0
#define DEFLATE_FIX_HUF_BLOCK_TYPE 1
#define DEFLATE_DYN_HUF_BLOCK_TYPE 2
#define DEFLATE_CODE_MAX_BIT_LENGTH 32
#define DEFLATE_ALPHABET_SIZE 288
#define DEFLATE_END_BLOCK_VALUE 256

// Can seem a little steep but according to
// https://tools.ietf.org/html/rfc1951#page-13, code lengths for dynamic
// dictionaries can be as long as 15 bits.
#define DYNAMIC_DICT_SIZE 65535

// https://tools.ietf.org/html/rfc1952#page-5
typedef struct header_s {
  uint16_t  magic;
  uint8_t   cm;
  uint8_t   flg;
  uint32_t  mtime;
  uint8_t   xfl;
  uint8_t   os;
} header_t;

typedef struct extra_header_s {
  uint16_t xlen;
  char *fname;
  char *fcomment;
  uint16_t crc16;
} extra_header_t;

typedef struct footer_s {
  uint32_t crc32;
  uint32_t isize;
} footer_t;

typedef struct metadata_s {
  header_t header;
  extra_header_t extra_header;
  ssize_t block_offset;
  footer_t footer;
} metadata_t;

// https://tools.ietf.org/html/rfc1951#page-10
typedef struct block_s {
  uint8_t bfinal:1;
  uint8_t btype:2;
  uint8_t *data;
} block_t;

#define FTEXT           1
#define FHCRC     (1 << 1)
#define FEXTRA    (1 << 2)
#define FNAME     (1 << 3)
#define FCOMMENT  (1 << 4)

const char *OS[14] = {
  "FAT filesystem (MS-DOS, OS/2, NT/Win32)",
  "Amiga",
  "VMS (or OpenVMS)",
  "Unix",
  "VM/CMS",
  "Atari TOS",
  "HPFS filesystem (OS/2, NT)",
  "Macintosh",
  "Z-System",
  "CP/M",
  "TOPS-20",
  "NTFS filesystem (NT)",
  "QDOS",
  "Acorn RISCOS",
};

// The static huffman alphabet for literals and length
// https://tools.ietf.org/html/rfc1951#page-12
typedef struct static_huffman_params_s {
  uint8_t code_lengths[DEFLATE_ALPHABET_SIZE];
  uint32_t next_codes[32];
} static_huffman_params_t;
// [8] * (144 - 0) + [9] * (256-144) + [7] * (280 - 256) + [8] * (288 - 280)
static_huffman_params_t static_huffman_params = {
  {
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8
  },
  { 0, 0, 0, 0, 0, 0, 0, 0b0000000, 0b00110000, 0b110010000 },
};

// The static huffman alphabet for distance
// https://tools.ietf.org/html/rfc1951#page-12
// As described in the RFC, "Distance codes 0-31 are represented by
// (fixed-length) 5-bit codes". So we don't really need a dictionary but we will
// still use one to keep coherent with the dynamic dictionary case.
#define DEFLATE_STATIC_DISTANCE_CODE_LENGTHS_SIZE 32
#define DEFLATE_SDCLS DEFLATE_STATIC_DISTANCE_CODE_LENGTHS_SIZE
uint8_t static_huffman_params_distance_code_lengths[DEFLATE_SDCLS] = {
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5
};

#define DEFLATE_LENGTH_EXTRA_BITS_ARRAY_SIZE 29
#define DEFLATE_LENGTH_EXTRA_BITS_ARRAY_OFFSET 257

uint16_t length_lookup[DEFLATE_LENGTH_EXTRA_BITS_ARRAY_SIZE] = {
  3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67,
  83, 99, 115, 131, 163, 195, 227, 258
};

// https://tools.ietf.org/html/rfc1951#page-12
uint8_t length_extra_bits[DEFLATE_LENGTH_EXTRA_BITS_ARRAY_SIZE] = {
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5,
  5, 5, 0
};

#define DEFLATE_DISTANCE_EXTRA_BITS_ARRAY_SIZE 30

uint16_t distance_lookup[DEFLATE_DISTANCE_EXTRA_BITS_ARRAY_SIZE] = {
  1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
  1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};

// https://tools.ietf.org/html/rfc1951#page-12
uint8_t distance_extra_bits[DEFLATE_DISTANCE_EXTRA_BITS_ARRAY_SIZE] = {
  0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11,
  11, 12, 12, 13, 13
};

// https://tools.ietf.org/html/rfc1951#page-14
#define CODE_LENGTHS_CODE_LENGTH 19 // yeah...
uint8_t code_length_code_alphabet[CODE_LENGTHS_CODE_LENGTH] = {
  16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};
uint8_t code_length_lengths_extra_size[CODE_LENGTHS_CODE_LENGTH] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 7
};
uint8_t code_length_lengths_extra_size_offset[CODE_LENGTHS_CODE_LENGTH] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 11
};

// For debugging purposes
uint8_t *g_buf = NULL;
uint8_t *g_output = NULL;

// Move the mask bit to the left up to 128, then reinit to 1 and increment ptr
#define INCREMENT_MASK(mask, ptr) \
  mask = mask << 1 | mask >> 7; \
  ptr += mask & 1;

// Retrieve multiple bits.
// 76543210 FEDCBA98
// ——▶——▶—— —————▶——
//  3 2  1    5   3
// result:
//  210 543 9876 FEDCB
#define READ(dest, mask, ptr, size) { \
  dest = 0; \
  uint32_t _pos = 1; \
  uint8_t _size = size; \
  while (_size--) { \
    dest |= (*ptr & mask ? _pos : 0); \
    _pos <<= 1; \
    INCREMENT_MASK(mask, ptr) \
  } \
}

typedef uint16_t *dict_t;

typedef struct dict_table_s {
  dict_t flat_lut;
  dict_t *table;
  uint16_t min_code_length;
} dict_table_t;

void usage() {
  fprintf(stderr, "usage: gzip <file>\n");
}

void print_metadata(metadata_t metadata) {
  fprintf(stdout, "magic: 0x%04x\n", metadata.header.magic);
  fprintf(stdout, "cm: 0x%02x (%s)\n",
    metadata.header.cm, metadata.header.cm == 8 ? "DEFLATE" : "");
  fprintf(stdout, "flg: ");
  fprintf(stdout, "%s", metadata.header.flg & FTEXT ? "FTEXT " : "");
  fprintf(stdout, "%s", metadata.header.flg & FHCRC ? "FHCRC " : "");
  fprintf(stdout, "%s", metadata.header.flg & FEXTRA ? "FEXTRA " : "");
  fprintf(stdout, "%s", metadata.header.flg & FNAME ? "FNAME " : "");
  fprintf(stdout, "%s", metadata.header.flg & FCOMMENT ? "FCOMMENT " : "");
  fprintf(stdout, "\n");
  long int time = metadata.header.mtime;
  fprintf(stdout, "mtime: %s", ctime(&time));
  fprintf(stdout, "xfl: 0x%02x\n", metadata.header.xfl);
  fprintf(stdout, "os: %s\n",
    metadata.header.os < 14 ? OS[metadata.header.os] : "unknown");

  if (metadata.header.flg & FNAME)
    fprintf(stdout, "filename: %s\n",
      metadata.extra_header.fname ? metadata.extra_header.fname : "");
  if (metadata.header.flg & FCOMMENT)
    fprintf(stdout, "comment: %s\n",
      metadata.extra_header.fcomment ? metadata.extra_header.fcomment : "");
  if (metadata.header.flg & FHCRC)
    fprintf(stdout, "crc16: 0x%02x\n", metadata.extra_header.crc16);

  fprintf(stdout, "block offset: %li bytes\n", metadata.block_offset);
  fprintf(stdout, "crc32: 0x%04x\n", metadata.footer.crc32);
  fprintf(stdout, "isize: %u bytes\n", metadata.footer.isize);
}

uint8_t *get_extra_header(uint8_t *buf, header_t header, extra_header_t *extra) {
  uint8_t *current = buf + GZIP_HEADER_SIZE;
  if (header.flg & FEXTRA) {
    uint16_t xlen = *current;
    extra->xlen = xlen;
    current += 2 + xlen;
  }
  if (header.flg & FNAME) {
    uint8_t *beg = current;
    while (*current++ != 0) ;
    extra->fname = strndup((const char *) beg, current - beg);
  }
  if (header.flg & FCOMMENT) {
    uint8_t *beg = current;
    while (*current++ != 0) ;
    extra->fcomment = strndup((const char *) beg, current - beg);
  }
  if (header.flg & FHCRC) {
    uint16_t crc16 = *current;
    extra->crc16 = crc16;
    current += 2;
  }
  return current;
}

void free_metadata(metadata_t *metadata) {
  if (metadata->extra_header.fname != NULL) free(metadata->extra_header.fname);
  if (metadata->extra_header.fcomment != NULL) free(metadata->extra_header.fcomment);
}

// https://tools.ietf.org/html/rfc1952#page-5
void get_metadata(uint8_t *buf, ssize_t size, metadata_t *metadata) {
  // Get header
  memcpy(&metadata->header, buf, GZIP_HEADER_SIZE);
  // Get extra header depeneding on xflg
  memset(&metadata->extra_header, 0, sizeof (extra_header_t));
  uint8_t *pos = get_extra_header(buf, metadata->header, &metadata->extra_header);
  // Footer
  metadata->footer.crc32 = buf[size - 8] | buf[size - 7] << 8 |
    buf[size - 6] << 16 | buf[size - 5] << 24;
  metadata->footer.isize = buf[size - 4] | buf[size - 3] << 8 |
    buf[size - 2] << 16 | buf[size - 1] << 24;

  // Sanity checks
  if (metadata->header.magic != GZIP_MAGIC) {
    fprintf(stderr, "error: incorrect magic number\n");
    exit(4);
  }
  if (metadata->header.cm != GZIP_DEFLATE_CM) {
    fprintf(stderr, "error: unknown compression method\n");
    exit(4);
  }

  metadata->block_offset = pos - buf;

  return;
}

/**
 * Generates a table which, for each length, will point to the proper offset
 * in a flat array (which represent the dictionary)
 * @param flat_lut        the allocated lookup table
 * @param lut_size        the lut size
 * @param table           the table to fillup
 * @param dict            the dict structure to fill up
 * @param max_code_length the maximum code length
 *
 * dict:
 * [0, 1, 2, 3, 4, 5, 6, 7, 8, X, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 21...]
 *  ^     ^           ^                           ^
 *  \__   |   _______/                           /
 *     \  |  /   _______________________________/
 * [0, 1, 2, 3, 4]
 *  |
 *  v
 * NULL
 *
 * So a code of length 3 with value 011 (3) will end up where the X is.
 */
void generate_dict_table(uint16_t *flat_lut, uint16_t lut_size, dict_t *table,
                         dict_table_t *dict, uint8_t max_code_length) {
  // Set the whole dict to NO_VALUE for now
  memset(flat_lut, NO_VALUE, lut_size * sizeof (uint16_t));
  // There will be no 0-length code
  table[0] = NULL;
  table[1] = &flat_lut[0];
  uint16_t index = 0;
  for (int i = 2; i <= max_code_length; ++i) {
    index += 1 << (i - 1);
    // printf("table[%u] -> &flat_lut[%u -> %u]\n", i, index,
    //   index + (1 << i) - 1);
    table[i] = &flat_lut[index];
  }
  dict->flat_lut = flat_lut;
  dict->table = table;
  dict->min_code_length = 0;
}

/**
 * Counts the number of code by length.
 * If { 2, 1, 3, 3 } represents the code lengths then there is one code of
 * length 2, 1 of length 1 and 2 of length 3. The function will fillup
 * bit_counts with the result this way: { 0, 1, 1, 2, 0 ...., 0 }.
 *
 * @params code_lengths is the array of code length
 * @params size is the size of code_lengths
 * @params length_counts is the resulting array. The array must be allocated
 * with a minimum size of DEFLATE_CODE_MAX_BIT_LENGTH.
 */
void count_by_code_length(const uint8_t *code_lengths, ssize_t size,
                          uint8_t *length_counts) {
  memset(length_counts, 0, DEFLATE_CODE_MAX_BIT_LENGTH * sizeof (uint8_t));
  for (; size > 0; --size) {
    length_counts[code_lengths[size - 1]]++;
  }
  // See https://tools.ietf.org/html/rfc1952#page-8
  length_counts[0] = 0;
}

/**
 * Generates the starting code for a specific code lengths.
 * if length_counts = { 0, 1, 1, 2 } the next_codes is { 0, 2, 6 }, representing
 * the following dictionary:
 * value code
 * ----- ----
 * A     10  -> A is encoded on two bits, first code is 2 (10)
 * B     0   -> B is encoded on one bit, first code is 0 (0)
 * C     110 -> C is encoded on three bits, first code is 110 (6)
 * D     111 -> D is encoded on three bits, code is 111 (6 + 1). This code is
 *              not present in next_codes as it is not the first code of its
 *              length
 * https://tools.ietf.org/html/rfc1951#page-8
 *
 * @param length_counts length_counts[N] is the number of code of length N.
 * Its size is DEFLATE_CODE_MAX_BIT_LENGTH.
 * @param next_codes  next_codes[N] is the first code of length N. It must be
 * allocated with a minimum size of DEFLATE_CODE_MAX_BIT_LENGTH.
 */
void generate_next_codes(uint8_t *length_counts, uint32_t *next_codes) {
  uint32_t code = 0;
  memset(next_codes, 0, DEFLATE_CODE_MAX_BIT_LENGTH * sizeof (uint32_t));
  for (uint8_t nbits = 1; nbits < DEFLATE_CODE_MAX_BIT_LENGTH; nbits++) {
    next_codes[nbits] = code = (code + length_counts[nbits - 1]) << 1;
  }
}

/**
 * Generates a 2-level lookup table for the dictionary.
 * @params code_lengths   is the array of code length
 * @params size           is the size of code_lengths
 * @param next_codes      next_codes[N] is the first code of length N. It must
 * be allocated with a minimum size of DEFLATE_CODE_MAX_BIT_LENGTH.
 * @params dict           the structure to fill up
 * For example:
 * { A: 010, B: 00, C: 10 }
 * is represented with this tree:
 *      x
 *   0/   \1
 *   x     x
 * 0/ \1 0/
 * B   x C
 *   0/
 *   A
 * which in turn is stored in this structure:
 * NULL
 *  |
 * [0, 1, 2, 3] <-- table
 *   _/   \_ \______________
 *  /       \               \
 *  v        v               v
 * [-1, -1,  B, -1,  C, -1, -1, -1,  A, -1] <-- lut
 *
 * (00  => 0 starting at index 2)
 * (010 => 2 starting at index 6)
 * (10  => 2 starting at index 2)
 */
void generate_dict(const uint8_t *code_lengths, ssize_t size,
                   uint32_t *next_codes, dict_table_t dict) {
  for (uint16_t i = 0; i < size; ++i) {
    uint8_t length = code_lengths[i];
    if (length == 0) continue;
    uint32_t code = next_codes[length];
    // printf("%u %s (%u)\n", i, tobin(code, length), length);
    // printf("dict_table[%u][%u] = %u \n", length, code, i);
    dict.table[length][code] = i;
    next_codes[code_lengths[i]]++;
    // Remember the smallest code length
    dict.min_code_length =
      dict.min_code_length < length ? dict.min_code_length : length;
  }
}

/**
 * Shorthand function.
 */
void generate_dict_from_code_length(const uint8_t *code_lengths,
                                    size_t length_counts_size,
                                    dict_table_t dict) {
  uint8_t length_counts[DEFLATE_CODE_MAX_BIT_LENGTH];
  count_by_code_length(code_lengths, length_counts_size, length_counts);

  uint32_t next_codes[DEFLATE_CODE_MAX_BIT_LENGTH];
  generate_next_codes(length_counts, next_codes);

  generate_dict(code_lengths, length_counts_size, next_codes, dict);
}

/**
 * Dynamic dictionaries are using special encoding rules.
 * https://tools.ietf.org/html/rfc1951#page-13.
 */
void decode_dynamic_dict_lengths(uint8_t **buf, uint8_t *mask, size_t output_size,
                                 dict_table_t dict, uint8_t *output) {
  uint16_t value = 0;

  while (output_size) {
    uint16_t code_length = 0;
    uint16_t code = 0;
    do {
      code_length++;
      code <<= 1;
      code += (**buf & *mask) != 0;
      // printf("%u", **buf & *mask ? 1 : 0);
      INCREMENT_MASK(*mask, *buf);
    } while ((value = dict.table[code_length][code]) == NO_VALUE);
    // printf("\n");
    // A little complicated dance here...
    // https://tools.ietf.org/html/rfc1951#page-13
    if (value < 16) {
      // Between 0 and 15, we just copy the value
      *output++ = value;
      output_size--;
    } else {
      if (value == 16) {
        // 16 we copy the last value according to the 2 next bits + 3
        uint8_t extra = 0;
        READ(extra, *mask, *buf, 2);
        uint8_t last_value = *(output - 1);
        for (int i = 0; i < extra + 3; ++i) {
          *output++ = last_value;
          output_size--;
        }
      } else {
        // 17 or 18, we append 0 according to the extra bits
        uint8_t extra = 0;
        uint8_t extra_size = code_length_lengths_extra_size[value];
        READ(extra, *mask, *buf, extra_size);
        for (int i = 0; i < extra + code_length_lengths_extra_size_offset[value]; ++i) {
          *output++ = 0;
          output_size--;
        }
      }
    }
  }
}

/**
 * Decode the dynamic code and generate a dictionary.
 * https://tools.ietf.org/html/rfc1951#page-13.
 */
void parse_dynamic_tree(uint8_t **buf, uint8_t *mask,
                        dict_table_t litdict, dict_table_t distdict) {
  // First read HLEN (4 bits), HDIST (5 bits) and HLIT (5 bits)
  uint8_t hlen;
  uint8_t hdist;
  uint8_t hlit;
  READ(hlit, *mask, *buf, 5);
  READ(hdist, *mask, *buf, 5);
  READ(hlen, *mask, *buf, 4);
  // printf("hlen %u hdist %u hlit %u\n", hlen + 4, hdist + 1, hlit + 257);
  // Read HLEN + 4 code length codes.
  uint8_t code_length_lengths[CODE_LENGTHS_CODE_LENGTH];
  memset(code_length_lengths, 0, CODE_LENGTHS_CODE_LENGTH * sizeof (uint8_t));
  for (uint8_t i = 0; i < hlen + 4; ++i) {
    // Warning: wasted a lot of time on this:
    // As the code length code are presented is a unsorted order, they need to
    // be reordered because the RFC specifies that codes are supposed to be of
    // consecutive values: See https://tools.ietf.org/html/rfc1951#page-7.
    // So if you have a code length of 3 for 4 (starting at 100 for example) and
    // 3 for 8 and 6. You might, like me, naively compute the code as follow:
    // 100 -> 4; 101 -> 8; 110 -> 6.
    // And you'd be very wrong indeed because, as the RFC says, you would have
    // to increase the code according to the order of the value. Then what you
    // should have is:
    // 100 -> 4; 101 -> 6; 110 -> 8.
    READ(code_length_lengths[code_length_code_alphabet[i]], *mask, *buf, 3);
  }
  // Generate dictionary from code length codes
  uint16_t code_length_lut[256];
  dict_t code_length_dict_table[16];
  dict_table_t code_length_dict;
  generate_dict_table(code_length_lut, 256, code_length_dict_table,
    &code_length_dict, 15);
  generate_dict_from_code_length(code_length_lengths, CODE_LENGTHS_CODE_LENGTH,
    code_length_dict);
  // Read the HLIT + 257 code length for the literal/length dynamic dictionary
  uint8_t literal_lengths[287];
  memset(literal_lengths, 0, 287 * sizeof (uint8_t));
  decode_dynamic_dict_lengths(buf, mask, hlit + 257, code_length_dict,
    literal_lengths);
  // Read the HDIST + 1 code length for the distance dynamic dictionary
  uint8_t distance_lengths[32];
  memset(distance_lengths, 0, 32 * sizeof (uint8_t));
  decode_dynamic_dict_lengths(buf, mask, hdist + 1, code_length_dict,
    distance_lengths);
  // Generates the dynamic dictionary
  generate_dict_from_code_length(literal_lengths, hlit + 257, litdict);
  generate_dict_from_code_length(distance_lengths, hdist + 1, distdict);
}

// TODO: break this function down into smaller functions
uint8_t * inflate_block(uint8_t **buf, uint8_t *mask,
                        dict_table_t litdict, dict_table_t distdict,
                        uint8_t *output) {
  uint16_t value = 0;

  while (value != DEFLATE_END_BLOCK_VALUE) {
    uint8_t code_length = 0;
    uint16_t code = 0;
    do {
      code_length++;
      code <<= 1;
      code += (**buf & *mask) != 0;
      // printf("litdict[%u][%u] == %u\n", length, code, litdict[length][code]);
      // printf("%u", **buf & *mask ? 1 : 0);
      INCREMENT_MASK(*mask, *buf);
    } while ((value = litdict.table[code_length][code]) == NO_VALUE);
    // printf("\n");
    // printf("found %u\n", value);
    if (value < DEFLATE_END_BLOCK_VALUE) {
      *output++ = value;
    }
    if (value > DEFLATE_END_BLOCK_VALUE) {
      uint16_t length = length_lookup[value - DEFLATE_END_BLOCK_VALUE - 1];
      // printf("length %u\n", length);
      // length code
      uint8_t nb_extra_bits = length_extra_bits[value - DEFLATE_END_BLOCK_VALUE - 1];
      uint16_t extra_bits = 0;
      READ(extra_bits, *mask, *buf, nb_extra_bits);
      // printf("extra_bits %u\n", extra_bits);
      length += extra_bits;
      // Now read the distance
      code = 0;
      code_length = 0;
      do {
        code_length++;
        code <<= 1;
        code += (**buf & *mask) != 0;
        INCREMENT_MASK(*mask, *buf);
      } while ((value = distdict.table[code_length][code]) == NO_VALUE);
      uint16_t distance = distance_lookup[value];
      nb_extra_bits = distance_extra_bits[value];
      extra_bits = 0;
      READ(extra_bits, *mask, *buf, nb_extra_bits);
      distance += extra_bits;
      // printf("length %u distance %u\n", length, distance);
      if (length > distance) {
        while (length--) {
          *output = *(output - distance);
          ++output;
        }
      } else {
        // memcpy to go a little faster
        memcpy(output, output - distance, length);
        output += length;
      }
    }
  }
  return output;
}

void inflate(uint8_t *buf, uint8_t *output) {
  g_buf = buf; // for debugging purposes
  g_output = output; // for debugging purposes
  // Generate the static huffman dictionary for literals/lengths
  uint16_t static_lut[1024];
  dict_t static_dict_table[10];
  dict_table_t static_dict;
  generate_dict_table(static_lut, 1024, static_dict_table, &static_dict, 9);
  generate_dict_from_code_length(static_huffman_params.code_lengths,
    DEFLATE_ALPHABET_SIZE, static_dict);
  // Generate the static huffman dictionary for distances
  uint16_t distance_static_dict[64];
  dict_t static_distance_dict_table[6];
  dict_table_t static_distance_dict;
  generate_dict_table(distance_static_dict, 64, static_distance_dict_table,
    &static_distance_dict, 5);
  generate_dict_from_code_length(static_huffman_params_distance_code_lengths,
    DEFLATE_SDCLS, static_distance_dict);

  uint8_t bfinal = 0; // 1 if this is the final block
  uint8_t *current_buf = buf; // the pointer to the current position in the buffer
  uint8_t mask = 1; // the integer used as mask to read bit by bit
  uint8_t *current_output = output; // the pointer to the current positionin the output
  do {
    READ(bfinal, mask, current_buf, 1);
    // Anything that is not inside the block is read from left to right.
    // See https://tools.ietf.org/html/rfc1951#page-6
    uint8_t btype = 0; // The buffer type
    READ(btype, mask, current_buf, 2);

    switch (btype) {
      case DEFLATE_LITERAL_BLOCK_TYPE: {
        // printf("DEFLATE_LITERAL_BLOCK_TYPE\n");
        // https://tools.ietf.org/html/rfc1951#page-11
        // Uncompressed block starts on the next byte
        if (mask != 1) current_buf++; // Only increment if we haven't done it before
        uint16_t len = *current_buf | *(current_buf + 1) << 8;
        current_buf += 4; // Skiping 4 bytes (LEN and NLEN)
        memcpy(current_output, current_buf, len * sizeof (uint8_t));
        current_buf += len;
        current_output += len;
        mask = 1;
        break;
      }
      case DEFLATE_FIX_HUF_BLOCK_TYPE: {
        // printf("DEFLATE_FIX_HUF_BLOCK_TYPE\n");
        current_output = inflate_block(&current_buf, &mask, static_dict,
          static_distance_dict, current_output);
        break;
      }
      case DEFLATE_DYN_HUF_BLOCK_TYPE: {
        // printf("DEFLATE_DYN_HUF_BLOCK_TYPE\n");
        uint16_t lut[DYNAMIC_DICT_SIZE];
        dict_t dict_table[16];
        dict_table_t dict;
        generate_dict_table(lut, DYNAMIC_DICT_SIZE, dict_table, &dict, 15);

        uint16_t dist_lut[DYNAMIC_DICT_SIZE];
        dict_t dist_dict_table[16];
        dict_table_t dist_dict;
        generate_dict_table(dist_lut, DYNAMIC_DICT_SIZE, dist_dict_table,
          &dist_dict, 15);

        parse_dynamic_tree(&current_buf, &mask, dict, dist_dict);
        current_output = inflate_block(&current_buf, &mask, dict, dist_dict,
          current_output);
        break;
      }
    }
  } while (bfinal != 1);
}
