#include <memory>

#include "tmachine.h"
#include "texception.h"
#include "tfilepath.h"
#include "tiio_webm.h"
#include "tiio.h"
#include "../compatibility/tfile_io.h"
#include "tpixel.h"


Tiio::WebmWriterProperties::WebmWriterProperties(){}

Tiio::Reader* Tiio::makeWebmReader(){ return nullptr; }
Tiio::Writer* Tiio::makeWebmWriter(){ return nullptr; }