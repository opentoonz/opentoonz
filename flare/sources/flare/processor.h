#pragma once

#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "traster.h"

//! Class that process a raster
class Processor {
  //! Used to switch on/off the processor
  bool m_active;

public:
  //! Constructor
  Processor() : m_active(true){};
  //! Virtual destructor
  virtual ~Processor(){};

  //! Virtual method to implement
  /*!
This method used the raster in input to compute values.
The input raster can be modified (if necessary) or can be only
used to compute.
\param raster A Raster input to compute stuff
*/
  virtual void process(TRaster32P raster) = 0;
  //! Virtual method to implement
  /*!
This method is used to draw any OpenGL primitive over the current
raster without affecting it. In this manner with the process method
you can compute some values without modify the raster, and then, with
this method you can draw points, lines, or anything using the computed
values
*/
  virtual void draw() = 0;

  bool isActive() { return m_active; }
  void setActive(bool value) { m_active = value; }
};

#endif  // PROCESSOR_H
