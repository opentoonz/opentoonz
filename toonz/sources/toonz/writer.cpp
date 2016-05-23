

#include <QPainter>
#include "tgl.h"
#include "writer.h"
#include "processor.h"
#include "trasterimage.h"

//=============================================================================
// Writer
//----------------------------------------------------------------------------

Writer::Writer(const TFilePath &outputFile, int lx, int ly)
	: m_pixmap(lx, ly), m_context(0), m_levelWriter(outputFile), m_frameCount(0), m_raster(lx, ly)
{
	QGLFormat glFormat;
	m_context = new QGLContext(glFormat, &m_pixmap);
	m_context->makeCurrent();

	glViewport(0, 0, lx, ly);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, lx, 0, ly, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	// To maintain pixel accuracy
	glTranslated(0.375, 0.375, 0.0);
	glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
}

//-----------------------------------------------------------------------------

Writer::~Writer()
{
	delete m_context;
}

//-----------------------------------------------------------------------------

void Writer::write(const TRaster32P &ras, Processor *processor)
{
	m_context->makeCurrent();
	glClear(GL_COLOR_BUFFER_BIT);
	if (ras) {
		glRasterPos2d(0, 0);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); // ras->getWrap());
		glDrawPixels(ras->getLx(), ras->getLy(), TGL_FMT, TGL_TYPE, ras->getRawData());
	}
	if (processor) {
		processor->draw();
	}
	glRasterPos2d(0, 0);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); // ras->getWrap());
	glReadPixels(0, 0, m_raster->getLx(), m_raster->getLy(), TGL_FMT, TGL_TYPE, m_raster->getRawData());

	TImageP img = TRasterImageP(m_raster);
	m_levelWriter->getFrameWriter(++m_frameCount)->save(img);
}

//-----------------------------------------------------------------------------
