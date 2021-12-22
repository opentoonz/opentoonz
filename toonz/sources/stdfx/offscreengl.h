#pragma once

#ifndef OFFSCREENGL_H
#define OFFSCREENGL_H
#if defined(LINUX) || defined(FREEBSD) || defined(MACOSX)
#include "tofflinegl.h"
#endif

class OffScreenGL {
public:
  OffScreenGL(int width, int height, int bpp, int stencilBpp = 0) {
// -----------------------------------------------------------------------------------
// //
// creo il contesto openGL //
// -----------------------------------------------------------------------------------
// //
#ifdef _WIN32
    initBITMAPINFO(m_info, width, height, bpp);

    m_offData = 0;  // a pointer to buffer
    m_offHdc  =     // open an offscreen device
        GetDC(NULL);

    m_offDIB =  // and a bitmap image
        CreateDIBSection(m_offHdc, &m_info, DIB_RGB_COLORS, &m_offData, NULL,
                         0);

    assert(m_offDIB);
    assert(m_offData);

    m_offDC = CreateCompatibleDC(m_offHdc);

    m_oldobj =  // select BIB to write
        SelectObject(m_offDC, m_offDIB);

    static PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),  // size of this pfd
        1,                              // version number
        0 | (false ? (0) : (PFD_DRAW_TO_BITMAP | PFD_SUPPORT_GDI)) |
            PFD_SUPPORT_OPENGL,  // support OpenGL
        PFD_TYPE_RGBA,           // RGBA type
        bpp,                     // 32-bit color depth
        0,
        0, 0, 0, 0, 0,   // color bits ignored
        bpp >> 2,        // alpha buffer
        0,               // shift bit ignored
        0,               // no accumulation buffer
        0, 0, 0, 0,      // accum bits ignored
        0,               // 32-bit z-buffer
        stencilBpp,      // no stencil buffer
        0,               // no auxiliary buffer
        PFD_MAIN_PLANE,  // main layer
        0,               // reserved
        0, 0, 0          // layer masks ignored
    };
    // std::cout << "bit depth = " << v << std::endl;
    // get the best available match of pixel format for the device context
    int iPixelFormat = ChoosePixelFormat(m_offDC, &pfd);
    assert(iPixelFormat != 0);

    // make that the pixel format of the device context
    int ret  = SetPixelFormat(m_offDC, iPixelFormat, &pfd);
    DWORD dw = GetLastError();
    assert(ret == TRUE);

    // make a valid context for OpenGL rendering
    m_hglRC = wglCreateContext(m_offDC);
    assert(m_hglRC);
/*
          ret = wglMakeCurrent( m_offDC, m_hglRC );
          assert(ret == TRUE);
    */
#else
#if defined(LINUX) || defined(FREEBSD) || defined(MACOSX)
    m_offlineGL = new TOfflineGL(TDimension(width, height));
#endif
#endif
  }

  ~OffScreenGL() {
// -----------------------------------------------------------------------------------
// //
// cancello il contesto openGL //
// -----------------------------------------------------------------------------------
// //
#ifdef _WIN32
    wglDeleteContext(m_hglRC);

    // release object
    SelectObject(m_offDC, m_oldobj);

    DeleteObject(m_offDC);
    DeleteObject(m_offDIB);
    DeleteObject(m_offHdc);
#else
#if defined(LINUX) || defined(FREEBSD) || defined(MACOSX)

    delete m_offlineGL;
#endif
#endif
  }

public:
// init a BITMAPINFO structure
#ifdef _WIN32
  void initBITMAPINFO(BITMAPINFO &info, int width, int height, int bpp) {
    memset(&info, 0, sizeof(BITMAPINFOHEADER));

    info.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth         = width;
    info.bmiHeader.biHeight        = height;
    info.bmiHeader.biPlanes        = 1;
    info.bmiHeader.biBitCount      = bpp;
    info.bmiHeader.biCompression   = BI_RGB;
    info.bmiHeader.biSizeImage     = 0;
    info.bmiHeader.biXPelsPerMeter = 1000;
    info.bmiHeader.biYPelsPerMeter = 1000;
    info.bmiHeader.biClrUsed       = 0;
    info.bmiHeader.biClrImportant  = 0;
  }

  void *m_offData;
  HDC m_offHdc;
  HBITMAP m_offDIB;
  HDC m_offDC;
  HGDIOBJ m_oldobj;
  HGLRC m_hglRC;
  BITMAPINFO m_info;
#else
#if defined(LINUX) || defined(FREEBSD) || defined(MACOSX)
  TOfflineGL *m_offlineGL;
#endif
#endif
};

#endif
