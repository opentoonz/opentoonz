

#include "tiio_jpg.h"
#include "tiio_bmp.h"
#include "tiio_std.h"
#include "tfiletype.h"

using namespace Tiio;
using namespace TFileType;

void Tiio::defineStd() {
  defineReaderMaker("jpg", Tiio::makeJpgReader);
  defineWriterMaker("jpg", Tiio::makeJpgWriter, true);
  declare("jpg", TFileType::RASTER_IMAGE);
  defineWriterProperties("jpg", new JpgWriterProperties());

  defineReaderMaker("jpeg", Tiio::makeJpgReader);
  declare("jpeg", TFileType::RASTER_IMAGE);

  defineReaderMaker("bmp", Tiio::makeBmpReader);
  defineWriterMaker("bmp", Tiio::makeBmpWriter, true);
  declare("bmp", TFileType::RASTER_IMAGE);
  defineWriterProperties("bmp", new BmpWriterProperties());
}
