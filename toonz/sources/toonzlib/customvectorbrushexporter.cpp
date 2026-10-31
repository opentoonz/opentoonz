#include "toonz/customvectorbrushexporter.h"

#include "tcolorstyles.h"
#include "tcontenthistory.h"
#include "tlevel.h"
#include "tlevel_io.h"
#include "tpalette.h"
#include "tsystem.h"

#include <QDir>
#include <QObject>
#include <QTemporaryFile>

#include <algorithm>
#include <limits>
#include <set>

namespace CustomVectorBrushExporter {
namespace {

constexpr int VectorImagePatternStyleTag = 2800;
constexpr int VectorBrushStyleTag        = 3000;

using FrameIds = std::set<TFrameId>;

QString exceptionMessage(const TException &exception) {
  return QString::fromStdWString(exception.getMessage());
}

bool hasUnsupportedStyles(const TVectorImageP &image) {
  TPalette *palette = image ? image->getPalette() : nullptr;
  if (!palette) return false;

  std::set<int> usedStyles;
  image->getUsedStyles(usedStyles);
  for (int styleId : usedStyles) {
    TColorStyle *style = palette->getStyle(styleId);
    if (!style) continue;
    const int tagId = style->getTagId();
    if (tagId == VectorImagePatternStyleTag || tagId == VectorBrushStyleTag)
      return true;
  }
  return false;
}

void removeTemporaryFile(const TFilePath &path) {
  if (path.isEmpty()) return;
  try {
    if (TFileStatus(path).doesExist()) TSystem::deleteFile(path);
  } catch (...) {
  }
}

bool validateSource(const Source &source,
                    std::vector<const SourceFrame *> &orderedFrames,
                    QString &errorMessage) {
  if (source.frames.empty()) {
    errorMessage = QObject::tr("No vector brush frames were supplied.");
    return false;
  }

  std::set<TFrameId> frameIds;
  auto preferred = source.frames.end();
  for (auto it = source.frames.begin(); it != source.frames.end(); ++it) {
    if (!it->image || it->image->getStrokeCount() == 0) {
      errorMessage = QObject::tr("A vector brush frame is empty.");
      return false;
    }
    if (!frameIds.insert(it->sourceFrameId).second) {
      errorMessage = QObject::tr("A vector brush source frame is duplicated.");
      return false;
    }
    if (it->sourceFrameId == source.preferredFirstFrame) preferred = it;
    if (hasUnsupportedStyles(it->image)) {
      errorMessage = QObject::tr(
          "The selection uses a Vector Brush or Vector Image Pattern style. "
          "Nested vector brush dependencies cannot be saved as a custom "
          "vector brush.");
      return false;
    }
  }

  if (preferred == source.frames.end()) {
    errorMessage =
        QObject::tr("The preferred first frame is not in the brush source.");
    return false;
  }

  orderedFrames.reserve(source.frames.size());
  orderedFrames.push_back(&*preferred);
  for (const SourceFrame &frame : source.frames)
    if (frame.sourceFrameId != source.preferredFirstFrame)
      orderedFrames.push_back(&frame);

  return true;
}

TVectorImageP prepareFrame(const TVectorImageP &source,
                           const TPaletteP &sharedPalette,
                           QString &errorMessage) {
  TVectorImageP sourceCopy = source->clone();
  TVectorImageP output     = new TVectorImage();
  output->setPalette(sharedPalette.getPointer());
  output->setAutocloseTolerance(source->getAutocloseTolerance());
  output->mergeImage(sourceCopy, TAffine());
  output->findRegions();

  const TRectD bounds = output->getBBox();
  if (bounds.isEmpty() || bounds.x0 >= bounds.x1 || bounds.y0 >= bounds.y1) {
    errorMessage = QObject::tr(
        "The selection has no usable width or height for a vector brush.");
    return TVectorImageP();
  }

  return output;
}

bool validateWrittenLevel(const TFilePath &path,
                          const FrameIds &expectedFrameIds,
                          QString &errorMessage) {
  try {
    TLevelReaderP reader(path);
    TLevelP level = reader->loadInfo();
    if (!level ||
        level->getFrameCount() != static_cast<int>(expectedFrameIds.size())) {
      errorMessage = QObject::tr(
          "The custom vector brush file could not be validated after writing.");
      return false;
    }

    FrameIds writtenFrameIds;
    for (TLevel::Iterator it = level->begin(); it != level->end(); ++it)
      writtenFrameIds.insert(it->first);
    if (writtenFrameIds != expectedFrameIds) {
      errorMessage = QObject::tr(
          "The custom vector brush file contains unexpected frame IDs after "
          "writing.");
      return false;
    }

    for (const TFrameId &frameId : expectedFrameIds) {
      TImageReaderP frameReader = reader->getFrameReader(frameId);
      TVectorImageP frame;
      if (frameReader) frame = frameReader->load();
      if (!frame) {
        errorMessage =
            QObject::tr(
                "The custom vector brush file does not contain a valid frame "
                "%1.")
                .arg(QString::fromStdString(frameId.expand(TFrameId::NO_PAD)));
        return false;
      }
    }
  } catch (const TException &exception) {
    errorMessage = QObject::tr("Could not validate the custom vector brush: %1")
                       .arg(exceptionMessage(exception));
    return false;
  } catch (...) {
    errorMessage = QObject::tr(
        "An unexpected error occurred while validating the custom vector "
        "brush.");
    return false;
  }
  return true;
}

bool prepareLevel(const TLevelP &level, const FrameIds &expectedFrameIds,
                  const TFilePath &destinationFolder, const QString &creator,
                  const TContentHistory *contentHistory, PreparedFile &prepared,
                  QString &errorMessage) {
  removeTemporaryFile(prepared.temporaryPath);
  prepared = PreparedFile();

  try {
    if (!TFileStatus(destinationFolder).doesExist())
      TSystem::mkDir(destinationFolder);
    if (!TFileStatus(destinationFolder).isWritableDir()) {
      errorMessage = QObject::tr("The destination folder is not writable: %1")
                         .arg(destinationFolder.getQString());
      return false;
    }

    QDir folder(destinationFolder.getQString());
    {
      // The Windows PLI stream requires the temporary-file reservation to be
      // released before it can open the path.
      QTemporaryFile temporaryFile(
          folder.filePath(".custom-vector-brush-XXXXXX.pli"));
      temporaryFile.setAutoRemove(true);
      if (!temporaryFile.open()) {
        errorMessage = QObject::tr(
            "Could not reserve a temporary PLI file in the destination "
            "folder.");
        return false;
      }
      prepared.temporaryPath = TFilePath(temporaryFile.fileName());
    }

    if (TFileStatus(prepared.temporaryPath).doesExist()) {
      errorMessage = QObject::tr("Could not release the temporary PLI file.");
      removeTemporaryFile(prepared.temporaryPath);
      prepared = PreparedFile();
      return false;
    }

    TLevelWriterP writer(prepared.temporaryPath);
    if (!creator.isEmpty()) writer->setCreator(creator);
    if (contentHistory) writer->setContentHistory(contentHistory->clone());
    writer->save(level);
    writer = TLevelWriterP();  // PLI serialization finishes in the destructor.

    TFileStatus fileStatus(prepared.temporaryPath);
    if (!fileStatus.doesExist()) {
      errorMessage =
          QObject::tr("The PLI writer could not create a temporary file in: %1")
              .arg(destinationFolder.getQString());
      removeTemporaryFile(prepared.temporaryPath);
      prepared = PreparedFile();
      return false;
    }
    if (fileStatus.getSize() <= 0) {
      errorMessage = QObject::tr("The PLI writer created an empty file.");
      removeTemporaryFile(prepared.temporaryPath);
      prepared = PreparedFile();
      return false;
    }

    prepared.serializedBytes = fileStatus.getSize();
    prepared.frameCount      = static_cast<int>(expectedFrameIds.size());
    if (!validateWrittenLevel(prepared.temporaryPath, expectedFrameIds,
                              errorMessage)) {
      removeTemporaryFile(prepared.temporaryPath);
      prepared = PreparedFile();
      return false;
    }
  } catch (const TException &exception) {
    errorMessage = QObject::tr("Could not save the PLI file: %1")
                       .arg(exceptionMessage(exception));
    removeTemporaryFile(prepared.temporaryPath);
    prepared = PreparedFile();
    return false;
  } catch (...) {
    errorMessage =
        QObject::tr("An unexpected error occurred while saving the PLI file.");
    removeTemporaryFile(prepared.temporaryPath);
    prepared = PreparedFile();
    return false;
  }

  return true;
}

}  // namespace

void discard(PreparedFile &prepared) {
  removeTemporaryFile(prepared.temporaryPath);
  prepared = PreparedFile();
}

bool prepare(const Source &source, const TFilePath &destinationFolder,
             PreparedFile &prepared, QString &errorMessage) {
  discard(prepared);
  errorMessage.clear();

  std::vector<const SourceFrame *> orderedFrames;
  if (!validateSource(source, orderedFrames, errorMessage)) return false;

  TPaletteP sharedPalette = new TPalette();
  TLevelP level           = new TLevel();
  level->setPalette(sharedPalette.getPointer());

  FrameIds expectedFrameIds;
  int outputFrameNumber = 1;
  for (const SourceFrame *sourceFrame : orderedFrames) {
    TVectorImageP output =
        prepareFrame(sourceFrame->image, sharedPalette, errorMessage);
    if (!output) return false;

    TFrameId outputFrameId(outputFrameNumber++);
    level->setFrame(outputFrameId, output);
    expectedFrameIds.insert(outputFrameId);
  }

  return prepareLevel(level, expectedFrameIds, destinationFolder, QString(),
                      nullptr, prepared, errorMessage);
}

bool prepareAppend(const Source &source, const TFilePath &destinationPath,
                   PreparedFile &prepared, QString &errorMessage) {
  discard(prepared);
  errorMessage.clear();

  std::vector<const SourceFrame *> orderedFrames;
  if (!validateSource(source, orderedFrames, errorMessage)) return false;

  try {
    TLevelReaderP reader(destinationPath);
    TLevelP existingLevel = reader->loadInfo();
    if (!existingLevel || existingLevel->getFrameCount() == 0) {
      errorMessage =
          QObject::tr("The existing PLI file does not contain any frames.");
      return false;
    }

    TPalette *existingPalette = existingLevel->getPalette();
    if (!existingPalette) {
      errorMessage =
          QObject::tr("The existing PLI file does not contain a palette.");
      return false;
    }

    TPaletteP sharedPalette = existingPalette->clone();
    TLevelP outputLevel     = new TLevel();
    outputLevel->setPalette(sharedPalette.getPointer());

    FrameIds expectedFrameIds;
    int highestFrameNumber = 0;
    for (TLevel::Iterator it = existingLevel->begin();
         it != existingLevel->end(); ++it) {
      const TFrameId frameId = it->first;
      highestFrameNumber = (std::max)(highestFrameNumber, frameId.getNumber());

      TImageReaderP frameReader = reader->getFrameReader(frameId);
      TVectorImageP frame;
      if (frameReader) frame = frameReader->load();
      if (!frame) {
        errorMessage =
            QObject::tr("Could not load frame %1 from the existing PLI file.")
                .arg(QString::fromStdString(frameId.expand(TFrameId::NO_PAD)));
        return false;
      }

      frame->setPalette(sharedPalette.getPointer());
      outputLevel->setFrame(frameId, frame);
      expectedFrameIds.insert(frameId);
    }

    const int maximumFrameNumber = (std::numeric_limits<int>::max)();
    if (highestFrameNumber >
        maximumFrameNumber - static_cast<int>(orderedFrames.size())) {
      errorMessage =
          QObject::tr("The existing PLI file has no available frame numbers.");
      return false;
    }

    int nextFrameNumber = highestFrameNumber + 1;
    for (const SourceFrame *sourceFrame : orderedFrames) {
      TVectorImageP output =
          prepareFrame(sourceFrame->image, sharedPalette, errorMessage);
      if (!output) return false;

      TFrameId outputFrameId(nextFrameNumber++);
      outputLevel->setFrame(outputFrameId, output);
      expectedFrameIds.insert(outputFrameId);
    }

    return prepareLevel(outputLevel, expectedFrameIds,
                        destinationPath.getParentDir(), reader->getCreator(),
                        reader->getContentHistory(), prepared, errorMessage);
  } catch (const TException &exception) {
    errorMessage = QObject::tr("Could not append to the PLI file: %1")
                       .arg(exceptionMessage(exception));
    return false;
  } catch (...) {
    errorMessage = QObject::tr(
        "An unexpected error occurred while appending to the PLI "
        "file.");
    return false;
  }
}

bool commit(PreparedFile &prepared, const TFilePath &destinationPath,
            QString &errorMessage) {
  errorMessage.clear();
  if (prepared.temporaryPath.isEmpty() ||
      !TFileStatus(prepared.temporaryPath).doesExist()) {
    errorMessage = QObject::tr("No prepared custom vector brush file exists.");
    return false;
  }

  try {
    TSystem::replaceFile(destinationPath, prepared.temporaryPath);
    prepared = PreparedFile();
  } catch (const TException &exception) {
    errorMessage = QObject::tr("Could not install the custom vector brush: %1")
                       .arg(exceptionMessage(exception));
    return false;
  } catch (...) {
    errorMessage = QObject::tr(
        "An unexpected error occurred while installing the custom vector "
        "brush.");
    return false;
  }
  return true;
}

}  // namespace CustomVectorBrushExporter
