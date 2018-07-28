#include "gziped.h"

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
