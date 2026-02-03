#pragma once

#ifndef TZERARYFX_H
#define TZERARYFX_H

#include "trasterfx.h"

//============================================================

//    Forward declarations

class TZeraryColumnFx;

//============================================================

//************************************************************************
//    TBaseZeraryFx  definition
//************************************************************************

//! Defines built-in Toonz zerary fxs.
/*!
\par Toonz Internals - Zerary Column Fxs

  Toonz Zerary fxs are currently implemented with associated
  <I> zerary column fxs <\I> owning them. It is important to make
  these column fxs accessible from the actual zerary fx in order to
  allow complete traversability of an fxs tree.
*/
class TZeraryFx : public TRasterFx {
public:
  friend class TZeraryColumnFx;  // Defined in ToonzLib

public:
  TZeraryFx() : m_columnFx() {}

  //! Returns the associated column fx, if any.
  TZeraryColumnFx *getColumnFx() const { return m_columnFx; }

private:
  TZeraryColumnFx *m_columnFx;  //!< The associated column fx. Note that
                                //!< it is never cloned, as expected.
};

#endif  // TZERARYFX_H
