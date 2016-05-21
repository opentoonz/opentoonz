#pragma once

#ifndef TIIO_SGI_INCLUDED
#define TIIO_SGI_INCLUDED

#include "tiio.h"
//#include "timage_io.h"
#include "tproperty.h"

namespace Tiio
{

Tiio::ReaderMaker makeSgiReader;
Tiio::WriterMaker makeSgiWriter;

class SgiWriterProperties : public TPropertyGroup
{
public:
	TEnumProperty m_pixelSize;
	TBoolProperty m_compressed;
	TEnumProperty m_endianess;
	SgiWriterProperties();
};

} // namespace

#endif
