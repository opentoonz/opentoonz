

#include "qtofflinegl.h"
#include <traster.h>
#include <tconvert.h>

//-----------------------------------------------------------------------------

#ifdef WIN32

void swapRedBlueChannels(void *buffer, int bufferSize) // Flips The Red And Blue Bytes (WidthxHeight)
{
	void *b = buffer; // Pointer To The Buffer

#ifdef x64
	int size = bufferSize;
	UCHAR *pix = (UCHAR *)b;
	while (size > 0) {
		UCHAR r = *pix;
		UCHAR b = *(pix + 2);
		*pix = b;
		*(pix + 2) = r;
		pix += 4;
		size--;
	}
#else
	__asm		  // Assembler Code To Follow
	{
		mov ecx, bufferSize // Counter Set To Dimensions Of Our Memory Block
			mov ebx, b // Points ebx To Our Data (b)
			label: // Label Used For Looping
			mov al,[ebx+0] // Loads Value At ebx Into al
			mov ah,[ebx+2] // Loads Value At ebx+2 Into ah
			mov [ebx+2],al // Stores Value In al At ebx+2
			mov [ebx+0],ah // Stores Value In ah At ebx

			add ebx,4 // Moves Through The Data By 4 Bytes
			dec ecx // Decreases Our Loop Counter
			jnz label // If Not Zero Jump Back To Label
	}
#endif
}

#endif

//-----------------------------------------------------------------------------

#if defined(MACOSX)
#if defined(powerpc)

void rightRotateBits(UCHAR *buf, int bufferSize)
{
	UINT *buffer = (UINT *)buf;
	register UINT app;
	for (int i = 0; i < bufferSize; i++, buffer++) {
		app = *buffer;
		*buffer = app >> 8 | app << 24;
	}
}
#else
void rightRotateBits(UCHAR *buf, int bufferSize)
{
	UINT *buffer = (UINT *)buf;
	register UINT app;
	for (int i = 0; i < bufferSize; i++, buffer++) {
		app = *buffer;
		*buffer = (app >> 16 & 0x000000ff) | (app << 16 & 0x00ff0000) | (app & 0xff00ff00);
	}
}
#endif
#endif

//-----------------------------------------------------------------------------

//=============================================================================
// QtOfflineGL : implem. offlineGL usando QT
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

QtOfflineGL::QtOfflineGL(TDimension rasterSize, const TOfflineGL::Imp *shared)
	: TOfflineGL::Imp(rasterSize.lx, rasterSize.ly), m_context(0), m_oldContext(0)
{
	createContext(rasterSize, shared);
	/*
  makeCurrent();

  glClearColor(0.0f,0.0f,0.0f,0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  doneCurrent();
*/
}

//-----------------------------------------------------------------------------

QtOfflineGL::~QtOfflineGL()
{
	delete m_context;
}

//-----------------------------------------------------------------------------

void QtOfflineGL::createContext(TDimension rasterSize, const TOfflineGL::Imp *shared)
{
	// Imposto il formato dei Pixel (pixelFormat)
	/*
	 32,                    // 32-bit color depth
	 0, 0, 0, 0, 0, 0,      // color bits ignored
	 8,                     // no alpha buffer
	 0,                     // shift bit ignored
	 0,                     // no accumulation buffer
	 0, 0, 0, 0,            // accum bits ignored
	 32,                    // 32-bit z-buffer
	 32,                    // max stencil buffer
	 0,                     // no auxiliary buffer
	 PFD_MAIN_PLANE,        // main layer
	 0,                     // reserved
	 0, 0, 0                // layer masks ignored

	 ATTENZIONE !! SU MAC IL FORMATO E' DIVERSO (casomai possiamo mettere un ifdef)

	 SPECIFICHE  MAC = depth_size 24, stencil_size 8, alpha_size 1

  */

	QGLFormat fmt;

#ifdef WIN32
	fmt.setAlphaBufferSize(8);
	fmt.setAlpha(true);
	fmt.setRgba(true);
	fmt.setDepthBufferSize(32);
	fmt.setDepth(true);
	fmt.setStencilBufferSize(32);
	fmt.setStencil(true);
	fmt.setAccum(false);
	fmt.setPlane(0);
#elif MACOSX
	fmt = QGLFormat::defaultFormat();
	//printf("GL Version: %s\n",glGetString(GL_VERSION));
	fmt.setVersion(2, 1); /* OSX10.8 では 3.2 だめかも */
#if 0
  fmt.setAlphaBufferSize(8);
  fmt.setAlpha(true);
  fmt.setRgba(true);
  fmt.setDepthBufferSize(32);
  fmt.setDepth(true);
  fmt.setStencilBufferSize(8);
  fmt.setStencil(true);
  fmt.setAccum(false);
  fmt.setPlane(0);
  fmt.setDirectRendering(false);
#endif
#endif
	/* FIXME: ここでいう QPixmap は Level Strip のセルに相当する.
	 QPixmap に GLContext を生成して bind できれば描画したベクタのラスタ画像がそこに反映されるはずだが
	 QLContext の生成ができないためにうまくいかない.
   */
	printf("QPixmap(%d, %d)\n", rasterSize.lx, rasterSize.ly);

	QSurfaceFormat format;
	format.setProfile(QSurfaceFormat::CompatibilityProfile);

	m_surface = new QOffscreenSurface();
	m_surface->setFormat(format);
	m_surface->create();

	m_context = new QOpenGLContext();
	m_context->setFormat(format);
	m_context->create();
	m_context->makeCurrent(m_surface);

	QOpenGLFramebufferObjectFormat fbo_format;
	m_fbo = new QOpenGLFramebufferObject(rasterSize.lx, rasterSize.ly, fbo_format);
	m_fbo->bind();

	printf("create context:%p [thread:0x%x]\n", m_context, QThread::currentThreadId());

	// Creo il contesto OpenGL - assicurandomi che sia effettivamente creato
	// NOTA: Se il contesto non viene creato, di solito basta ritentare qualche volta.

}
//-----------------------------------------------------------------------------

void QtOfflineGL::makeCurrent()
{
	if (m_context) {
		m_context->makeCurrent(m_surface);
	}
	// else
	//  m_oldContext = 0;
}

//-----------------------------------------------------------------------------

void QtOfflineGL::doneCurrent()
{
	if (m_context) {
		m_context->doneCurrent();
	}
}

//-----------------------------------------------------------------------------

void QtOfflineGL::saveCurrentContext()
{
	//  m_oldContext = const_cast<QGLContext*>(QGLContext::currentContext());
}

//-----------------------------------------------------------------------------

void QtOfflineGL::restoreCurrentContext()
{
	//  if(m_oldContext) m_oldContext->makeCurrent();
	//  m_oldContext = 0;
}

//-----------------------------------------------------------------------------

void QtOfflineGL::getRaster(TRaster32P raster)
{
	makeCurrent();
	glFlush();

	int lx = raster->getLx();
	int ly = raster->getLy();

	raster->lock();
	raster->copy( TRaster32P(lx, ly, lx, (TPixelRGBM32 *)m_fbo->toImage().mirrored().bits(), false) );

#ifdef WIN32
	swapRedBlueChannels(raster->getRawData(), lx * ly);
#elif MACOSX
	//rightRotateBits(raster->getRawData(), lx * ly);
#endif
	raster->unlock();
}

//QGLPixelBuffer::hasOpenGLPbuffers() (statica) -> true se la scheda supporta i PBuffer

//=============================================================================
// QtOfflineGLPBuffer : implem. offlineGL usando QT e PBuffer
//-----------------------------------------------------------------------------

QtOfflineGLPBuffer::QtOfflineGLPBuffer(TDimension rasterSize)
	: TOfflineGL::Imp(rasterSize.lx, rasterSize.ly), m_context(0)
{
	createContext(rasterSize);

	makeCurrent();

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	doneCurrent();
}

//-----------------------------------------------------------------------------

QtOfflineGLPBuffer::~QtOfflineGLPBuffer()
{
	delete m_context;
}

//-----------------------------------------------------------------------------

void QtOfflineGLPBuffer::createContext(TDimension rasterSize)
{
	// Imposto il formato dei Pixel (pixelFormat)
	/*
      32,                    // 32-bit color depth
      0, 0, 0, 0, 0, 0,      // color bits ignored
      8,                     // no alpha buffer
      0,                     // shift bit ignored
      0,                     // no accumulation buffer
      0, 0, 0, 0,            // accum bits ignored
      32,                    // 32-bit z-buffer
      32,                    // max stencil buffer
      0,                     // no auxiliary buffer
      PFD_MAIN_PLANE,        // main layer
      0,                     // reserved
      0, 0, 0                // layer masks ignored

      ATTENZIONE !! SU MAC IL FORMATO E' DIVERSO (casomai possiamo mettere un ifdef)

      SPECIFICHE  MAC = depth_size 24, stencil_size 8, alpha_size 1

    */

	QGLFormat fmt;

#ifdef WIN32
	fmt.setAlphaBufferSize(8);
	fmt.setAlpha(false);
	fmt.setRgba(true);
	fmt.setDepthBufferSize(32);
	fmt.setDepth(true);
	fmt.setStencilBufferSize(32);
	fmt.setStencil(true);
	fmt.setAccum(false);
	fmt.setPlane(0);
#elif MACOSX
	fmt.setAlphaBufferSize(1);
	fmt.setAlpha(false);
	fmt.setRgba(true);
	fmt.setDepthBufferSize(24);
	fmt.setDepth(true);
	fmt.setStencilBufferSize(8);
	fmt.setStencil(true);
	fmt.setAccum(false);
	fmt.setPlane(0);
#endif

	// Il PixelBuffer deve essere con width ed height potenze di 2

	int sizeMax = tmax(rasterSize.lx, rasterSize.ly);

	// trovo la potenza di 2 che "contiene" sizeMax e la utilizzo per il PBuffer
	int pBufferSize = 2;
	while (pBufferSize < sizeMax)
		pBufferSize *= 2;

	m_context = new QGLPixelBuffer(QSize(pBufferSize, pBufferSize), fmt);
}

//-----------------------------------------------------------------------------

void QtOfflineGLPBuffer::makeCurrent()
{
	if (m_context)
		m_context->makeCurrent();
}

//-----------------------------------------------------------------------------

void QtOfflineGLPBuffer::doneCurrent()
{
	if (m_context)
		m_context->doneCurrent();
}

//-----------------------------------------------------------------------------

void QtOfflineGLPBuffer::getRaster(TRaster32P raster)
{
	makeCurrent();
	glFlush();

	//The image is stored using a 32-bit ARGB format (0xAARRGGBB).
	QImage image = m_context->toImage();

	int lx = raster->getLx(), ly = raster->getLy();

	static const TRaster32P emptyRaster;
	if (image.height() == 0 || image.width() == 0)
		return;

	// devo iniziare a leggere la Y da un certo offset
	// dato dalla differenza tra la y della image e la y del raster
	int yOffset = image.height() - ly;
	raster->lock();

	for (int y = 0; y < ly; y++) {
		QRgb *inpPix = (QRgb *)image.scanLine(yOffset + y);

		TPixel32 *pix = raster->pixels(ly - 1 - y);
		TPixel32 *endPix = pix + lx;

		for (; pix < endPix; ++pix) {
			pix->m = 255;
			pix->r = qRed(*inpPix);
			pix->g = qGreen(*inpPix);
			pix->b = qBlue(*inpPix);
			*inpPix++;
		}
	}
	raster->unlock();
}
