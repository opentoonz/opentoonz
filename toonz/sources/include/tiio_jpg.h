#pragma once

#ifndef TTIO_JPG_INCLUDED
#define TTIO_JPG_INCLUDED

#include "tiio.h"
#include "tproperty.h"

extern "C" {
#define XMD_H
#include "jpeglib.h"

/*
 * <setjmp.h> is used for the optional error recovery mechanism shown in
 * the second part of the example.
 */
#include <setjmp.h>
}

#undef DVAPI
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#else
#define DVAPI DV_IMPORT_API
#endif

namespace Tiio
{

class DVAPI JpgReader : public Tiio::Reader
{
	struct jpeg_decompress_struct m_cinfo;
	struct jpeg_error_mgr m_jerr;
	FILE *m_chan;
	JSAMPARRAY m_buffer;
	bool m_isOpen;

public:
	JpgReader();
	~JpgReader();

	Tiio::RowOrder getRowOrder() const;

	void open(FILE *file);

	void readLine(char *buffer, int x0, int x1, int shrink);
	int skipLines(int lineCount);
};

DVAPI Tiio::ReaderMaker makeJpgReader;
DVAPI Tiio::WriterMaker makeJpgWriter;

class DVAPI JpgWriterProperties : public TPropertyGroup
{
public:
	TIntProperty m_quality;
	TIntProperty m_smoothing;

	static const std::string QUALITY;

	JpgWriterProperties()
		: m_quality(QUALITY, 0, 100, 90), m_smoothing("Smoothing", 0, 100, 0)
	{
		bind(m_quality);
		bind(m_smoothing);
	}
};

} // namespace

#endif
