#pragma once

#ifndef CURVEIO_H
#define CURVEIO_H

#include <string>

class TDoubleParam;

void saveCurve(TDoubleParam *curve);
void loadCurve(TDoubleParam *curve);
void exportCurve(TDoubleParam *curve, const std::string &name);

#endif
