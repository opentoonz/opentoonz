#pragma once

#ifndef DUMMYPROCESSOR_H
#define DUMMYPROCESSOR_H

#include "processor.h"

//! A dummy Processor
class DummyProcessor : public Processor
{
	std::vector<int> m_dummyData;

public:
	//! Constructor
	DummyProcessor();
	//! Destructor
	~DummyProcessor();

	//! process the raster
	void process(TRaster32P raster);
	//! draw after the processing phase
	void draw();
};

#endif // DUMMYPROCESSOR_H
