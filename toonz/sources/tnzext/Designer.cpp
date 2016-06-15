

#include "ext/Designer.h"
#include <tgl.h>
#include <assert.h>

//-----------------------------------------------------------------------------

ToonzExt::Designer::Designer() {}

//-----------------------------------------------------------------------------

ToonzExt::Designer::~Designer() {}

//-----------------------------------------------------------------------------

void ToonzExt::Designer::draw(ToonzExt::SmoothDeformation *) {
  assert(!"Designer for SmoothDeformation not available!");
}

//-----------------------------------------------------------------------------

void ToonzExt::Designer::draw(ToonzExt::CornerDeformation *) {
  assert(!"Designer for CornerDeformation not available!");
}

//-----------------------------------------------------------------------------

void ToonzExt::Designer::draw(ToonzExt::StraightCornerDeformation *) {
  assert(!"Designer for StraightCornerDeformation not available!");
}

//-----------------------------------------------------------------------------

void ToonzExt::Designer::draw(ToonzExt::StrokeDeformation *) {
  assert(!"Designer for StrokeDeformation not available!");
}

//-----------------------------------------------------------------------------

void ToonzExt::Designer::draw(ToonzExt::Selector *) {
  assert(!"Designer for Selector not available!");
}

//-----------------------------------------------------------------------------

double ToonzExt::Designer::getPixelSize2() const { return tglGetPixelSize2(); }

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
