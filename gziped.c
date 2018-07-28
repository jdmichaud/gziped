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

// http://www.zlib.org/rfc-gzip.html#member-format
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
  fetch_block(buf, &block);
  fprintf(stdout, "bfinal: %i\n", block.bfinal);
  fprintf(stdout, "btype: %i\n", block.btype);
  return NULL;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "error: wrong arguments\n");
    usage();
    exit(1);
  }

  int ifd = open(argv[1], O_RDONLY);
  if (ifd < 0) {
    perror("open");
    exit(1);
  }

  off_t size = lseek(ifd, 0, SEEK_END);
  if (size < 0) {
    perror("lseek");
    exit(1);
  }

  uint8_t *buffer = NULL;
  buffer = mmap(buffer, size, PROT_READ, MAP_SHARED, ifd, 0);

  metadata_t metadata;
  get_metadata(buffer, size, &metadata);
  // print_metadata(*metadata);

  uint8_t *inflated = (uint8_t *) malloc(metadata.footer.isize);
  inflate(&buffer[metadata.block_offset], inflated);

  free(inflated);
  free_metadata(&metadata);
  munmap(buffer, size);
  return 0;
}
