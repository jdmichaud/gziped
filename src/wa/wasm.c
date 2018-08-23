#include <gziped.h>
#include <emscripten.h>

EMSCRIPTEN_KEEPALIVE
void em_inflate(uint8_t *buf, uint8_t *output) {
  inflate(buf, output);
}
