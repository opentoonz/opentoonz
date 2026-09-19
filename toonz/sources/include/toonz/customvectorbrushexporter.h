#pragma once

#ifndef CUSTOM_VECTOR_BRUSH_EXPORTER_H
#define CUSTOM_VECTOR_BRUSH_EXPORTER_H

#include "tfilepath.h"
#include "tvectorimage.h"

#include <vector>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class QString;

namespace CustomVectorBrushExporter {

struct DVAPI SourceFrame {
  TFrameId sourceFrameId;
  TVectorImageP image;

  SourceFrame(const TFrameId &sourceFrameId = TFrameId(),
              const TVectorImageP &image    = TVectorImageP())
      : sourceFrameId(sourceFrameId), image(image) {}
};

struct DVAPI Source {
  std::vector<SourceFrame> frames;
  TFrameId preferredFirstFrame;
};

struct DVAPI PreparedFile {
  TFilePath temporaryPath;
  TINT64 serializedBytes = 0;
  int frameCount         = 0;
};

// Writes a validated temporary PLI, placing the preferred source frame first.
DVAPI bool prepare(const Source &source, const TFilePath &destinationFolder,
                   PreparedFile &prepared, QString &errorMessage);

// Appends source frames without changing existing frame IDs.
DVAPI bool prepareAppend(const Source &source, const TFilePath &destinationPath,
                         PreparedFile &prepared, QString &errorMessage);

// Replaces destinationPath with a prepared file.
DVAPI bool commit(PreparedFile &prepared, const TFilePath &destinationPath,
                  QString &errorMessage);

DVAPI void discard(PreparedFile &prepared);

}  // namespace CustomVectorBrushExporter

#endif  // CUSTOM_VECTOR_BRUSH_EXPORTER_H
