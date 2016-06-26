#include <memory>

#include "tmachine.h"
#include "texception.h"
#include "tfilepath.h"
#include "tiio_gif.h"
#include "tiio.h"
#include "../compatibility/tfile_io.h"
#include "tpixel.h"


Tiio::GifWriterProperties::GifWriterProperties(){}

Tiio::Reader* Tiio::makeGifReader(){ return nullptr; }
Tiio::Writer* Tiio::makeGifWriter(){ return nullptr; }