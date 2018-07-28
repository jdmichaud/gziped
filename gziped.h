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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>

#include "debug.h"

#define BUFFER_SIZE 1024

#define GZIP_HEADER_SIZE  10
#define GZIP_MAGIC        0x8B1F
#define GZIP_DEFLATE_CM   0x08

#define DEFLATE_LITERAL_BLOCK_TYPE 0
#define DEFLATE_FIX_HUF_BLOCK_TYPE 1
#define DEFLATE_DYN_HUF_BLOCK_TYPE 2
#define DEFLATE_CODE_MAX_BIT_LENGTH 32
#define DEFLATE_ALPHABET_SIZE 288

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

char *tobin(uint32_t code, uint8_t length) {
  if (length == 0) return NULL;
  char *s = (char *) malloc(sizeof (char) * length + 1);
  s[length] = 0;
  for (uint8_t i = 0; i < length; ++i) {
    s[length - i - 1] = code & (1 << i) ? '1' : '0';
  }
  return s;
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

void free_dict(char **dict, ssize_t size) {
  while (size > 0) {
    free(dict[size - 1]);
    size--;
  }
}

/**
 * Generates a dictionary mapping every value of the alphabet to a code.
  */
void generate_dict(const uint8_t *code_lengths, ssize_t size,
                   uint32_t *next_codes, char **dict) {
  for (uint16_t i = 0; i < size; ++i) {
    dict[i] = tobin(next_codes[code_lengths[i]], code_lengths[i]);
    next_codes[code_lengths[i]]++;
  }
}

void fetch_block(uint8_t *buf, block_t *block) {
  block->bfinal = (*buf & 128) >> 7;
  block->btype = (*buf & 96) >> 5;

  switch (block->btype) {
    case DEFLATE_LITERAL_BLOCK_TYPE:
      break;
    case DEFLATE_FIX_HUF_BLOCK_TYPE:
      break;
    case DEFLATE_DYN_HUF_BLOCK_TYPE:
      break;
  }
}

uint8_t *inflate(uint8_t *buf, uint8_t *output) {
  block_t block;
  // Generate the static huffman dictionary
  char *static_dict[288];
  generate_dict(static_huffman_params.code_lengths, DEFLATE_ALPHABET_SIZE,
    static_huffman_params.next_codes, static_dict);

  fetch_block(buf, &block);
  fprintf(stdout, "bfinal: %i\n", block.bfinal);
  fprintf(stdout, "btype: %i\n", block.btype);
  return NULL;
}

