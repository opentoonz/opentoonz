#pragma once

/*---------------------------------------------------------
 * Interfaccia di predict3d.cpp
 -------------------------------------------------------*/

#ifndef PREDICT3D_H
#define PREDICT3D_H

namespace Predict3D {

struct Point {
  double x, y;
};

bool Predict(int k, Point initial[], Point current[], bool visible[]);
}

#endif
