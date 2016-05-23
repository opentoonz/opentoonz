#pragma once

#ifndef WRITER_H
#define WRITER_H

#include <QGLWidget>
#include "traster.h"
#include "tfilepath.h"
#include "tlevel_io.h"

class Processor;

//! Draw the frame (with the processing) before saving it.
class Writer
{
	//! QPaintDevice used only to initialize the QGLContext
	QPixmap m_pixmap;
	//! Offline OpenGL context that draw directly to a QPaintDevice (in this case a QPixmap)
	QGLContext *m_context;
	//! Class to write the level to file
	TLevelWriterP m_levelWriter;
	//! Frame counter
	int m_frameCount;
	//! Current Raster
	TRaster32P m_raster;

public:
	//! Constructor
	/*!
    \param &outputFile filename of the output movie
    \param lx an integer contains the width of the movie
    \param ly an integer contains the height of the movie
   */
	Writer(const TFilePath &outputFile, int lx, int ly);
	//! Destructor
	~Writer();

	/*! Write the raster, using the processor to the GL Context,
      then read the context to store it in the m_raster. 
      Hence write m_raster to the movie file using TLevelWriter
  */
	void write(const TRaster32P &ras, Processor *processor);

protected:
};

#endif // WRITER_H
