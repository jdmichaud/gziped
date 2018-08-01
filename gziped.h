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
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>

#include "debug.h"

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

#define DEFLATE_LENGTH_EXTRA_BITS_ARRAY_SIZE 29
#define DEFLATE_LENGTH_EXTRA_BITS_ARRAY_OFFSET 257

// https://tools.ietf.org/html/rfc1951#page-12
uint8_t length_extra_bits[DEFLATE_LENGTH_EXTRA_BITS_ARRAY_SIZE] = {
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5,
  5, 5, 0
};

#define DEFLATE_DISTANCE_EXTRA_BITS_ARRAY_SIZE 30

// https://tools.ietf.org/html/rfc1951#page-12
uint8_t distance_extra_bits[DEFLATE_DISTANCE_EXTRA_BITS_ARRAY_SIZE] = {
  0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11,
  11, 12, 12, 13, 13
};

// Move the mask bit to the right up to 1, then reinit to 128 and increment ptr
#define INCREMENT_MASK(mask, ptr) (mask = (mask = mask >> 1) ? mask : 128) == 128 && ++ptr;

void usage() {
  fprintf(stderr, "usage: gzip <file>\n");
}

void print_metadata(metadata_t metadata) {
  fprintf(stdout, "magic: 0x%04x\n", metadata.header.magic);
  fprintf(stdout, "cm: 0x%02x (%s)\n", metadata.header.cm, metadata.header.cm == 8 ? "DEFLATE" : "");
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
  fprintf(stdout, "os: %s\n", metadata.header.os < 14 ? OS[metadata.header.os] : "unknown");

  if (metadata.header.flg & FNAME)
    fprintf(stdout, "filename: %s\n", metadata.extra_header.fname ? metadata.extra_header.fname : "");
  if (metadata.header.flg & FCOMMENT)
    fprintf(stdout, "comment: %s\n", metadata.extra_header.fcomment ? metadata.extra_header.fcomment : "");
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
  metadata->footer.crc32 = buf[size - 8];
  metadata->footer.isize = buf[size - 4];

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
  memset(length_counts, 0, DEFLATE_CODE_MAX_BIT_LENGTH * sizeof (uint8_t));
  for (; size > 0; --size) {
    length_counts[code_lengths[size - 1]]++;
  }
}

/**
 * Generates the starting code for a specific code lengths.
 * if bit_counts = { 0, 1, 1, 2 } the next_codes is { 0, 2, 6 }, representing
 * the following dictionnary:
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
 * @param bit_counts bit_counts[N] is the number of code of length N. Its size
 * is DEFLATE_CODE_MAX_BIT_LENGTH.
 * @param next_codes  next_codes[N] is the first code of length N. It must be
 * allocated with a minimum size of DEFLATE_CODE_MAX_BIT_LENGTH.
 */
void generate_next_codes(uint8_t *bit_counts, uint32_t *next_codes) {
  uint32_t code = 0;
  for (uint8_t nbits = 1; nbits <= DEFLATE_CODE_MAX_BIT_LENGTH; nbits++) {
    next_codes[nbits] = code = (code + bit_counts[nbits - 1]) << 1;
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
                   uint32_t *next_codes, uint16_t *dict) {
  memset(dict, NO_VALUE, 512 * sizeof (uint16_t));
  for (uint16_t i = 0; i < size; ++i) {
    uint8_t length = code_lengths[i];

    uint32_t code = next_codes[length];
    printf("%u %s (%u)\n", i, tobin(code, length), length);
    uint32_t m = 1 << (length - 1);
    uint16_t index = 0;
    while (m) {
      index <<= 1;
      index += code & m ? 2 : 1;
      m >>= 1;
    }
    dict[index] = i;
    next_codes[code_lengths[i]]++;
  }
}

void inflate_block(uint8_t **pos, uint8_t *mask,
                   uint16_t *dict, uint8_t *output) {
  uint16_t index = 0;
  uint16_t value = 0;
  uint8_t i = 0;
  // while (value != DEFLATE_END_BLOCK_VALUE) {
    do {
      index <<= 1;
      index += **pos & *mask ? 2 : 1;
      if ((i+3) % 4 == 0) printf(" ");
      printf("%u", **pos & *mask ? 1 : 0);
      INCREMENT_MASK(*mask, *pos);
      ++i;
    } while (i < 32); //while ((value = dict[index]) == NO_VALUE);
    // printf(" - %u\n", value);
  // }
}

void inflate(uint8_t *buf, uint8_t *output) {
  // Generate the static huffman dictionary
  uint16_t static_dict[288];
  generate_dict(static_huffman_params.code_lengths, DEFLATE_ALPHABET_SIZE,
    static_huffman_params.next_codes, static_dict);

  uint8_t bfinal = 0;
  uint8_t *current_buf = buf;
  uint8_t mask = 0b10000000;
  uint8_t *current_output = output;
  do {
    bfinal = (*current_buf & mask) ? 1 : 0;
    INCREMENT_MASK(mask, current_buf);
    uint8_t btype = *current_buf & mask ? 2 : 0;
    INCREMENT_MASK(mask, current_buf);
    btype |= *current_buf & mask ? 1 : 0;
    INCREMENT_MASK(mask, current_buf);

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
        printf("DEFLATE_FIX_HUF_BLOCK_TYPE\n");
        inflate_block(&current_buf, &mask, static_dict, output);
        break;
      }
      case DEFLATE_DYN_HUF_BLOCK_TYPE: {
        printf("DEFLATE_DYN_HUF_BLOCK_TYPE\n");
        uint16_t dict[1024];
        inflate_block(&current_buf, &mask, dict, output);
        break;
      }
    }
    return;
  } while (bfinal != 1);
}

