

#include "toonz/sceneproperties.h"

// TnzLib includes
#include "toonz/cleanupparameters.h"
#include "toonz/tcenterlinevectorizer.h"
#include "toonz/captureparameters.h"
#include "toonz/tcamera.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/txshleveltypes.h"
#include "toonz/preferences.h"
#include "cleanuppalette.h"

// TnzBase includes
#include "toutputproperties.h"
#include "trasterfx.h"
#include "tscanner.h"

// TnzCore includes
#include "tstream.h"
#include "tpalette.h"
#include "tproperty.h"
#include "tiio.h"

//=============================================================================

TSceneProperties::TSceneProperties()
    : m_cleanupParameters(new CleanupParameters())
    , m_scanParameters(new TScannerParameters())
    , m_vectorizerParameters(new VectorizerParameters())
    , m_captureParameters(new CaptureParameters())
    , m_outputProp(new TOutputProperties())
    , m_previewProp(new TOutputProperties())
    , m_bgColor(255, 255, 255, 0)
    , m_markerDistance(6)
    , m_markerOffset(0)
    , m_fullcolorSubsampling(1)
    , m_tlvSubsampling(1)
    , m_fieldGuideSize(16)
    , m_fieldGuideAspectRatio(1.77778)
    , m_columnColorFilterOnRender(false)
    , m_camCapSaveInPath() {
  // Default color
  m_notesColor.push_back(TPixel32(255, 235, 140));
  m_notesColor.push_back(TPixel32(255, 160, 120));
  m_notesColor.push_back(TPixel32(255, 180, 190));
  m_notesColor.push_back(TPixel32(135, 205, 250));
  m_notesColor.push_back(TPixel32(145, 240, 145));
  m_notesColor.push_back(TPixel32(130, 255, 210));
  m_notesColor.push_back(TPixel32(150, 245, 255));
}

//-----------------------------------------------------------------------------

TSceneProperties::~TSceneProperties() {
  delete m_cleanupParameters;
  delete m_scanParameters;
  delete m_vectorizerParameters;
  delete m_captureParameters;
  clearPointerContainer(m_cameras);
  delete m_outputProp;
  delete m_previewProp;
}

//-----------------------------------------------------------------------------

void TSceneProperties::assign(const TSceneProperties *sprop) {
  assert(sprop);

  m_hGuides      = sprop->m_hGuides;
  m_vGuides      = sprop->m_vGuides;
  *m_outputProp  = *sprop->m_outputProp;
  *m_previewProp = *sprop->m_previewProp;

  m_cleanupParameters->assign(sprop->m_cleanupParameters);
  m_scanParameters->assign(sprop->m_scanParameters);

  assert(sprop->m_vectorizerParameters);
  *m_vectorizerParameters = *sprop->m_vectorizerParameters;

  if (sprop != this) {
    m_cameras = sprop->m_cameras;
    for (int i     = 0; i < (int)m_cameras.size(); i++)
      m_cameras[i] = new TCamera(*m_cameras[i]);
  }
  m_bgColor                   = sprop->m_bgColor;
  m_markerDistance            = sprop->m_markerDistance;
  m_markerOffset              = sprop->m_markerOffset;
  m_fullcolorSubsampling      = sprop->m_fullcolorSubsampling;
  m_tlvSubsampling            = sprop->m_tlvSubsampling;
  m_fieldGuideSize            = sprop->m_fieldGuideSize;
  m_fieldGuideAspectRatio     = sprop->m_fieldGuideAspectRatio;
  m_columnColorFilterOnRender = sprop->m_columnColorFilterOnRender;
  int i;
  for (i = 0; i < m_notesColor.size(); i++)
    m_notesColor.replace(i, sprop->getNoteColor(i));
}

//-----------------------------------------------------------------------------

void TSceneProperties::onInitialize() {
  // set initial output folder to $scenefolder when the scene folder mode is set
  // in user preferences
  if (Preferences::instance()->getPathAliasPriority() ==
          Preferences::SceneFolderAlias &&
      !TFilePath("$scenefolder").isAncestorOf(m_outputProp->getPath())) {
    std::string ext = m_outputProp->getPath().getDottedType();
    m_outputProp->setPath(TFilePath("$scenefolder/") + ext);
  }

  //  m_scanParameters->adaptToCurrentScanner();
}

//-----------------------------------------------------------------------------

void TSceneProperties::setBgColor(const TPixel32 &color) { m_bgColor = color; }

//-----------------------------------------------------------------------------

void TSceneProperties::setMarkers(int distance, int offset) {
  m_markerDistance = distance;
  m_markerOffset   = offset;
}

//-----------------------------------------------------------------------------

void TSceneProperties::setFullcolorSubsampling(int s) {
  assert(1 <= s && s <= 100);
  m_fullcolorSubsampling = tcrop(s, 1, 100);
}

//-----------------------------------------------------------------------------

void TSceneProperties::setTlvSubsampling(int s) {
  assert(1 <= s && s <= 100);
  m_tlvSubsampling = tcrop(s, 1, 100);
}

//-----------------------------------------------------------------------------

void TSceneProperties::setFieldGuideSize(int size) {
  assert(1 <= size && size <= 100);
  m_fieldGuideSize = tcrop(size, 1, 100);
}

//-----------------------------------------------------------------------------

void TSceneProperties::setFieldGuideAspectRatio(double ar) {
  assert(ar >= 0);
  if (ar <= 0) ar         = 1;
  m_fieldGuideAspectRatio = ar;
}

//-----------------------------------------------------------------------------

void TSceneProperties::saveData(TOStream &os) const {
  if (!m_hGuides.empty()) {
    os.openChild("hGuides");
    for (int i = 0; i < (int)m_hGuides.size(); i++) os << m_hGuides[i];
    os.closeChild();
  }
  if (!m_vGuides.empty()) {
    os.openChild("vGuides");
    for (int i = 0; i < (int)m_vGuides.size(); i++) os << m_vGuides[i];
    os.closeChild();
  }

  int i;
  if (!m_cameras.empty()) {
    os.openChild("cameras");
    for (i = 0; i < (int)m_cameras.size(); i++) {
      os.openChild("camera");
      m_cameras[i]->saveData(os);
      os.closeChild();
    }
    os.closeChild();
  }

  os.openChild("outputs");
  std::vector<TOutputProperties *> outputs;
  outputs.push_back(getOutputProperties());
  outputs.push_back(getPreviewProperties());
  for (i = 0; i < (int)outputs.size(); i++) {
    TOutputProperties &out    = *outputs[i];
    const TRenderSettings &rs = out.getRenderSettings();
    std::map<std::string, std::string> attr;
    attr["name"] = i == 0 ? "main" : "preview";
    os.openChild("output", attr);

    TFilePath outPath = out.getPath();
    int from, to, step;
    out.getRange(from, to, step);
    os.child("range") << from << to;
    os.child("step") << step;
    os.child("shrink") << rs.m_shrinkX;
    os.child("applyShrinkToViewer") << (rs.m_applyShrinkToViewer ? 1 : 0);
    os.child("fps") << out.getFrameRate();
    os.child("path") << outPath;
    os.child("bpp") << rs.m_bpp;
    os.child("multimedia") << out.getMultimediaRendering();
    os.child("threadsIndex") << out.getThreadIndex();
    os.child("maxTileSizeIndex") << out.getMaxTileSizeIndex();
    os.child("subcameraPrev") << (out.isSubcameraPreview() ? 1 : 0);
    os.child("stereoscopic") << (rs.m_stereoscopic ? 1 : 0)
                             << rs.m_stereoscopicShift;

    switch (rs.m_quality) {
    case TRenderSettings::StandardResampleQuality:
      os.child("resquality") << (int)0;
      break;
    case TRenderSettings::ImprovedResampleQuality:
      os.child("resquality") << (int)1;
      break;
    case TRenderSettings::HighResampleQuality:
      os.child("resquality") << (int)2;
      break;
    case TRenderSettings::Triangle_FilterResampleQuality:
      os.child("resquality") << (int)100;
      break;
    case TRenderSettings::Mitchell_FilterResampleQuality:
      os.child("resquality") << (int)101;
      break;
    case TRenderSettings::Cubic5_FilterResampleQuality:
      os.child("resquality") << (int)102;
      break;
    case TRenderSettings::Cubic75_FilterResampleQuality:
      os.child("resquality") << (int)103;
      break;
    case TRenderSettings::Cubic1_FilterResampleQuality:
      os.child("resquality") << (int)104;
      break;
    case TRenderSettings::Hann2_FilterResampleQuality:
      os.child("resquality") << (int)105;
      break;
    case TRenderSettings::Hann3_FilterResampleQuality:
      os.child("resquality") << (int)106;
      break;
    case TRenderSettings::Hamming2_FilterResampleQuality:
      os.child("resquality") << (int)107;
      break;
    case TRenderSettings::Hamming3_FilterResampleQuality:
      os.child("resquality") << (int)108;
      break;
    case TRenderSettings::Lanczos2_FilterResampleQuality:
      os.child("resquality") << (int)109;
      break;
    case TRenderSettings::Lanczos3_FilterResampleQuality:
      os.child("resquality") << (int)110;
      break;
    case TRenderSettings::Gauss_FilterResampleQuality:
      os.child("resquality") << (int)111;
      break;
    case TRenderSettings::ClosestPixel_FilterResampleQuality:
      os.child("resquality") << (int)112;
      break;
    case TRenderSettings::Bilinear_FilterResampleQuality:
      os.child("resquality") << (int)113;
      break;
    default:
      assert(false);
    }
    switch (rs.m_fieldPrevalence) {
    case TRenderSettings::NoField:
      os.child("fieldprevalence") << (int)0;
      break;
    case TRenderSettings::EvenField:
      os.child("fieldprevalence") << (int)1;
      break;
    case TRenderSettings::OddField:
      os.child("fieldprevalence") << (int)2;
      break;
    default:
      assert(false);
    }
    os.child("gamma") << rs.m_gamma;
    os.child("timestretch") << rs.m_timeStretchFrom << rs.m_timeStretchTo;

    if (out.getOffset() != 0) os.child("offset") << out.getOffset();

    os.openChild("formatsProperties");
    std::vector<std::string> fileExtensions;
    out.getFileFormatPropertiesExtensions(fileExtensions);
    for (int i = 0; i < (int)fileExtensions.size(); i++) {
      std::string ext    = fileExtensions[i];
      TPropertyGroup *pg = out.getFileFormatProperties(ext);
      assert(pg);
      std::map<std::string, std::string> attr;
      attr["ext"] = ext;
      os.openChild("formatProperties", attr);
      pg->saveData(os);
      os.closeChild();
    }
    os.closeChild();

    os.closeChild();  // </output>
  }
  os.closeChild();
  os.openChild("cleanupParameters");
  m_cleanupParameters->saveData(os);
  os.closeChild();
  os.openChild("scanParameters");
  m_scanParameters->saveData(os);
  os.closeChild();
  os.openChild("vectorizerParameters");
  m_vectorizerParameters->saveData(os);
  os.closeChild();
  os.child("bgColor") << m_bgColor;
  os.child("markers") << m_markerDistance << m_markerOffset;
  os.child("subsampling") << m_fullcolorSubsampling << m_tlvSubsampling;
  os.child("fieldguide") << m_fieldGuideSize << m_fieldGuideAspectRatio;
  if (m_columnColorFilterOnRender) os.child("columnColorFilterOnRender") << 1;
  if (!m_camCapSaveInPath.isEmpty())
    os.child("cameraCaputureSaveInPath") << m_camCapSaveInPath;

  os.openChild("noteColors");
  for (i = 0; i < m_notesColor.size(); i++) os << m_notesColor.at(i);
  os.closeChild();
}

//-----------------------------------------------------------------------------
/*! Set all scene properties elements, to information contained in \b TIStream
   \b is.
                \sa saveData()
*/
void TSceneProperties::loadData(TIStream &is, bool isLoadingProject) {
  TSceneProperties defaultProperties;
  assign(&defaultProperties);

  int globFrom = -1, globTo = 0, globStep = 1;
  double globFrameRate = -1;
  std::string tagName;
  *m_outputProp = *m_previewProp = TOutputProperties();
  while (is.matchTag(tagName)) {
    if (tagName == "projectPath") {
      TFilePath projectPath;
      is >> projectPath;
    } else if (tagName == "range") {
      is >> globFrom >> globTo;
    }  // backCompatibility: prima range e fps non erano in Output
    else if (tagName == "step") {
      is >> globStep;
    } else if (tagName == "fps") {
      is >> globFrameRate;
    } else if (tagName == "bgColor") {
      is >> m_bgColor;
    } else if (tagName == "viewerBgColor" || tagName == "previewBgColor" ||
               tagName == "chessboardColor1" ||
               tagName == "chessboardColor2")  // back compatibility
    {
      TPixel32 dummy;
      is >> dummy;
    } else if (tagName == "markers") {
      is >> m_markerDistance >> m_markerOffset;
    } else if (tagName == "subsampling") {
      is >> m_fullcolorSubsampling >> m_tlvSubsampling;
    } else if (tagName == "fieldguide") {
      is >> m_fieldGuideSize >> m_fieldGuideAspectRatio;
    } else if (tagName == "columnColorFilterOnRender") {
      int val;
      is >> val;
      enableColumnColorFilterOnRender(val == 1);
    } else if (tagName == "safearea") {
      double dummy1, dummy2;
      is >> dummy1 >> dummy2;
    }  // back compatibility
    else if (tagName == "columnIconLoadingPolicy") {
      int dummy;
      is >> dummy;
    }                                 // back compatibility
    else if (tagName == "playrange")  // back compatibility
    {
      std::string dummy;
      is >> globFrom >> globTo >> dummy;
    } else if (tagName == "camera")  // back compatibility with tab 2.2
    {
      clearPointerContainer(m_cameras);
      m_cameras.clear();
      TCamera *camera = new TCamera();
      m_cameras.push_back(camera);
      TDimension res(0, 0);
      is >> res.lx >> res.ly;
      camera->setRes(res);
      double cameraDpi = 36.0;
      camera->setSize(TDimensionD(res.lx / cameraDpi, res.ly / cameraDpi));
    } else if (tagName == "playRange") {
      int playR0, playR1;
      is >> playR0 >> playR1;
    } else if (tagName == "hGuides" || tagName == "vGuides") {
      Guides &guides = tagName == "hGuides" ? m_hGuides : m_vGuides;
      while (!is.matchEndTag()) {
        double g;
        is >> g;
        guides.push_back(g);
      }
      continue;
    } else if (tagName == "cameras") {
      clearPointerContainer(m_cameras);
      m_cameras.clear();
      while (is.matchTag(tagName)) {
        if (tagName == "camera") {
          TCamera *camera = new TCamera();
          m_cameras.push_back(camera);
          camera->loadData(is);
        }  // if "camera"
        else
          throw TException("unexpected property tag: " + tagName);
        is.closeChild();
      }
    } else if (tagName == "outputs" || tagName == "outputStreams") {
      while (is.matchTag(tagName)) {
        if (tagName == "output" || tagName == "outputStream") {
          TOutputProperties dummyOut;
          TOutputProperties *outPtr = &dummyOut;
          std::string name          = is.getTagAttribute("name");
          if (name == "preview")
            outPtr = m_previewProp;
          else if (name == "main")
            outPtr               = m_outputProp;
          TOutputProperties &out = *outPtr;
          TRenderSettings renderSettings;
          if (globFrom != -1)
            out.setRange(globFrom, globTo, globStep);
          else if (globStep != 1) {
            int from, to, dummy;
            out.getRange(from, to, dummy);
            out.setRange(from, to, globStep);
          }

          if (globFrameRate != -1) out.setFrameRate(globFrameRate);
          while (is.matchTag(tagName)) {
            if (tagName == "camera") {
              int dummy;
              is >> dummy;
            }                                    // per compatibilita'
            else if (tagName == "cameraData") {  // per compatibilita'
              while (is.matchTag(tagName)) {
                if (tagName == "size") {
                  TDimensionD size(0, 0);
                  is >> size.lx >> size.ly;
                } else if (tagName == "res") {
                  TDimension res(0, 0);
                  is >> res.lx >> res.ly;
                }
                is.closeChild();
              }
            } else if (tagName == "path") {
              TFilePath fp;
              is >> fp;
              std::string ext    = fp.getUndottedType();
              TPropertyGroup *pg = out.getFileFormatProperties(ext);
              if (ext == "avi" && pg->getPropertyCount() != 1)
                fp = fp.withType("tif");
              out.setPath(fp);
            } else if (tagName == "offset") {
              int j;
              is >> j;
              out.setOffset(j);
            } else if (tagName == "range") {
              int from, to, step, dummy;
              is >> from >> to;
              out.getRange(dummy, dummy, step);
              out.setRange(from, to, step);
            } else if (tagName == "step") {
              int dummy, from, to, step;
              is >> step;
              out.getRange(from, to, dummy);
              out.setRange(from, to, step);
            } else if (tagName == "shrink") {
              int shrink;
              is >> shrink;
              renderSettings.m_shrinkX = renderSettings.m_shrinkY = shrink;
            } else if (tagName == "applyShrinkToViewer") {
              int applyShrinkToViewer;
              is >> applyShrinkToViewer;
              renderSettings.m_applyShrinkToViewer = (applyShrinkToViewer != 0);
            } else if (tagName == "fps") {
              double j;
              is >> j;
              out.setFrameRate(j);
            } else if (tagName == "bpp") {
              int j;
              is >> j;
              if (j == 32 || j == 64) renderSettings.m_bpp = j;
            } else if (tagName == "multimedia") {
              int j;
              is >> j;
              out.setMultimediaRendering(j);
            } else if (tagName == "threadsIndex") {
              int j;
              is >> j;
              out.setThreadIndex(j);
            } else if (tagName == "maxTileSizeIndex") {
              int j;
              is >> j;
              out.setMaxTileSizeIndex(j);
            } else if (tagName == "subcameraPrev") {
              int j;
              is >> j;
              out.setSubcameraPreview(j != 0);
            } else if (tagName == "resquality") {
              int j;
              is >> j;
              switch (j) {
              case 0:
                renderSettings.m_quality =
                    TRenderSettings::StandardResampleQuality;
                break;
              case 1:
                renderSettings.m_quality =
                    TRenderSettings::ImprovedResampleQuality;
                break;
              case 2:
                renderSettings.m_quality = TRenderSettings::HighResampleQuality;
                break;
              case 100:
                renderSettings.m_quality =
                    TRenderSettings::Triangle_FilterResampleQuality;
                break;
              case 101:
                renderSettings.m_quality =
                    TRenderSettings::Mitchell_FilterResampleQuality;
                break;
              case 102:
                renderSettings.m_quality =
                    TRenderSettings::Cubic5_FilterResampleQuality;
                break;
              case 103:
                renderSettings.m_quality =
                    TRenderSettings::Cubic75_FilterResampleQuality;
                break;
              case 104:
                renderSettings.m_quality =
                    TRenderSettings::Cubic1_FilterResampleQuality;
                break;
              case 105:
                renderSettings.m_quality =
                    TRenderSettings::Hann2_FilterResampleQuality;
                break;
              case 106:
                renderSettings.m_quality =
                    TRenderSettings::Hann3_FilterResampleQuality;
                break;
              case 107:
                renderSettings.m_quality =
                    TRenderSettings::Hamming2_FilterResampleQuality;
                break;
              case 108:
                renderSettings.m_quality =
                    TRenderSettings::Hamming3_FilterResampleQuality;
                break;
              case 109:
                renderSettings.m_quality =
                    TRenderSettings::Lanczos2_FilterResampleQuality;
                break;
              case 110:
                renderSettings.m_quality =
                    TRenderSettings::Lanczos3_FilterResampleQuality;
                break;
              case 111:
                renderSettings.m_quality =
                    TRenderSettings::Gauss_FilterResampleQuality;
                break;
              case 112:
                renderSettings.m_quality =
                    TRenderSettings::ClosestPixel_FilterResampleQuality;
                break;
              case 113:
                renderSettings.m_quality =
                    TRenderSettings::Bilinear_FilterResampleQuality;
                break;
              default:
                renderSettings.m_quality =
                    TRenderSettings::StandardResampleQuality;
              }
            } else if (tagName == "fieldprevalence") {
              int j;
              is >> j;
              switch (j) {
              case 0:
                renderSettings.m_fieldPrevalence = TRenderSettings::NoField;
                break;
              case 1:
                renderSettings.m_fieldPrevalence = TRenderSettings::EvenField;
                break;
              case 2:
                renderSettings.m_fieldPrevalence = TRenderSettings::OddField;
                break;
              default:
                renderSettings.m_fieldPrevalence = TRenderSettings::NoField;
                break;
              }
            } else if (tagName == "gamma") {
              double g;
              is >> g;
              renderSettings.m_gamma = g;
            } else if (tagName == "timestretch") {
              double from, to;
              is >> from >> to;
              renderSettings.m_timeStretchFrom = from;
              renderSettings.m_timeStretchTo   = to;
            } else if (tagName == "stereoscopic") {
              int doit;
              double val;
              is >> doit >> val;
              renderSettings.m_stereoscopic      = (doit == 1);
              renderSettings.m_stereoscopicShift = val;
            }

            // TODO: aggiungere la lettura della quality
            else if (tagName == "res") {  // obsoleto
              TDimension d(0, 0);
              is >> d.lx >> d.ly;
              if (!is.eos()) {
                std::string s;
                is >> s;
              }
            } else if (tagName == "formatsProperties") {
              while (is.matchTag(tagName)) {
                if (tagName == "formatProperties") {
                  std::string ext    = is.getTagAttribute("ext");
                  TPropertyGroup *pg = out.getFileFormatProperties(ext);
                  if (ext == "avi") {
                    TPropertyGroup appProperties;
                    appProperties.loadData(is);
                    if (pg->getPropertyCount() != 1 ||
                        appProperties.getPropertyCount() != 1) {
                      is.closeChild();
                      continue;
                    }
                    TEnumProperty *enumProp =
                        dynamic_cast<TEnumProperty *>(pg->getProperty(0));
                    TEnumProperty *enumAppProp = dynamic_cast<TEnumProperty *>(
                        appProperties.getProperty(0));
                    assert(enumAppProp && enumProp);
                    if (enumAppProp && enumProp) {
                      try {
                        enumProp->setValue(enumAppProp->getValue());
                      } catch (TProperty::RangeError &) {
                      }
                    } else
                      throw TException();
                  } else
                    pg->loadData(is);

                  ////////ここだ！
                  {
                    TPropertyGroup *refPg = Tiio::makeWriterProperties(ext);
                    pg->assignUINames(refPg);
                    delete refPg;
                  }

                  is.closeChild();
                } else
                  throw TException("unexpected tag: " + tagName);
              }  // end while
            } else {
              throw TException("unexpected property tag: " + tagName);
            }
            is.closeChild();
          }
          if (renderSettings.m_timeStretchFrom ==
                  renderSettings.m_timeStretchTo &&
              renderSettings.m_timeStretchTo == 1)
            renderSettings.m_timeStretchFrom = renderSettings.m_timeStretchTo =
                out.getFrameRate();

          out.setRenderSettings(renderSettings);
        } else
          throw TException("unexpected property tag: " + tagName);
        is.closeChild();
      }  // while (outputs/outputStreams)
    } else if (tagName == "cleanupPalette") {
      m_cleanupParameters->m_cleanupPalette->loadData(is);
    } else if (tagName == "cleanupParameters") {
      m_cleanupParameters->loadData(is, !isLoadingProject);
    } else if (tagName == "scanParameters") {
      // m_scanParameters->adaptToCurrentScanner(); Rallenta tutto!!!
      m_scanParameters->loadData(is);
    } else if (tagName == "vectorizerParameters") {
      m_vectorizerParameters->loadData(is);
    } else if (tagName == "captureParameters") {
      m_captureParameters->loadData(is);
    } else if (tagName == "defaultLevelParameters") {
      // this
      int type;
      double width, height, dpi;
      is >> type >> width >> height >> dpi;
    } else if (tagName == "noteColors") {
      int i = 0;
      while (!is.eos()) {
        TPixel32 color;
        is >> color;
        m_notesColor.replace(i, color);
        i++;
      }
      assert(i == 7);
    } else if (tagName == "cameraCaputureSaveInPath") {
      is >> m_camCapSaveInPath;
    } else {
      throw TException("unexpected property tag: " + tagName);
    }
    is.closeChild();
  }
}

//-----------------------------------------------------------------------------

void TSceneProperties::cloneCamerasFrom(TStageObjectTree *stageObjects) {
  clearPointerContainer(m_cameras);
  int cameraCount = stageObjects->getCameraCount();

  int tmpCameraId = 0;

  for (int i = 0; i < cameraCount;) {
    /*-- カメラが見つからなかった場合、tmpCameraIdのみ進める --*/
    if (!stageObjects->getStageObject(TStageObjectId::CameraId(tmpCameraId),
                                      false)) {
      tmpCameraId++;
      continue;
    }
    TStageObject *cameraObject =
        stageObjects->getStageObject(TStageObjectId::CameraId(tmpCameraId));
    TCamera *camera = new TCamera(*cameraObject->getCamera());
    m_cameras.push_back(camera);
    /*-- カメラが見つかったので、i も tmpCameraId も進める --*/
    i++;
    tmpCameraId++;
  }
}

//-----------------------------------------------------------------------------

void TSceneProperties::cloneCamerasTo(TStageObjectTree *stageObjects) const {
  TDimension maxCameraRes(0, 0);

  for (int i = 0; i < (int)m_cameras.size(); i++) {
    TStageObject *cameraPegbar =
        stageObjects->getStageObject(TStageObjectId::CameraId(i));
    TCamera *camera = m_cameras[i];

    TDimension cameraRes = camera->getRes();
    bool modified        = false;
    if (maxCameraRes.lx > 0 && cameraRes.lx > maxCameraRes.lx) {
      cameraRes.lx = maxCameraRes.lx;
      modified     = true;
    }
    if (maxCameraRes.ly > 0 && cameraRes.ly > maxCameraRes.ly) {
      cameraRes.ly = maxCameraRes.ly;
      modified     = true;
    }
    if (modified) camera->setRes(cameraRes);

    *cameraPegbar->getCamera() = *m_cameras[i];
  }
}

//-----------------------------------------------------------------------------

QList<TPixel32> TSceneProperties::getNoteColors() const { return m_notesColor; }

//-----------------------------------------------------------------------------

TPixel32 TSceneProperties::getNoteColor(int colorIndex) const {
  return m_notesColor[colorIndex];
}

//-----------------------------------------------------------------------------

void TSceneProperties::setNoteColor(TPixel32 color, int colorIndex) {
  m_notesColor[colorIndex] = color;
}
