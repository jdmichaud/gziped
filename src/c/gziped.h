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
  if ((mask = (mask = mask << 1) ? mask : 1) == 1) ++ptr;

// Retrieve multiple bits in inverse order.
// 76543210 FEDCBA98
// ◀——————— ◀———————
//     1        2
// result:
//  01234567 89ABCDEF
#define READ_INV(dest, mask, ptr, size) { \
  dest = 0; \
  uint8_t _size = size; \
  while (_size--) { \
    dest <<= 1; \
    dest += (*ptr & mask ? 1 : 0); \
    INCREMENT_MASK(mask, ptr) \
  } \
}

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
  bzero(length_counts, DEFLATE_CODE_MAX_BIT_LENGTH * sizeof (uint8_t));
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
  bzero(next_codes, DEFLATE_CODE_MAX_BIT_LENGTH * sizeof (uint32_t));
  for (uint8_t nbits = 1; nbits <= DEFLATE_CODE_MAX_BIT_LENGTH; nbits++) {
    next_codes[nbits] = code = (code + length_counts[nbits - 1]) << 1;
  }
}

/**
 * Generates an containg the mapped value.
 * @params code_lengths is the array of code length
 * @params size is the size of code_lengths
 * @param next_codes  next_codes[N] is the first code of length N. It must be
 * allocated with a minimum size of DEFLATE_CODE_MAX_BIT_LENGTH.
 * @params dict A array filled up with values depending on their huffman
 * code. For example:
 * { A: 010, B: 00, C: 10 }
 * is represented with this tree:
 *      x
 *    /   \
 *   x     x
 *  / \   /
 * B   x C
 *    /
 *   A
 * which in turn is stored in this array:
 * { -1, -1, -1, B, -1, C, -1, -1, -1, A, -1, -1, -1, -1, -1 }
 */
void generate_dict(const uint8_t *code_lengths, ssize_t size,
                   uint32_t *next_codes, dict_t dict, uint16_t dict_size) {
  memset(dict, NO_VALUE, dict_size * sizeof (uint16_t));
  for (uint16_t i = 0; i < size; ++i) {
    uint8_t length = code_lengths[i];
    if (length == 0) continue;
    uint32_t code = next_codes[length];
    // printf("%u %s (%u)\n", i, tobin(code, length), length);
    uint32_t m = 1 << (length - 1);
    uint16_t index = 0;
    while (m) {
      index <<= 1;
      index += code & m ? 2 : 1;
      m >>= 1;
    }
    // printf("[%u] = %u \n", index, i);
    dict[index] = i;
    next_codes[code_lengths[i]]++;
  }
}

/**
 * Shorthand function.
 */
void generate_dict_from_code_length(const uint8_t *code_lengths,
                                    size_t length_counts_size, dict_t dict,
                                    uint32_t dict_size) {
  uint8_t length_counts[DEFLATE_CODE_MAX_BIT_LENGTH];
  count_by_code_length(code_lengths, length_counts_size, length_counts);

  uint32_t next_codes[DEFLATE_CODE_MAX_BIT_LENGTH];
  generate_next_codes(length_counts, next_codes);

  generate_dict(code_lengths, length_counts_size,
    next_codes, dict, dict_size);
}

/**
 * Decode an input according to a dictionary and write the values in the output.
 * @param buf         the input buffer with huffman cdoes
 * @param mask        the position in the buffer
 * @param dict        the huffman dictionary
 * @param output      the output buffer. Shall be allocated to the proper size.
 */
void decode(uint8_t **buf, uint8_t *mask, dict_t dict, uint8_t *output) {
  uint16_t index = 0;
  uint16_t value = 0;

  while (value != DEFLATE_END_BLOCK_VALUE) {
    do {
      index <<= 1;
      index += **buf & *mask ? 2 : 1;
      // printf("%u", **buf & *mask ? 1 : 0);
      INCREMENT_MASK(*mask, *buf);
    } while ((value = dict[index]) == NO_VALUE);
    // printf("\nfound %u at %u\n", value, index);
    *output++ = value;
    index = 0;
  }
}

/**
 * Dynamic dictionaries are using special encoding rules.
 * https://tools.ietf.org/html/rfc1951#page-13.
 */
void decode_dynamic_dict_lengths(uint8_t **buf, uint8_t *mask, size_t output_size,
                                 dict_t dict, uint8_t *output, uint8_t *alphabet) {
  uint16_t index = 0;
  uint16_t value = 0;

  while (output_size) {
    do {
      index <<= 1;
      index += **buf & *mask ? 2 : 1;
      // printf("%u", **buf & *mask ? 1 : 0);
      INCREMENT_MASK(*mask, *buf);
    } while ((value = dict[index]) == NO_VALUE);
    // printf("\n");
    index = 0;
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
        uint8_t last_value = output[-1];
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
                        dict_t litdict, dict_t distdict) {
  // First read HLEN (4 bits), HDIST (5 bits) and HLIT (5 bits)
  uint8_t hlen;
  uint8_t hdist;
  uint8_t hlit;
  READ(hlit, *mask, *buf, 5);
  READ(hdist, *mask, *buf, 5);
  READ(hlen, *mask, *buf, 4);
  // printf("hlen %u hdist %u hlit %u\n", hlen + 4, hdist + 1, hlit + 257);
  // Read HLEN + 4 code length codes.
  uint8_t code_length_lengths[19];
  bzero(code_length_lengths, 19 * sizeof (uint8_t));
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
  uint16_t code_length_dict[256];
  generate_dict_from_code_length(code_length_lengths, 19, code_length_dict, 256);
  // Read the HLIT + 257 code length for the literal/length dynamic dictionary
  uint8_t literal_lengths[287];
  bzero(literal_lengths, 287 * sizeof (uint8_t));
  decode_dynamic_dict_lengths(buf, mask, hlit + 257, code_length_dict,
    literal_lengths, code_length_code_alphabet);
  // Read the HDIST + 1 code length for the distance dynamic dictionary
  uint8_t distance_lengths[32];
  bzero(distance_lengths, 32 * sizeof (uint8_t));
  decode_dynamic_dict_lengths(buf, mask, hdist + 1, code_length_dict,
    distance_lengths, code_length_code_alphabet);
  // Generates the dynamic dictionary
  generate_dict_from_code_length(literal_lengths, hlit + 257, litdict,
    DYNAMIC_DICT_SIZE);
  generate_dict_from_code_length(distance_lengths, hdist + 1, distdict,
    DYNAMIC_DICT_SIZE);
}

// TODO: break this function down into smaller functions
uint8_t * inflate_block(uint8_t **buf, uint8_t *mask,
                        dict_t litdict, dict_t distdict, uint8_t *output) {
  uint16_t index = 0;
  uint16_t value = 0;

  while (value != DEFLATE_END_BLOCK_VALUE) {
    uint8_t count = 0;
    do {
      index <<= 1;
      index += **buf & *mask ? 2 : 1;
      INCREMENT_MASK(*mask, *buf);
      ++count;
      if (count > 32) exit(0);
    } while ((value = litdict[index]) == NO_VALUE);
    if (value < DEFLATE_END_BLOCK_VALUE) {
      *output++ = value;
    }
    if (value > DEFLATE_END_BLOCK_VALUE) {
      uint16_t length = length_lookup[value - DEFLATE_END_BLOCK_VALUE - 1];
      // length code
      uint8_t nb_extra_bits = length_extra_bits[value - DEFLATE_END_BLOCK_VALUE - 1];
      uint16_t extra_bits = 0;
      READ(extra_bits, *mask, *buf, nb_extra_bits);
      length += extra_bits;
      // Now read the distance
      index = 0;
      do {
        index <<= 1;
        index += **buf & *mask ? 2 : 1;
        INCREMENT_MASK(*mask, *buf);
      } while ((value = distdict[index]) == NO_VALUE);
      uint16_t distance = distance_lookup[value];
      nb_extra_bits = distance_extra_bits[value];
      extra_bits = 0;
      READ(extra_bits, *mask, *buf, nb_extra_bits);
      distance += extra_bits;
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
    index = 0;
  }
  return output;
}

void inflate(uint8_t *buf, uint8_t *output) {
  g_buf = buf; // for debugging purposes
  g_output = output; // for debugging purposes
  // Generate the static huffman dictionary for literals/lengths
  uint16_t static_dict[1024];
  generate_dict(static_huffman_params.code_lengths, DEFLATE_ALPHABET_SIZE,
    static_huffman_params.next_codes, static_dict, 1024);
  // Generate the static huffman dictionary for distances
  uint16_t distance_static_dict[64];
  memset(distance_static_dict, -1, 64 * sizeof (uint16_t));
  generate_dict_from_code_length(static_huffman_params_distance_code_lengths,
    32, distance_static_dict, 32);

  uint8_t bfinal = 0; // 1 if this is the final block
  uint8_t *current_buf = buf; // the pointer to the current position in the buffer
  uint8_t mask = 1; // the integer used as mask to read bit by bit
  uint8_t *current_output = output; // the pointer to the current positionin the output
  do {
    READ(bfinal, mask, current_buf, 1);
    // Anything that is not inside the block is read from left to right.
    // See https://tools.ietf.org/html/rfc1951#page-6
    uint8_t btype; // The buffer type
    READ(btype, mask, current_buf, 2);

    switch (btype) {
      case DEFLATE_LITERAL_BLOCK_TYPE: {
        // https://tools.ietf.org/html/rfc1951#page-11
        // Uncompressed block starts on the next byte
        if (mask != 128) current_buf++;
        uint16_t len = *current_buf;
        current_buf += 4; // Skiping 4 bytes (LEN and NLEN)
        memcpy(current_output, current_buf, len * sizeof (uint8_t));
        current_buf += len;
        break;
      }
      case DEFLATE_FIX_HUF_BLOCK_TYPE: {
        // printf("DEFLATE_FIX_HUF_BLOCK_TYPE\n");
        output = inflate_block(&current_buf, &mask, static_dict,
          distance_static_dict, output);
        break;
      }
      case DEFLATE_DYN_HUF_BLOCK_TYPE: {
        // printf("DEFLATE_DYN_HUF_BLOCK_TYPE\n");
        uint16_t dict[DYNAMIC_DICT_SIZE];
        uint16_t dist_dict[DYNAMIC_DICT_SIZE];
        parse_dynamic_tree(&current_buf, &mask, dict, dist_dict);
        output = inflate_block(&current_buf, &mask, dict, dist_dict, output);
        break;
      }
    }
  } while (bfinal != 1);
}
