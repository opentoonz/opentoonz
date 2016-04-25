#pragma once

#ifndef FLIPCONSOLEOWNER_H
#define FLIPCONSOLEOWNER_H

//-----------------------------------------------------------------
/*!	FlipConsoleOwner class
	inherited by ViewerPane and FlipBook, which receives redraw signal from FlipConsole.
*/

#include "toonzqt/flipconsole.h"

class FlipConsole;

class FlipConsoleOwner
{
public:
	virtual void onDrawFrame(int frame, const ImagePainter::VisualSettings &settings) = 0;

	// return true if the frmae is in cache. reimplemented only in Flipbook
	virtual bool isFrameAlreadyCached(int frame) { return true; };
	virtual void swapBuffers(){};
	virtual void changeSwapBehavior(bool enable){};
};
#endif