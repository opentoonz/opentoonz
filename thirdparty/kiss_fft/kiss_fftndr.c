/*
 *  Copyright (c) 2003-2004, Mark Borgerding. All rights reserved.
 *  This file is part of KISS FFT - https://github.com/mborgerding/kissfft
 *
 *  SPDX-License-Identifier: BSD-3-Clause
 *  See COPYING file for more information.
 */

#include "kiss_fftndr.h"
#include "_kiss_fft_guts.h"
#define MAX(x,y) ( ( (x)<(y) )?(y):(x) )

struct kiss_fftndr_state
{
    int dimReal;
    int dimOther;
    kiss_fftr_cfg cfg_r;
    kiss_fftnd_cfg cfg_nd;
    void * tmpbuf;
};

static int prod(const int *dims, int ndims)
{
    int x=1;
    while (ndims--) 
        x *= *dims++;
    return x;
}

kiss_fftndr_cfg kiss_fftndr_alloc(const int *dims,int ndims,int inverse_fft,void*mem,size_t*lenmem)
{
    KISS_FFT_ALIGN_CHECK(mem)

    kiss_fftndr_cfg st = NULL;
    size_t nr=0 , nd=0,ntmp=0;
    int dimReal = dims[ndims-1];
    int dimOther = prod(dims,ndims-1);
    size_t memneeded;
    char * ptr = NULL;

    (void)kiss_fftr_alloc(dimReal,inverse_fft,NULL,&nr);
    (void)kiss_fftnd_alloc(dims,ndims-1,inverse_fft,NULL,&nd);
    ntmp =
        MAX( 2*dimOther , dimReal+2) * sizeof(kiss_fft_scalar)  // freq buffer for one pass
        + dimOther*(dimReal+2) * sizeof(kiss_fft_scalar);  // large enough to hold entire input in case of in-place

    memneeded = KISS_FFT_ALIGN_SIZE_UP(sizeof( struct kiss_fftndr_state )) + KISS_FFT_ALIGN_SIZE_UP(nr) + KISS_FFT_ALIGN_SIZE_UP(nd) + KISS_FFT_ALIGN_SIZE_UP(ntmp);

    if (lenmem==NULL) {
        ptr = (char*) malloc(memneeded);
    }else{
        if (*lenmem >= memneeded)
            ptr = (char *)mem;
        *lenmem = memneeded; 
    }
    if (ptr==NULL)
        return NULL;
    
    st = (kiss_fftndr_cfg) ptr;
    memset( st , 0 , memneeded);
    ptr += KISS_FFT_ALIGN_SIZE_UP(sizeof(struct kiss_fftndr_state));
    
    st->dimReal = dimReal;
    st->dimOther = dimOther;
    st->cfg_r = kiss_fftr_alloc( dimReal,inverse_fft,ptr,&nr);
    ptr += KISS_FFT_ALIGN_SIZE_UP(nr);
    st->cfg_nd = kiss_fftnd_alloc(dims,ndims-1,inverse_fft, ptr,&nd);
    ptr += KISS_FFT_ALIGN_SIZE_UP(nd);
    st->tmpbuf = ptr;

    return st;
}

void kiss_fftndr(kiss_fftndr_cfg st,const kiss_fft_scalar *timedata,kiss_fft_cpx *freqdata)
{
    int k1,k2;
    int dimReal = st->dimReal;
    int dimOther = st->dimOther;
    int nrbins = dimReal/2+1;

    kiss_fft_cpx * tmp1 = (kiss_fft_cpx*)st->tmpbuf; 
    kiss_fft_cpx * tmp2 = tmp1 + MAX(nrbins,dimOther);

    // timedata is N0 x N1 x ... x Nk real

    // take a real chunk of data, fft it and place the output at correct intervals
    for (k1=0;k1<dimOther;++k1) {
        kiss_fftr( st->cfg_r, timedata + k1*dimReal , tmp1 ); // tmp1 now holds nrbins complex points
        for (k2=0;k2<nrbins;++k2)
           tmp2[ k2*dimOther+k1 ] = tmp1[k2];
    }

    for (k2=0;k2<nrbins;++k2) {
        kiss_fftnd(st->cfg_nd, tmp2+k2*dimOther, tmp1);  // tmp1 now holds dimOther complex points
        for (k1=0;k1<dimOther;++k1) 
            freqdata[ k1*(nrbins) + k2] = tmp1[k1];
    }
}

void kiss_fftndri(kiss_fftndr_cfg st,const kiss_fft_cpx *freqdata,kiss_fft_scalar *timedata)
{
    int k1,k2;
    int dimReal = st->dimReal;
    int dimOther = st->dimOther;
    int nrbins = dimReal/2+1;
    kiss_fft_cpx * tmp1 = (kiss_fft_cpx*)st->tmpbuf; 
    kiss_fft_cpx * tmp2 = tmp1 + MAX(nrbins,dimOther);

    for (k2=0;k2<nrbins;++k2) {
        for (k1=0;k1<dimOther;++k1) 
            tmp1[k1] = freqdata[ k1*(nrbins) + k2 ];
        kiss_fftnd(st->cfg_nd, tmp1, tmp2+k2*dimOther);
    }

    for (k1=0;k1<dimOther;++k1) {
        for (k2=0;k2<nrbins;++k2)
            tmp1[k2] = tmp2[ k2*dimOther+k1 ];
        kiss_fftri( st->cfg_r,tmp1,timedata + k1*dimReal);
    }
}
