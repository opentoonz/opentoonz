

#include "dummyprocessor.h"
#include "tgl.h"
#include "tgeometry.h"
#include "tpixelutils.h"

//=============================================================================
// DummyProcessor
//-----------------------------------------------------------------------------

DummyProcessor::DummyProcessor() {}

//-----------------------------------------------------------------------------

DummyProcessor::~DummyProcessor() {}

//-----------------------------------------------------------------------------

void DummyProcessor::process(TRaster32P raster) {
  if (isActive()) {
    int x, y, lx = raster->getLx(), ly = raster->getLy();
    m_dummyData.clear();
    m_dummyData.resize(ly, 0);
    std::vector<int> hues(lx);
    for (y = 0; y < ly; y++) {
      TPixel32 *pix = raster->pixels(y);
      for (x = 0; x < lx; x++) {
        int hsv[3];
        rgb2hsv(hsv, *pix);
        hues[x] = hsv[0];
        pix++;
      }
      std::pair<int, int> range;
      for (x = 0; x + 1 < lx; x++)
        if (abs(hues[x] - hues[x + 1]) > 5) break;
      m_dummyData[y] = x;
    }
  }
}

//-----------------------------------------------------------------------------

void DummyProcessor::draw() {
  if (isActive()) {
    glColor3f(1.0, 1.0, 1.0);
    glBegin(GL_LINE_STRIP);
    for (int y = 0; y < (int)m_dummyData.size(); y++)
      glVertex2d(m_dummyData[y], y);
    glEnd();
  }
}

//-----------------------------------------------------------------------------
