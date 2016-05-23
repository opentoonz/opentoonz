

#include <QPainter>
#include "tgl.h"
#include "viewer.h"
#include "processor.h"

//=============================================================================
// Viewer
//-----------------------------------------------------------------------------

Viewer::Viewer(QWidget *parent)
	: QGLWidget(parent), m_raster(0), m_processor(0), update_frame(true)
{
}

//-----------------------------------------------------------------------------

Viewer::~Viewer()
{
}

//-----------------------------------------------------------------------------

void Viewer::initializeGL()
{
	glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
}

//-----------------------------------------------------------------------------

void Viewer::resizeGL(int width, int height)
{
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, 0, height, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	// To maintain pixel accuracy
	glTranslated(0.375, 0.375, 0.0);
}

//-----------------------------------------------------------------------------

void Viewer::paintGL()
{
	glClear(GL_COLOR_BUFFER_BIT);
	if (m_raster) {
		glRasterPos2d(0, 0);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); // ras->getWrap());
		glDrawPixels(m_raster->getLx(), m_raster->getLy(), TGL_FMT, TGL_TYPE, m_raster->getRawData());
	}
	if (m_processor) {
		m_processor->draw();
	}

	if (m_message != "") {
		glColor3d(0, 0, 0);
		glPushMatrix();
		const double sc = 5;
		glScaled(sc, sc, sc);
		tglDrawText(TPointD(5, 5), m_message);
		glPopMatrix();
	}
}

//-----------------------------------------------------------------------------

void Viewer::setRaster(TRaster32P raster)
{
	m_raster = raster;
	if (update_frame)
		update();
}

//-----------------------------------------------------------------------------

void Viewer::setMessage(std::string msg)
{
	m_message = msg;
	update();
}
