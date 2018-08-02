#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "gziped.h"

void write_file(metadata_t metadata, uint8_t *content) {
  if (metadata.extra_header.fname != NULL) {
    int of = open(metadata.extra_header.fname, O_RDWR | O_CREAT | O_TRUNC,
      S_IRUSR | S_IWUSR | S_IRGRP);
    if (of < 0) {
      perror("open");
      return;
    }
    if (write(of, content, metadata.footer.isize) != metadata.footer.isize) {
      // We ignore the fact that we can write fewer byte than requested for now
      perror("write");
      return;
    }
    if (close(of) != 0) perror("close");
  }
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
  write_file(metadata, inflated);

  free(inflated);
  free_metadata(&metadata);
  munmap(buffer, size);
  return 0;
}
