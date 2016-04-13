/* 
 * usage : lzocompress.exe <size>
 * 
 * lzocompress reads <size> bytes from stdin, compress them and write the result to stdout
 * errors are logged to stderr
 * return == 0 iff no errors
 */
#include "lzo/lzoconf.h"
#include "lzo/lzo1x.h"

/* portability layer */
#define WANT_LZO_MALLOC 1
#include "lzo/lzoutil.h"
#include <fcntl.h>


#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <io.h>
#endif

int main(int argc, char *argv[])
{
  lzo_uint src_len, dst_len, max_dst_len;
  lzo_bytep src;
  lzo_bytep dst;
  lzo_bytep wrkmem;
  int r;

#if defined(_WIN32)
  /* stdin/stdout must be binary streams */
  r = _setmode(_fileno(stdout), _O_BINARY);
  r = _setmode(_fileno(stdin), _O_BINARY);
#endif
  
  if(argc != 2) 
  {
    fprintf(stderr, "Usage : %s <size>\n    <size> is the input buffer size in bytes\n", argv[0]);
	return -1; 
  }
  src_len = atoi(argv[1]); 
  if (lzo_init() != LZO_E_OK) 
  {
    fprintf(stderr, "Couldn't initialize lzo\n");
	return -2;
  }

  /* read input buffer */
  src = (lzo_bytep) lzo_malloc(src_len);    
  fread(src, 1, src_len, stdin);
  
  /* allocate the buffer to store the compressed data */
  max_dst_len = src_len + src_len/64 + 16 + 3;  
  dst = (lzo_bytep) lzo_malloc(max_dst_len);


  /* allocate the working memory for lzo */  
  wrkmem = (lzo_bytep) lzo_malloc(LZO1X_1_MEM_COMPRESS);

  /* compress data */  
  dst_len = 0;
  r = lzo1x_1_compress(src, src_len, dst, &dst_len, wrkmem);
  
  if (r == LZO_E_OK)
  {	
    /* write compressed data */
	fwrite(dst, 1, dst_len, stdout); 	
  }
  else
  {
	fprintf(stderr, "Compression failed : code = %d\n", r);
  }
  
  /* free memory */  
  lzo_free(wrkmem);
  lzo_free(dst);
  lzo_free(src);
  
  /* return final result */
  if (r == LZO_E_OK)
    return 0;
  else 
    return -3;
}
    
  
