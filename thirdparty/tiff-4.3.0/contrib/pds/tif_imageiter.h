typedef struct _TIFFImageIter TIFFImageIter;

/* The callback function is called for each "block" of image pixel data after
   it has been read from the file and decoded. This image pixel data is in the
   buffer pp, and this data represents the image pixels from (x,y) to
   (x+w,y+h). It is stored in pixel format, so each pixel contains
   img->samplesperpixel consecutive samples each containing img->bitspersample
   bits of data. The array pp is ordered in h consecutive rows of w+fromskew
   pixels each. */
typedef void (*ImageIterTileContigRoutine)
    (TIFFImageIter*, void *, uint32_t, uint32_t, uint32_t, uint32_t, int32_t,
	unsigned char*);
#define	DECLAREContigCallbackFunc(name) \
static void name(\
    TIFFImageIter* img, \
    void* user_data, \
    uint32_t x, uint32_t y, \
    uint32_t w, uint32_t h, \
    int32_t fromskew, \
    u_char* pp \
)

typedef void (*ImageIterTileSeparateRoutine)
    (TIFFImageIter*, void *, uint32_t, uint32_t, uint32_t, uint32_t, int32_t,
	unsigned char*, unsigned char*, unsigned char*, unsigned char*);
#define	DECLARESepCallbackFunc(name) \
static void name(\
    TIFFImageIter* img, \
    void* user_data, \
    uint32_t x, uint32_t y, \
    uint32_t w, uint32_t h,\
    int32_t fromskew, \
    u_char* r, u_char* g, u_char* b, u_char* a\
)

struct _TIFFImageIter {
	TIFF*	tif;				/* image handle */
	int	stoponerr;			/* stop on read error */
	int	isContig;			/* data is packed/separate */
	int	alpha;				/* type of alpha data present */
	uint32_t	width;				/* image width */
	uint32_t	height;				/* image height */
	uint16_t	bitspersample;			/* image bits/sample */
	uint16_t	samplesperpixel;		/* image samples/pixel */
	uint16_t	orientation;			/* image orientation */
	uint16_t	photometric;			/* image photometric interp */
	uint16_t*	redcmap;			/* colormap palette */
	uint16_t*	greencmap;
	uint16_t*	bluecmap;
						/* get image data routine */
	int	(*get)(TIFFImageIter*, void *udata, uint32_t, uint32_t);
	union {
	    void (*any)(TIFFImageIter*);
	    ImageIterTileContigRoutine		contig;
	    ImageIterTileSeparateRoutine	separate;
	} callback;				/* fn to exec for each block */
};
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 8
 * fill-column: 78
 * End:
 */
