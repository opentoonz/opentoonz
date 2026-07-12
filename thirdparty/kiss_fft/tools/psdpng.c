/*
 *  Copyright (c) 2003-2004, Mark Borgerding. All rights reserved.
 *  This file is part of KISS FFT - https://github.com/mborgerding/kissfft
 *
 *  SPDX-License-Identifier: BSD-3-Clause
 *  See COPYING file for more information.
 */

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <png.h>

#include "kiss_fft.h"
#include "kiss_fftr.h"

int nfft=1024;
FILE * fin=NULL;
FILE * fout=NULL;

int navg=20;
int remove_dc=0;
int nrows=0;
float * vals=NULL;
int stereo=0;

static
void config(int argc,char** argv)
{
    while (1) {
        int c = getopt (argc, argv, "n:r:as");
        if (c == -1)
            break;
        switch (c) {
        case 'n': nfft=(int)atoi(optarg);break;
        case 'r': navg=(int)atoi(optarg);break;
        case 'a': remove_dc=1;break;
        case 's': stereo=1;break;
        case '?':
            fprintf (stderr, "usage options:\n"
                     "\t-n d: fft dimension(s) [1024]\n"
                     "\t-r d: number of rows to average [20]\n"
                     "\t-a : remove average from each fft buffer\n"
                     "\t-s : input is stereo, channels will be combined before fft\n"
                     "16 bit machine format real input is assumed\n"
                     );
            break;
        default:
            fprintf (stderr, "bad %c\n", c);
            exit (1);
            break;
        }
    }
    if ( optind < argc ) {
        if (strcmp("-",argv[optind]) !=0)
            fin = fopen(argv[optind],"rb");
        ++optind;
    }

    if ( optind < argc ) {
        if ( strcmp("-",argv[optind]) !=0 ) 
            fout = fopen(argv[optind],"wb");
        ++optind;
    }
    if (fin==NULL)
        fin=stdin;
    if (fout==NULL)
        fout=stdout;
}

#define CHECKNULL(p) if ( (p)==NULL ) do { fprintf(stderr,"CHECKNULL failed @ %s(%d): %s\n",__FILE__,__LINE__,#p );exit(1);} while(0)

typedef struct
{
    png_byte r;
    png_byte g;
    png_byte b;
} rgb_t;

static 
void val2rgb(float x,rgb_t *p)
{
    const double pi = 3.14159265358979;
    p->g = (int)(255*sin(x*pi));
    p->r = (int)(255*abs(sin(x*pi*3/2)));
    p->b = (int)(255*abs(sin(x*pi*5/2)));
    //fprintf(stderr,"%.2f : %d,%d,%d\n",x,(int)p->r,(int)p->g,(int)p->b);
}

static
void cpx2pixels(rgb_t * res,const float * fbuf,size_t n)
{
    size_t i;
    float minval,maxval,valrange;
    minval=maxval=fbuf[0];

    for (i = 0; i < n; ++i) {
        if (fbuf[i] > maxval) maxval = fbuf[i];
        if (fbuf[i] < minval) minval = fbuf[i];
    }

    fprintf(stderr,"min ==%f,max=%f\n",minval,maxval);
    valrange = maxval-minval;
    if (valrange == 0) {
        fprintf(stderr,"min == max == %f\n",minval);
        exit (1);
    }

    for (i = 0; i < n; ++i)
        val2rgb( (fbuf[i] - minval)/valrange , res+i );
}

static
void transform_signal(void)
{
    short *inbuf;
    kiss_fftr_cfg cfg=NULL;
    kiss_fft_scalar *tbuf;
    kiss_fft_cpx *fbuf;
    float *mag2buf;
    int i;
    int n;
    int avgctr=0;

    int nfreqs=nfft/2+1;

    CHECKNULL( cfg=kiss_fftr_alloc(nfft,0,0,0) );
    CHECKNULL( inbuf=(short*)malloc(sizeof(short)*2*nfft ) );
    CHECKNULL( tbuf=(kiss_fft_scalar*)malloc(sizeof(kiss_fft_scalar)*nfft ) );
    CHECKNULL( fbuf=(kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx)*nfreqs ) );
    CHECKNULL( mag2buf=(float*)calloc(nfreqs,sizeof(float) ) );

    while (1) {
        if (stereo) {
            n = fread(inbuf,sizeof(short)*2,nfft,fin);
            if (n != nfft ) 
                break;
            for (i=0;i<nfft;++i) 
                tbuf[i] = inbuf[2*i] + inbuf[2*i+1];
        }else{
            n = fread(inbuf,sizeof(short),nfft,fin);
            if (n != nfft ) 
                break;
            for (i=0;i<nfft;++i) 
                tbuf[i] = inbuf[i];
        }

        if (remove_dc) {
            float avg = 0;
            for (i=0;i<nfft;++i)  avg += tbuf[i];
            avg /= nfft;
            for (i=0;i<nfft;++i)  tbuf[i] -= (kiss_fft_scalar)avg;
        }

        /* do FFT */
        kiss_fftr(cfg,tbuf,fbuf);

        for (i=0;i<nfreqs;++i)
            mag2buf[i] += fbuf[i].r * fbuf[i].r + fbuf[i].i * fbuf[i].i;

        if (++avgctr == navg) {
            avgctr=0;
            ++nrows;
            CHECKNULL( vals = (float*)realloc(vals,sizeof(float)*nrows*nfreqs) );
            float eps = 1;
            for (i=0;i<nfreqs;++i)
                vals[(nrows - 1) * nfreqs + i] = 10 * log10 ( mag2buf[i] / navg + eps );
            memset(mag2buf,0,sizeof(mag2buf[0])*nfreqs);
        }
    }

    free(cfg);
    free(inbuf);
    free(tbuf);
    free(fbuf);
    free(mag2buf);
}

static
void make_png(void)
{
    png_bytepp row_pointers=NULL;
    rgb_t * row_data=NULL;
    int i;
    int nfreqs = nfft/2+1;

    png_structp png_ptr=NULL;
    png_infop info_ptr=NULL;
    
    CHECKNULL( png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING,0,0,0) );
    CHECKNULL( info_ptr = png_create_info_struct(png_ptr) );


    png_init_io(png_ptr, fout );
    png_set_IHDR(png_ptr, info_ptr ,nfreqs,nrows,8,PNG_COLOR_TYPE_RGB,PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT,PNG_FILTER_TYPE_DEFAULT );
    

    CHECKNULL( row_data = (rgb_t*)malloc(sizeof(rgb_t) * nrows * nfreqs) );
    cpx2pixels(row_data, vals, nfreqs*nrows );

    CHECKNULL( row_pointers = malloc(nrows*sizeof(png_bytep)) );
    for (i=0;i<nrows;++i) {
        row_pointers[i] = (png_bytep)(row_data + i*nfreqs);
    }
    png_set_rows(png_ptr, info_ptr, row_pointers);


    fprintf(stderr,"creating %dx%d png\n",nfreqs,nrows);
    fprintf(stderr,"bitdepth %d \n",png_get_bit_depth(png_ptr,info_ptr ) );

    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY , NULL);

    free(row_pointers);
    free(row_data);
    png_destroy_write_struct(&png_ptr, &info_ptr);
}

int main(int argc,char ** argv)
{
    config(argc,argv);

    transform_signal();

    make_png();

    if (fout!=stdout) fclose(fout);
    if (fin!=stdin) fclose(fin);
    free(vals);
    return 0;
}
