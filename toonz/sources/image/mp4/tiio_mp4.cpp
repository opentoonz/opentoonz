#include <memory>

#include "tmachine.h"
#include "texception.h"
#include "tfilepath.h"
#include "tiio_mp4.h"
#include "tiio.h"
#include "../compatibility/tfile_io.h"
#include "tpixel.h"


Tiio::Mp4WriterProperties::Mp4WriterProperties(){}

Tiio::Reader* Tiio::makeMp4Reader(){
	return nullptr;
}
Tiio::Writer* Tiio::makeMp4Writer(){ return nullptr; 
}