

// TnzCore includes
#include "tsystem.h"
#include "tpalette.h"
#include "tvectorimage.h"
#include "tropcm.h"
#include "timage_io.h"
#include "tlevel_io.h"
#include "tofflinegl.h"
#include "tvectorrenderdata.h"
#include "tstroke.h"

// ToonzLib includes
#include "toonz/txshsimplelevel.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/imagemanager.h"
#include "toonz/tcamera.h"
#include "toonz/toonzimageutils.h"
#include "toonz/levelupdater.h"
#include "toonz/preferences.h"

#include "toutputproperties.h"
#include "ttile.h"

// ToonzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"

// Toonz includes
#include "tapp.h"

// STD includes
#include <deque>

// tcg includes
#include "tcg/tcg_numeric_ops.h"
#include "tcg/tcg_poly_ops.h"

// Qt includes
#include <QApplication>

#include "exportlevelcommand.h"

using namespace DVGui;

//***********************************************************************************
//    Local namespace stuff
//***********************************************************************************

namespace {

struct BusyCursorOverride {
  BusyCursorOverride() { QApplication::setOverrideCursor(Qt::WaitCursor); }
  ~BusyCursorOverride() { QApplication::restoreOverrideCursor(); }
};

//---------------------------------------------------------------------------

struct ExportOverwriteCB final : public IoCmd::OverwriteCallbacks {
  bool overwriteRequest(const TFilePath &fp) override {
    int ret = DVGui::MsgBox(
        QObject::tr("Warning: file %1 already exists.").arg(toQString(fp)),
        QObject::tr("Continue Exporting"), QObject::tr("Stop Exporting"), 1);
    return (ret == 1);
  }
};

//---------------------------------------------------------------------------

struct ExportProgressCB final : public IoCmd::ProgressCallbacks {
  QString m_processedName;
  ProgressDialog m_pb;

public:
  ExportProgressCB() : m_pb("", QObject::tr("Cancel"), 0, 0) { m_pb.show(); }
  static QString msg() {
    return QObject::tr("Exporting level of %1 frames in %2");
  }

  void setProcessedName(const QString &name) override {
    m_processedName = name;
  }
  void setRange(int min, int max) override {
    m_pb.setMaximum(max);
    buildString();
  }
  void setValue(int val) override { m_pb.setValue(val); }
  bool canceled() const override { return m_pb.wasCanceled(); }
  void buildString() {
    m_pb.setLabelText(
        msg().arg(QString::number(m_pb.maximum())).arg(m_processedName));
  }
};

}  // namespace

//***********************************************************************************
//    Special image processing
//***********************************************************************************

namespace {

struct VectorThicknessTransformer {
  const TVectorImageP &m_vi;

  VectorThicknessTransformer(const TVectorImageP &vi,
                             double thickTransform[2][2], double param)
      : m_vi(vi), m_mutexLocker(vi->getMutex()) {
    // Build thickness transform
    assert(0.0 <= param && param <= 1.0);

    double thickPoly[2] = {tcg::numeric_ops::lerp(thickTransform[0][0],
                                                  thickTransform[1][0], param),
                           tcg::numeric_ops::lerp(thickTransform[0][1],
                                                  thickTransform[1][1], param)};

    if (thickPoly[0] == 0.0 && thickPoly[1] == 1.0) return;

    // Iterate each stroke
    UINT s, sCount = m_vi->getStrokeCount();
    for (s = 0; s != sCount; ++s) {
      TStroke *stroke = m_vi->getStroke(s);

      // Discriminate completely thin strokes
      if (stroke->getMaxThickness() > 0.0) {
        // Backup and alter the thickness of each control point
        m_thicknessesBackup.push_back(StrokeThicknesses(s));
        StrokeThicknesses &thickBackup = m_thicknessesBackup.back();

        int cp, cpCount = stroke->getControlPointCount();
        for (cp = 0; cp != cpCount; ++cp) {
          TThickPoint point = stroke->getControlPoint(cp);

          thickBackup.m_thicknesses.push_back(point.thick);

          point.thick =
              std::max(tcg::poly_ops::evaluate(thickPoly, 1, point.thick), 0.0);

          stroke->setControlPoint(cp, point);
        }
      }
    }
  }

  ~VectorThicknessTransformer() {
    // Iterate altered strokes
    std::deque<StrokeThicknesses>::const_iterator tbt,
        tbEnd = m_thicknessesBackup.end();

    for (tbt = m_thicknessesBackup.begin(); tbt != tbEnd; ++tbt) {
      TStroke *stroke = m_vi->getStroke(tbt->m_strokeIdx);

      assert(stroke->getControlPointCount() == tbt->m_thicknesses.size());

      // Restore each altered control point
      int cp, cpCount = stroke->getControlPointCount();
      std::vector<double>::const_iterator tt = tbt->m_thicknesses.begin();

      for (cp = 0; cp != cpCount; ++cp, ++tt) {
        TThickPoint point = stroke->getControlPoint(cp);
        point.thick       = *tt;

        stroke->setControlPoint(cp, point);
      }
    }
  }

private:
  struct StrokeThicknesses {
    int m_strokeIdx;
    std::vector<double> m_thicknesses;

  public:
    StrokeThicknesses(int strokeIdx) : m_strokeIdx(strokeIdx) {}
  };

private:
  std::deque<StrokeThicknesses> m_thicknessesBackup;
  QMutexLocker m_mutexLocker;
};

//---------------------------------------------------------------------------

inline void flattenTones(const TRasterCM32P &cmout, const TRasterCM32P &cmin) {
  int lx = cmin->getLx(), ly = cmin->getLy();
  int wrapin = cmin->getWrap(), wrapout = cmout->getWrap();

  assert(lx == cmout->getLx() && ly == cmout->getLy());

  TPixelCM32 *pixIn, *lineIn, *lineEnd;
  TPixelCM32 *pixOut, *lineOut;

  for (int y = 0; y < ly; ++y) {
    lineIn = cmin->pixels(y), lineOut = cmout->pixels(y);
    lineEnd = lineIn + lx;
    for (pixIn = lineIn, pixOut = lineOut; pixIn < lineEnd; ++pixIn, ++pixOut)
      *pixOut = *pixIn, pixOut->setTone(pixOut->getTone() < 128 ? 0 : 255);
  }
}

}  // namespace

//***********************************************************************************
//    ImageExporter  definition
//***********************************************************************************

namespace {
using namespace IoCmd;

class ImageExporter {
  const TXshSimpleLevel &m_sl;
  ExportLevelOptions m_opts;

  TRasterP m_rout;
  std::unique_ptr<TOfflineGL> m_glContext;

public:
  ImageExporter(const TXshSimpleLevel &sl, const ExportLevelOptions &opts)
      : m_sl(sl), m_opts(opts) {}

  TImageP exportedImage(const TFrameId &fid, TXshLevelType outImgType);

private:
  void adjust0Dpi(const TRasterImageP &ri);

  TRasterImageP exportedImage(TRasterImageP ri);
  TRasterImageP exportedImage(TToonzImageP ti);
  TRasterImageP exportedRasterImage(TVectorImageP vi);
  TToonzImageP exportedToonzImage(TVectorImageP vi);
};

//===========================================================================

void ImageExporter::adjust0Dpi(const TRasterImageP &ri) {
  double dpix, dpiy;
  ri->getDpi(dpix, dpiy);

  if (dpix == 0 && dpiy == 0) {
    dpix = dpiy = Preferences::instance()->getDefLevelDpi();
    ri->setDpi(dpix, dpiy);
  }
}

//---------------------------------------------------------------------------

TRasterImageP ImageExporter::exportedImage(TRasterImageP ri) {
  assert(ri);

  adjust0Dpi(ri);

  if (m_opts.m_bgColor != TPixel::Transparent) {
    ri = (TRasterImageP)ri->cloneImage();
    TRop::addBackground(ri->getRaster(), m_opts.m_bgColor);
  }

  return ri;
}

//---------------------------------------------------------------------------

TRasterImageP ImageExporter::exportedImage(TToonzImageP ti) {
  // Allocate a suitable fullcolor raster if not already done
  if (!m_rout || m_rout->getSize() != ti->getRaster()->getSize())
    m_rout = TRaster32P(ti->getRaster()->getSize());

  // Remove antialias if required
  TRasterCM32P cm(ti->getRaster());
  if (m_opts.m_noAntialias) {
    // Copy output to rout's buffer for convenience
    cm = TRasterCM32P(m_rout->getLx(), m_rout->getLy(), m_rout->getWrap(),
                      (TPixelCM32 *)m_rout->getRawData());

    flattenTones(cm, ti->getRaster());
  }

  // Convert the frame's colormap to fullcolor
  {
    TTile srcTile(cm), dstTile(m_rout);
    TRop::convert(dstTile, srcTile,
                  ti->getPalette(),  // May be an in-place conversion, if
                  false, true);      // antialias was removed
  }

  // Fallback on fullcolor case
  TRasterImageP ri(m_rout);

  double dpix, dpiy;
  ti->getDpi(dpix, dpiy);
  ri->setDpi(dpix, dpiy);

  return exportedImage(TRasterImageP(m_rout));
}

//---------------------------------------------------------------------------

TRasterImageP ImageExporter::exportedRasterImage(TVectorImageP vi) {
  const TCamera &camera = m_opts.m_camera;

  TVectorRenderData rd(TVectorRenderData::ProductionSettings(),
                       camera.getStageToCameraRef(), TRect(), vi->getPalette());
  rd.m_antiAliasing = !m_opts.m_noAntialias;

  if (!m_glContext.get()) m_glContext.reset(new TOfflineGL(camera.getRes()));

  // Render the vector image to fullcolor raster
  TRasterImageP ri;
  {
    m_glContext->clear(m_opts.m_bgColor);
    m_glContext->draw(vi, rd);

    ri = TRasterImageP(m_glContext->getRaster());

    const TPointD &dpi = camera.getDpi();
    ri->setDpi(dpi.x, dpi.y);
  }

  return ri;
}

//---------------------------------------------------------------------------

TToonzImageP ImageExporter::exportedToonzImage(TVectorImageP vi) {
  // Prepare rendering data
  TPalette *plt = vi->getPalette();

  const TCamera &camera   = m_opts.m_camera;
  const TAffine &aff      = camera.getStageToCameraRef();
  const TDimensionD &size = camera.getSize();
  const TDimension &res   = camera.getRes();

  const TPointD pos(-0.5 * size.lx, -0.5 * size.ly);

  // Render to toonz image
  TToonzImageP ti = ToonzImageUtils::vectorToToonzImage(vi, aff, plt, pos, res);
  ti->setPalette(plt);

  if (m_opts.m_noAntialias) {
    TRasterCM32P ras = ti->getRaster();
    flattenTones(ras, ras);
  }

  return ti;
}

//---------------------------------------------------------------------------

TImageP ImageExporter::exportedImage(const TFrameId &fid,
                                     TXshLevelType outImgType) {
  // Retrieve the source image
  TImageP img = m_sl.getFullsampledFrame(fid, ImageManager::dontPutInCache);
  if (!img) return TImageP();

  switch (m_sl.getType()) {
  case OVL_XSHLEVEL:
    img = exportedImage(TRasterImageP(img));
    break;
  case TZP_XSHLEVEL:
    img = exportedImage(TToonzImageP(img));
    break;
  case PLI_XSHLEVEL: {
    TVectorImageP vi(img);

    // Apply temporary thickness modification
    int fCount = m_sl.getFrameCount();
    double param =
        (fCount == 1) ? 0.0 : m_sl.fid2index(fid) / double(fCount - 1);

    VectorThicknessTransformer transformer(vi, m_opts.m_thicknessTransform,
                                           param);

    img = (outImgType == TZP_TYPE) ? TImageP(exportedToonzImage(vi))
                                   : TImageP(exportedRasterImage(vi));
    break;
  }
  default:
    assert(!"Wrong level type");
    img = TImageP();
    break;
  }

  return img;
}

}  // namespace

//***********************************************************************************
//    Export Level commands
//***********************************************************************************

TImageP IoCmd::exportedImage(const std::string &ext, const TXshSimpleLevel &sl,
                             const TFrameId &fid,
                             const ExportLevelOptions &opts) {
  ImageExporter exporter(sl, opts);

  return exporter.exportedImage(fid, (ext == "tlv") ? TZP_TYPE : OVL_TYPE);
}

//---------------------------------------------------------------------------

// esporta il livello corrente in un formato full color. se forRetas e' true,
// viene esportato
// in modo adatto ad essere usato in Retas, e cioe':
//- messo il bianco al posto del transparente
//- se c'e' bianco nell'immagine, viene "sporcato"(altrimenti poi viene letto
// come trasparente da retas)
//- l'output e' solo tga a 24bit compressed nel formato pippo0001.tga

bool IoCmd::exportLevel(const TFilePath &path, TXshSimpleLevel *sl,
                        ExportLevelOptions opts,
                        OverwriteCallbacks *overwriteCB,
                        ProgressCallbacks *progressCB) {
  struct Locals {
    const TFilePath &m_path;
    TXshSimpleLevel *m_sl;
    const ExportLevelOptions &m_opts;

    OverwriteCallbacks *m_overwriteCB;
    ProgressCallbacks *m_progressCB;

    bool exportToMultifile() {
      ImageExporter exporter(*m_sl, m_opts);

      TXshLevelType outputLevelType =
          (m_path.getType() == "tlv") ? TZP_TYPE : OVL_TYPE;

      bool firstTime = true;
      for (int i = 0; i < m_sl->getFrameCount(); ++i) {
        if (m_progressCB->canceled()) return false;

        // Prepare frame export path
        TFilePath fpout;

        if (m_opts.m_forRetas) {
          QString pathOut =
              QString::fromStdWString(m_path.getParentDir().getWideString()) +
              "\\" + QString::fromStdString(m_path.getName()) +
              QString::fromStdString(m_sl->index2fid(i).expand()) + "." +
              QString::fromStdString(m_path.getType());
          fpout = TFilePath(pathOut.toStdString());
        } else
          fpout = TFilePath(m_path.withFrame(m_sl->index2fid(i)));

        // Ask for overwrite permission in case a level with the built path
        // already exists
        if (firstTime) {
          firstTime = false;

          if (TSystem::doesExistFileOrLevel(fpout)) {
            QApplication::restoreOverrideCursor();
            bool overwrite = m_overwriteCB->overwriteRequest(fpout);
            QApplication::setOverrideCursor(Qt::WaitCursor);

            if (!overwrite) return false;
          }
        }

        // Retrieve the image to export at current frame
        TImageP img =
            exporter.exportedImage(m_sl->index2fid(i), outputLevelType);
        assert(img);

        // Save the prepared fullcolor image to file
        TImageWriter iw(fpout);
        iw.setProperties(m_opts.m_props);
        iw.save(img);

        m_progressCB->setValue(i + 1);
      }

      return true;
    }

    bool exportToTlv() {
      TFilePath fp(m_path.withNoFrame());

      // Remove any existing level
      if (TSystem::doesExistFileOrLevel(fp)) {
        bool overwrite = m_overwriteCB->overwriteRequest(fp);
        if (!overwrite) return false;

        TSystem::deleteFile(fp);
      }

      TSystem::removeFileOrLevel(fp.withType("tpl"));

      // Export level
      TLevelWriterP lw(fp);

      ImageExporter exporter(*m_sl, m_opts);

      for (int i = 0; i < m_sl->getFrameCount(); ++i) {
        if (m_progressCB->canceled()) return false;

        const TFrameId &fid = m_sl->index2fid(i);

        TImageP img = exporter.exportedImage(fid, TZP_TYPE);
        assert(img);

        lw->getFrameWriter(fid)->save(img);

        m_progressCB->setValue(i + 1);
      }

      return true;
    }
  };  // Locals

  // Use default values in case some were not specified by input

  // Level
  if (!sl) {
    sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();

    if (!sl) {
      DVGui::error(QObject::tr("No level selected!"));
      return false;
    }
  }

  if (sl->isEmpty()) return false;

  // Output properties
  if (!opts.m_props) {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    opts.m_props =
        scene->getProperties()->getOutputProperties()->getFileFormatProperties(
            path.getType());
  }

  // Camera (todo)
  assert(opts.m_camera.getRes().lx > 0 && opts.m_camera.getRes().ly > 0);

  // Callbacks
  std::unique_ptr<OverwriteCallbacks> overwriteDefault(
      overwriteCB ? 0 : (overwriteCB = new ExportOverwriteCB()));
  std::unique_ptr<ProgressCallbacks> progressDefault(
      progressCB ? 0 : (progressCB = new ExportProgressCB()));

  // Initialize variables
  Locals locals = {path, sl, opts, overwriteCB, progressCB};

  progressCB->setProcessedName(QString::fromStdWString(path.getWideString()));
  progressCB->setRange(0, sl->getFrameCount());

  // Export level
  BusyCursorOverride cursorOverride;
  QCoreApplication::processEvents();  // Refresh screen.   ...But WHY is this
                                      //                      necessary?
  try {
    return (path.getType() == "tlv") ? assert(sl->getType() == PLI_XSHLEVEL),
           locals.exportToTlv() : locals.exportToMultifile();
  } catch (...) {
    return false;
  }
}
