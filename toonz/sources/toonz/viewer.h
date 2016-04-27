

#ifndef VIEWER_H
#define VIEWER_H

#include <QGLWidget>
#include "traster.h"

class Processor;

//! OpenGL Widget that display the movie
class Viewer : public QGLWidget
{
	//! Raster that store the current Frame of the Movie.
	TRaster32P m_raster;
	Q_OBJECT;
	//! String used to display "loading" text while the movie is being loaded
	std::string m_message;
	//! Pointer to a Processor to process the frame drawing OpenGL primitive over it.
	Processor *m_processor;

public:
	bool update_frame;

	//! Construct a QGLWidget object which is a child of parent.
	Viewer(QWidget *parent = 0);
	//! Destroys the widget
	~Viewer();

	//! Set the raster to draw
	void setRaster(TRaster32P raster);
	//! Returns the raster
	const TRaster32P &getRaster() const { return m_raster; }

	//! Set the Processor to draw over the current frame
	void setProcessor(Processor *processor) { m_processor = processor; }
	//! Set the message. This message is displayed for every frame (if is a non-empty string).
	void setMessage(std::string msg);

protected:
	void initializeGL();
	void paintGL();
	void resizeGL(int width, int height);
};

#endif // VIEWER_H
