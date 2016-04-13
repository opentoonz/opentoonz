/* 
 * usage : lzodecompress.exe <output-size> <input-size>
 * 
 * lzodecompress reads <input-size> bytes from stdin, decompress them and write the result to stdout
 * the decompressed buffer must be <output-size> bytes 
 *
 * errors are logged to stderr
 * return == 0 iff no errors
 */

#include "lzo/lzoconf.h"
#include "lzo/lzo1x.h"

/* portability layer */
#define WANT_LZO_MALLOC 1
#include "lzo/lzoutil.h"

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <io.h>
#endif

int main(int argc, char *argv[])
{
  lzo_uint src_len, dst_len;
  lzo_bytep src;
  lzo_bytep dst;
  lzo_bytep wrkmem;
  int r;

#if defined(_WIN32)
  /* stdin/stdout must be binary streams */
  r = _setmode(_fileno(stdout), _O_BINARY);
  r = _setmode(_fileno(stdin), _O_BINARY);
#endif
  
  if(argc != 3) 
  {
    fprintf(stderr, "Usage : %s <output-size> <input-size>\n"
	                "    <output-size> is the decompressed buffer size in bytes\n"
                    "    <input-size> is the compressed buffer size in bytes\n", argv[0]);
    return -1; 
  }
  dst_len = atoi(argv[1]); 
  src_len = atoi(argv[2]); 
  if (lzo_init() != LZO_E_OK)
  {
    fprintf(stderr, "Couldn't initialize lzo\n");
	return -2;
  }
    
  /* allocate and read input buffer */
  src = (lzo_bytep) lzo_malloc(src_len);  
  fread(src, 1, src_len, stdin);

  /* allocate the buffer for the decompressed data */  
  dst = (lzo_bytep) lzo_malloc(dst_len);
  
  /* allocate the working memory for lzo */  
  wrkmem = (lzo_bytep) lzo_malloc(LZO1X_1_MEM_COMPRESS);
  
  /* decompress data (note: lzo1x_decompress_safe does not throw exception. It instead can return error codes) */
  r = lzo1x_decompress_safe(src, src_len, dst, &dst_len, wrkmem);
  
  if (r == LZO_E_OK)
  {
    /* write compressed data */
	fwrite(dst, 1, dst_len, stdout);
  }
  else
  {
    fprintf(stderr, "Compression failed : code = %d\n", r);
  }  
  
  /* free the memory */  
  lzo_free(wrkmem);
  lzo_free(dst);
  lzo_free(src);
  
  /* return code */
  if (r == LZO_E_OK)
    return 0;
  else 
    return -3;
}
