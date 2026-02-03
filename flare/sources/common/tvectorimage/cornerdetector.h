#pragma once

#ifndef CORNERDETECTOR_H
#define CORNERDETECTOR_H

void detectCorners(const std::vector<T3DPointD> &inputPoints, int minSampleNum,
                   int minDist, int maxDist, double maxAngle,
                   std::vector<int> &cornerIndexes);

#endif  // CORNERDETECTOR_H
