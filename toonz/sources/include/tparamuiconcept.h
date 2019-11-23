#pragma once

#ifndef TPARAM_UI_CONCEPT_H
#define TPARAM_UI_CONCEPT_H

#include "tparam.h"

#include <vector>
#include <string>

//***********************************************************************************************
//    TParamUIConcept  definition
//***********************************************************************************************

//! Defines the User Interface Types available for a group of TParam objects.
/*!
  A User Interface Concept is used to associate parameters to the way they
should be
  interactively represented. For example, a TDoubleParamP may be used to
represent
  multiple objects, such as lengths or angles; a couple of TPointParamP may
represent
  vectors, and so on. Observe that each entry in the list is associated to a
specific
  struct of parameter objects.
\n\n
  The list in the Type enum provides all the interface types currently supported
by Toonz.
*/
class TParamUIConcept {
public:
  // Associated with each specific types are the structure the m_params member
  // will need to hold.
  // The mandatory parameters are in square brackets.

  enum Type {
    NONE = 0,

    RADIUS,   // Distance from a point (radius). Represented by     {
              // [TDoubleParamP], TPointParamP }
    WIDTH,    // Width, as distance from a line with given angle.   {
              // [TDoubleParamP], TDoubleParamP }
    ANGLE,    // An angle.                                          {
              // [TDoubleParamP] }
    ANGLE_2,  // An angle range defined with start and end angles.
              // { [2 TDoubleParamP], TDoubleParamP }

    POINT,    // A Point.                                           {
              // [TPointParamP] }
    POINT_2,  // A Point given its X and Y coordinates.             { [2
              // TDoubleParamP] }
    VECTOR,   // A Vector.                                          {
              // [TPointParamP], TPointParamP }
    POLAR,    // A Vector in polar coordinates, from the origin.    { [2
              // TDoubleParamP] }

    SIZE,  // Size, shown with a rect with P00 on the origin.    {
           // [TDoubleParamP], TDoubleParamP }
    QUAD,  // A Quadrilateral.                                   { [4
           // TPointParamP] }
    RECT,  // A Rect, with width, height and center.             { [2
           // TDoubleParamP], TPointParamP }

    DIAMOND,       // A diagonally laid square.                          {
                   // [TDoubleParamP] }
    LINEAR_RANGE,  // A band-like range between two points.
                   // { [2 TPointParamP] }

    TYPESCOUNT
  };

public:
  Type m_type;                    //!< Concept identifier
  std::string m_label;            //!< Name to show on editing
  std::vector<TParamP> m_params;  //!< Associated parameters
};

#endif  // TPARAM_UI_CONCEPT_H
