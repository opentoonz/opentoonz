#pragma once

#ifndef TTIO_EXR_INCLUDED
#define TTIO_EXR_INCLUDED

#include "tiio.h"
#include "tproperty.h"

#include <QCoreApplication>
namespace Tiio {

//===========================================================================

class ExrWriterProperties final : public TPropertyGroup {
  Q_DECLARE_TR_FUNCTIONS(ExrWriterProperties)
public:
  TEnumProperty m_compressionType;
  TEnumProperty m_storageType;
  TEnumProperty m_bitsPerPixel;
  TDoubleProperty m_colorSpaceGamma;

  ExrWriterProperties();

  void updateTranslation() override;
};

//===========================================================================

Tiio::Reader* makeExrReader();
Tiio::Writer* makeExrWriter();
}  // namespace Tiio

#endif