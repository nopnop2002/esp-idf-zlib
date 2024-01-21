// test.c
#include <zlib.h>
int main(void) {
  z_stream strm = { .zalloc = Z_NULL, .zfree = Z_NULL, .opaque = Z_NULL };
  deflateInit(&strm, Z_DEFAULT_COMPRESSION);
  deflateEnd(&strm);
  return 0;
}
