#include "ocaio.h"

#include "tsystem.h"
#include "tenv.h"

#include "toonz/tcamera.h"

#include "toonz/toonzscene.h"
#include "toonz/tproject.h"
#include "toonz/levelset.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/preferences.h"
#include "toonz/preferencesitemids.h"
#include "toonz/sceneproperties.h"
#include "toonz/tstageobject.h"
#include "toutputproperties.h"
#include "toonz/txshlevelcolumn.h"

#include "toonzqt/gutil.h"

#include "tapp.h"
#include "menubarcommandids.h"
#include "filebrowserpopup.h"

#include "tiio.h"
#include "timage_io.h"
#include "tlevel_io.h"
#include "tvectorimage.h"
#include "exportlevelcommand.h"
#include "iocommand.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDesktopServices>
#include <QCoreApplication>

#include <QWidget>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QVBoxLayout>

using namespace OCAIo;

void OCAData::write(QJsonObject &json) const {
  // Header
  json["name"]       = m_name;
  json["frameRate"]  = m_framerate;
  json["width"]      = m_width;
  json["height"]     = m_height;
  json["startTime"]  = m_startTime;
  json["endTime"]    = m_endTime;
  json["colorDepth"] = m_raEXR ? "U16" : "U8";

  // Background color
  QJsonArray bgColorArray;
  bgColorArray.append(m_bgRed);
  bgColorArray.append(m_bgGreen);
  bgColorArray.append(m_bgBlue);
  bgColorArray.append(m_bgAlpha);
  json["backgroundColor"] = bgColorArray;

  // Layers
  json["layers"] = m_layers;

  // Versions
  json["originApp"] = "OpenToonz";
  json["originAppVersion"] =
      QString::fromStdString(TEnv::getApplicationVersion());
  json["ocaVersion"] = "1.1.0";
}

int OCAData::frameLen(TXshCellColumn *column, const QList<int> &rows,
                      int index) {
  // Next cells must match same level and frame-id
  int length          = 0;
  const TXshCell &stc = column->getCell(rows[index]);
  for (int i = index; i < rows.count(); i++) {
    int currentRow       = rows[i];
    const TXshCell &cell = column->getCell(currentRow);
    if (stc.m_frameId != cell.m_frameId || stc.m_level != cell.m_level) break;
    length++;
  }
  return length;
}

bool OCAData::isBlank(TXshCellColumn *column, int row) {
  TXshCell cell = column->getCell(row);
  return cell.isEmpty();
}

bool OCAData::getLayerName(TXshCellColumn *column, QString &out) {
  // Layer name will be the first cel that occur on the column
  TXshCell cell = column->getCell(column->getFirstRow());
  if (cell.isEmpty()) return false;

  out = QString::fromStdWString(cell.m_level->getName());
  return true;
}

bool OCAData::getCellName(TXshCellColumn *column, int row, QString &out) {
  TXshCell cell = column->getCell(row);
  if (cell.isEmpty()) return false;

  TFilePath fp(cell.m_level->getName());
  fp  = fp.withFrame(cell.getFrameId(), TFrameId::FrameFormat::FOUR_ZEROS);
  out = fp.getQString();
  return true;
}

bool OCAData::saveCell(TXshCellColumn *column, int row, const QString &cellname,
                       OCAAsset &out) {
  TXshCell cell       = column->getCell(row);
  TXshSimpleLevel *sl = cell.getSimpleLevel();

  std::string format = m_raEXR ? "exr" : "png";

  // Rasterize cell
  TImageP image = IoCmd::exportedImage(format, *sl, cell.getFrameId());
  if (!image) return false;

  if (TRasterImageP ri = (TRasterImageP)(image)) {
    TRasterP raster;
    raster     = ri->getRaster();
    out.width  = raster->getLx();
    out.height = raster->getLy();
    if (!raster) return false;
  } else {
    return false;
  }

  out.fileExt = QString::fromStdString(format);

  if (m_veSVG) {
    // Get vector image directly and discard rasterized image
    TVectorImageP vectorImg = cell.getImage(false);
    if (vectorImg) {
      image       = vectorImg;
      out.fileExt = "svg";
    }
  }

  QString filename = QString("/%1.%2").arg(cellname, out.fileExt);
  TFilePath fpr(m_path);
  TFilePath fpa(m_path + filename);
  out.fileName = QString::fromStdWString(fpr.getWideName()) + filename;

  TLevelWriterP lw(fpa);
  lw->setFrameRate(m_framerate);
  TImageWriterP iw = lw->getFrameWriter(cell.getFrameId());
  iw->setFilePath(fpa);  // Added to aid my own sanity!
  iw->save(image);

  return true;
}

bool OCAData::isGroup(TXshCellColumn *column) {
  TXshCell firstcell = column->getCell(column->getFirstRow());
  TXshChildLevel *cl = firstcell.getChildLevel();
  return cl;
}

bool OCAData::buildGroup(QJsonObject &json, const QList<int> &rows,
                         TXshCellColumn *column, bool exportReferences) {
  QString layername;
  if (!getLayerName(column, layername)) return false;

  // Get sub-xsheet
  int firstrow       = column->getFirstRow();
  TXshCell firstcell = column->getCell(firstrow);
  TXshChildLevel *cl = firstcell.getChildLevel();
  TXsheet *xsheet    = cl->getXsheet();
  if (!xsheet) return false;

  // Build a list of child rows
  QList<int> crows;
  for (int i = m_startTime; i <= m_endTime; i++) {
    TXshCell cell = column->getCell(i);
    if (cell.isEmpty())
      crows.append(-1);
    else
      crows.append(cell.getFrameId().getNumber() - 1);
  }

  // Build all columns from sub-xsheet
  int subID = ++m_subId;
  QJsonArray layers;
  for (int col = 0; col < xsheet->getFirstFreeColumnIndex(); col++) {
    if (xsheet->isColumnEmpty(col)) continue;
    TXshCellColumn *column = xsheet->getColumn(col)->getCellColumn();
    if (!column) continue;                      // skip non-cell column
    if (!exportReferences && !column->isPreviewVisible())
      continue;  // skip inactive column

    if (column->getColumnType() == column->eLevelType) {
      QJsonObject json;
      if (isGroup(column)) {
        if (buildGroup(json, crows, column, exportReferences))
          layers.append(json);
      } else {
        if (buildLayer(json, crows, column)) layers.append(json);
      }
    }
  }

  QJsonArray jsonBlankPos;
  jsonBlankPos.append(m_width / 2);
  jsonBlankPos.append(m_height / 2);
  json["name"]         = layername;
  json["position"]     = jsonBlankPos;
  json["width"]        = m_width;
  json["height"]       = m_height;
  json["frames"]       = QJsonArray();
  json["childLayers"]  = layers;
  json["type"]         = "grouplayer";
  json["fileType"]     = "png";
  json["blendingMode"] = "normal";
  json["animated"]     = false;
  json["label"]        = 0;
  json["opacity"]      = column->getOpacity() / 255.0;
  json["visible"]      = column->isCamstandVisible();
  json["passThrough"]  = false;
  json["reference"]    = !column->isPreviewVisible();
  json["inheritAlpha"] = false;

  return true;
}

bool OCAData::buildLayer(QJsonObject &json, const QList<int> &rows,
                         TXshCellColumn *column) {
  QJsonArray jsonFrames;
  bool firstcel = true;

  // Layer name will be the first cel that occur on the column
  int firstrow = column->getFirstRow();
  QString layername;
  QString cellname;
  if (!getLayerName(column, layername)) return false;
  if (!getCellName(column, firstrow, cellname)) return false;

  QJsonArray jsonBlankPos;
  jsonBlankPos.append(m_width / 2);
  jsonBlankPos.append(m_height / 2);
  json["name"]     = layername;
  json["position"] = jsonBlankPos;
  json["width"]    = 0;
  json["height"]   = 0;

  for (int i = 0; i < rows.count(); i++) {
    int row = rows[i];

    QJsonObject frame;
    QJsonArray jsonPosition;
    jsonPosition.append(m_width / 2);
    jsonPosition.append(m_height / 2);
    int len = frameLen(column, rows, i);

    frame["frameNumber"] = i;
    frame["position"]    = jsonPosition;
    frame["opacity"]     = 1.0;  // OT uses Transparency Fx node
    frame["duration"]    = len;

    if (isBlank(column, row)) {
      frame["name"]     = "_blank";
      frame["fileName"] = "";
      frame["width"]    = 0;
      frame["height"]   = 0;
    } else {
      // Cell name will be used for the frame name
      if (!getCellName(column, row, cellname)) continue;

      // Save cell image as an asset
      OCAAsset asset;
      if (!m_assets.contains(cellname)) {
        if (!saveCell(column, row, cellname, asset)) return false;
        m_assets.insert(cellname, asset);
      } else {
        asset = m_assets.value(cellname);
      }

      frame["name"]     = cellname;
      frame["fileName"] = asset.fileName;
      frame["width"]    = asset.width;
      frame["height"]   = asset.height;

      if (firstcel) {
        // Layer size and type will depend of the first cell too
        json["width"]    = asset.width;
        json["height"]   = asset.height;
        json["fileType"] = asset.fileExt;
        firstcel         = false;
      }
    }
    jsonFrames.append(frame);

    i += len - 1;
  }

  json["frames"]       = jsonFrames;
  json["childLayers"]  = QJsonArray();
  json["type"]         = json["fileType"] == "svg" ? "vectorlayer" : "paintlayer";
  json["blendingMode"] = "normal";  // OT uses nodes Fx to make blending
                                    // possible, how to approach this?
  json["animated"]     = jsonFrames.count() >= 2;
  json["label"]        = 0;  // OT doesn't support labels
  json["opacity"]      = column->getOpacity() / 255.0;
  json["visible"]      = column->isCamstandVisible();
  json["passThrough"]  = false;
  json["reference"]    = !column->isPreviewVisible();
  json["inheritAlpha"] = false;

  return true;
}

void OCAData::setProgressDialog(DVGui::ProgressDialog *dialog) {
  m_progressDialog = dialog;
}

void OCAData::build(ToonzScene *scene, TXsheet *xsheet, QString name,
                    QString path, bool useEXR, bool vectorAsSVG,
                    bool exportReferences) {
  m_name  = name;
  m_path  = path;
  m_subId = 0;
  m_raEXR = useEXR;
  m_veSVG = vectorAsSVG;

  // if the current xsheet is top xsheet in the scene and the output
  // frame range is specified, set the "to" frame value as duration
  TOutputProperties *oprop = scene->getProperties()->getOutputProperties();
  int from, to, step;
  if (scene->getTopXsheet() == xsheet && oprop->getRange(from, to, step)) {
    m_startTime = from;
    m_endTime   = to;
    // m_stepTime  = step;
  } else {
    m_startTime = 0;
    m_endTime   = xsheet->getFrameCount() - 1;
    // m_stepTime  = 1;
  }
  if (m_endTime < 0) m_endTime = 0;

  // Build a list of rows
  QList<int> rows;
  for (int i = m_startTime; i <= m_endTime; i++) {
    rows.append(i);
  }

  m_framerate = oprop->getFrameRate();
  m_width     = scene->getCurrentCamera()->getRes().lx;
  m_height    = scene->getCurrentCamera()->getRes().ly;

  TPixel bgc = scene->getProperties()->getBgColor();
  m_bgRed    = bgc.r / double(TPixel::maxChannelValue);
  m_bgGreen  = bgc.g / double(TPixel::maxChannelValue);
  m_bgBlue   = bgc.b / double(TPixel::maxChannelValue);
  m_bgAlpha  = bgc.m / double(TPixel::maxChannelValue);

  // Build all columns from current xsheet
  m_layers.empty();
  for (int col = 0; col < xsheet->getColumnCount(); col++) {
    if (xsheet->isColumnEmpty(col)) continue;
    TXshCellColumn *column = xsheet->getColumn(col)->getCellColumn();
    if (!column) continue;                      // skip non-cell column
    if (!exportReferences && !column->isPreviewVisible())
      continue;  // skip inactive column

    if (column->getColumnType() == column->eLevelType) {
      if (m_progressDialog) {
        QString layername;
        getLayerName(column, layername);
        m_progressDialog->setLabelText(QObject::tr("Layer: ") + layername);
        m_progressDialog->setMaximum(xsheet->getColumnCount());
        m_progressDialog->setValue(col);
        QCoreApplication::processEvents();
      }

      QJsonObject json;
      if (isGroup(column)) {
        if (buildGroup(json, rows, column, exportReferences))
          m_layers.append(json);
      } else {
        if (buildLayer(json, rows, column)) m_layers.append(json);
      }
    }
  }
}

ExportOCACommand::ExportOCACommand() : MenuItemHandler(MI_ExportOCA) {}

void ExportOCACommand::execute() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TXsheet *xsheet   = TApp::instance()->getCurrentXsheet()->getXsheet();
  TFilePath fp      = scene->getScenePath().withType("oca");

  static GenericSaveFilePopup *savePopup = 0;
  static QCheckBox *exrImageFormat       = nullptr;
  static QCheckBox *rasVectors           = nullptr;
  static QCheckBox *doKeyframing         = nullptr;
  static QCheckBox *exportReferences     = nullptr;

  static DVGui::ProgressDialog *progressDialog = nullptr;

  if (!savePopup) {
    QWidget *customWidget = new QWidget();

    exrImageFormat   = new QCheckBox(tr("Save Images in EXR Format"));
    rasVectors       = new QCheckBox(tr("Rasterize Vectors"));
    exportReferences = new QCheckBox(tr("Export Reference Layers"));

    exrImageFormat->setToolTip(
        tr("Checked: Images are saved as EXR\nUnchecked: Images are "
           "saved as PNG"));
    rasVectors->setToolTip(
        tr("Checked: Rasterize into EXR/PNG\nUnchecked: Vectors are "
           "saved as SVG"));
    exportReferences->setToolTip(
        tr("Checked: Layers with Preview Visible OFF are also "
           "exported\nUnchecked: Only layers with Preview Visible ON are "
           "exported"));
    rasVectors->setChecked(true);

    QGridLayout *customLay = new QGridLayout();
    customLay->setMargin(5);
    customLay->setSpacing(5);
    customLay->addWidget(exrImageFormat, 0, 0);
    customLay->addWidget(rasVectors, 1, 0);
    customLay->addWidget(exportReferences, 2, 0);
    customWidget->setLayout(customLay);

    progressDialog = new DVGui::ProgressDialog("", tr("Hide"), 0, 0);
    progressDialog->setWindowTitle(tr("Exporting..."));
    progressDialog->setWindowFlags(
        Qt::Dialog | Qt::WindowTitleHint);  // Don't show ? and X buttons
    progressDialog->setWindowModality(Qt::WindowModal);

    savePopup = new GenericSaveFilePopup(
        QObject::tr("Export Open Cel Animation (OCA)"), customWidget);
    savePopup->addFilterType("oca");
  }
  if (!scene->isUntitled())
    savePopup->setFolder(fp.getParentDir());
  else
    savePopup->setFolder(
        TProjectManager::instance()->getCurrentProject()->getScenesPath());

  savePopup->setFilename(fp.withoutParentDir());
  fp = savePopup->getPath();
  if (fp.isEmpty()) return;

  bool exrImageFmt = exrImageFormat->isChecked();
  bool rasterVecs  = rasVectors->isChecked();
  bool exportRefs  = exportReferences->isChecked();

  // Export

  progressDialog->setLabelText(tr("Starting..."));
  progressDialog->setValue(0);
  progressDialog->show();
  QCoreApplication::processEvents();

  QString ocafile = fp.getQString();
  QString ocafolder(ocafile);
  ocafolder.replace(".", "_");

  QFile saveFile(ocafile);
  QDir saveDir(ocafolder);

  if (!saveFile.open(QIODevice::WriteOnly)) {
    progressDialog->close();
    DVGui::error(QObject::tr("Unable to open OCA file for saving."));
    return;
  }
  if (!saveDir.exists()) {
    if (!saveDir.mkpath(".")) {
      progressDialog->close();
      DVGui::error(QObject::tr("Unable to create folder for saving layers."));
      return;
    }
  }

  OCAData ocaData;
  ocaData.setProgressDialog(progressDialog);
  ocaData.build(scene, xsheet, QString::fromStdString(fp.getName()), ocafolder,
                exrImageFmt, !rasterVecs, exportRefs);
  if (ocaData.isEmpty()) {
    progressDialog->close();
    DVGui::error(QObject::tr("No columns can be exported."));
    return;
  }

  QJsonObject ocaObject;
  ocaData.write(ocaObject);
  QJsonDocument saveDoc(ocaObject);
  QByteArray jsonByteArrayData = saveDoc.toJson();
  saveFile.write(jsonByteArrayData);

  progressDialog->close();

  // Done

  QString str = QObject::tr("%1 has been exported successfully.")
                    .arg(QString::fromStdString(fp.getLevelName()));

  std::vector<QString> buttons = {QObject::tr("OK"),
                                  QObject::tr("Open containing folder")};
  int ret = DVGui::MsgBox(DVGui::INFORMATION, str, buttons);

  if (ret == 2) {
    TFilePath folderPath = fp.getParentDir();
    if (TSystem::isUNC(folderPath))
      QDesktopServices::openUrl(QUrl(folderPath.getQString()));
    else
      QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath.getQString()));
  }
}

//-------------------------------------------------

class ImportOCACommand final : public MenuItemHandler {
public:
  ImportOCACommand() : MenuItemHandler(MI_ImportOCA) {}
  void execute() override;
} ImportOCACommand;

void ImportOCACommand::execute() {
  bool importEnabled = false;
  ToonzScene *scene  = TApp::instance()->getCurrentScene()->getScene();
  TXsheet *xsheet    = TApp::instance()->getCurrentXsheet()->getXsheet();
  TFilePath fp       = scene->getScenePath().withType("oca");
  static GenericLoadFilePopup *loadPopup = 0;
  if (!loadPopup) {
    loadPopup = new GenericLoadFilePopup(
        QObject::tr("Import Open Cel Animation (OCA)"));
    loadPopup->addFilterType("oca");
  }
  if (!scene->isUntitled())
    loadPopup->setFolder(fp.getParentDir());
  else
    loadPopup->setFolder(
        TProjectManager::instance()->getCurrentProject()->getScenesPath());

  loadPopup->setFilename(fp.withoutParentDir());
  fp = loadPopup->getPath();
  if (fp.isEmpty()) {
    DVGui::info(QObject::tr("OCA Import cancelled : empty filepath."));
    return;
  } else {
    DVGui::info(QObject::tr("OCA Import file : %1").arg(fp.getQString()));

    QString label = QObject::tr(
        "Do you want to import or load image files from their original "
        "location?");
    QStringList buttons;
    buttons << QObject::tr("Import") << QObject::tr("Load")
            << QObject::tr("Cancel");
    DVGui::Dialog *importDialog =
        DVGui::createMsgBox(DVGui::QUESTION, label, buttons, 0);
    int ret = importDialog->exec();
    importDialog->deleteLater();

    if (ret == 0 || ret == 3) return;

    importEnabled = (ret == 1);
  }

  QString ocafile           = fp.getQString();
  OCAInputData ocaInputData = OCAInputData(0, false, false);
  QJsonObject ocaObject;
  if (!ocaInputData.read(ocafile, ocaObject)) {
    DVGui::warning(
        QObject::tr("Failed to load OCA file: %1").arg(fp.getQString()));
    return;
  }
  ocaInputData.getSceneData();                  // gather default values
  ocaInputData.load(ocaObject, importEnabled);  // load resources
  ocaInputData.setSceneData();                  // set scene/xsheet values
  // TApp::instance()->getCurrentLevel()->notifyLevelChange(); > importOcaLayer
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifyCastChange();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

OCAIo::OCAInputData::OCAInputData(float antialiasSoftness, bool whiteTransp,
                                  bool doPremultiply)
    : m_antialiasSoftness(antialiasSoftness)
    , m_whiteTransp(whiteTransp)
    , m_doPremultiply(doPremultiply)
    , m_supressImportMessages(false) {
  m_scene  = TApp::instance()->getCurrentScene()->getScene();
  m_xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
  m_oprop  = m_scene->getProperties()->getOutputProperties();
  m_dpi    = Preferences::instance()->getDefLevelDpi();
}

bool OCAIo::OCAInputData::read(QString path, QJsonObject &json) {
  // see XdtsIo::loadXdtsScene
  m_path      = path;
  m_parentDir = TFilePath(m_path).getParentDir();
  // 1. Open the QFile, read it in a byteArray and close the file
  QFile file;
  file.setFileName(path);
  if (!file.open(QIODevice::ReadOnly)) {
    DVGui::error(QObject::tr("Unable to open OCA file for loading."));
    return false;
  } else {
    DVGui::info(QObject::tr("Reading OCA file: %1").arg(path));
  }
  QByteArray byteArray;
  byteArray = file.readAll();
  file.close();

  // 2. Format the content of the byteArray as QJsonDocument and check on parse
  // Errors
  QJsonParseError parseError;
  QJsonDocument jsonDoc;
  jsonDoc = QJsonDocument::fromJson(byteArray, &parseError);
  if (parseError.error != QJsonParseError::NoError) {
    DVGui::warning(QObject::tr("Parse error at %1 while loading OCA file.")
                       .arg(parseError.offset));
    return false;
  }
  json = jsonDoc.object();
  return true;
}

void OCAIo::OCAInputData::getSceneData() {
  m_framerate = m_oprop->getFrameRate();
  int from, to, step;
  if (m_scene->getTopXsheet() == m_xsheet &&
      m_oprop->getRange(from, to, step)) {
    m_startTime = from;
    m_endTime   = to;
  } else {
    m_startTime = 0;
    m_endTime   = m_xsheet->getFrameCount() - 1;
  }
  if (m_endTime < 0) m_endTime = 0;

  m_width  = m_scene->getCurrentCamera()->getRes().lx;
  m_height = m_scene->getCurrentCamera()->getRes().ly;
}

void OCAIo::OCAInputData::load(const QJsonObject &json, bool importFiles) {
  m_originApp             = json.value("originApp").toString();
  m_originAppVersion      = json.value("originAppVersion").toString();
  m_ocaVersion            = json.value("ocaVersion").toString();
  m_name                  = json.value("name").toString();
  m_framerate             = json.value("frameRate").toInt(m_framerate);
  m_width                 = json.value("width").toInt(m_width);
  m_height                = json.value("height").toInt(m_height);
  m_startTime             = json.value("startTime").toInt(m_startTime);
  m_endTime               = json.value("endTime").toInt(m_endTime);
  m_colorDepth            = json.value("colorDepth").toString();
  QJsonArray bgColorArray = json["backgroundColor"].toArray();
  m_bgRed                 = bgColorArray[0].toDouble();
  m_bgGreen               = bgColorArray[1].toDouble();
  m_bgBlue                = bgColorArray[2].toDouble();
  m_bgAlpha               = bgColorArray[3].toDouble();
  m_layers                = json.value("layers").toArray();
  for (auto jsonLayer : m_layers) {
    importOcaLayer(jsonLayer.toObject(), importFiles);
  }
}

void OCAIo::OCAInputData::setSceneData() {
  // Only set scene data if this is an untitled scene
  if (!m_scene->isUntitled()) return;

  // Never set scene name
  // m_scene->setSceneName(m_name.toStdWString());
  m_oprop->setFrameRate(m_framerate);
  auto resolution = TDimension(m_width, m_height);
  if (m_dpi > 0) {
    TDimensionD size(0, 0);
    size.lx = resolution.lx / m_dpi;
    size.ly = resolution.ly / m_dpi;
    m_scene->getCurrentCamera()->setSize(size, false, false);
  }
  m_scene->getCurrentCamera()->setRes(resolution);

  m_xsheet->updateFrameCount();
  m_oprop->setRange(m_startTime, m_endTime, 1);

  // If background is all 0s, use our default Bg color
  if (m_bgRed || m_bgGreen || m_bgBlue || m_bgAlpha) {
    TPixel32 color = TPixel32(m_bgRed * 255.0, m_bgGreen * 255.0,
                              m_bgBlue * 255.0, m_bgAlpha * 255.0);
    m_scene->getProperties()->setBgColor(color);
  }

  int nonlinearBpp = m_oprop->getNonlinearBpp();
  if (m_colorDepth == "U8" && nonlinearBpp < 32)
    m_oprop->setNonlinearBpp(32);
  else if (m_colorDepth == "U16" && nonlinearBpp < 64)
    m_oprop->setNonlinearBpp(64);
  else if (m_colorDepth == "F16" && nonlinearBpp < 64)
    m_oprop->setNonlinearBpp(64);
  else if (m_colorDepth == "U32" && nonlinearBpp < 128)
    m_oprop->setNonlinearBpp(128);
}

void OCAIo::OCAInputData::importOcaLayer(const QJsonObject &jsonLayer,
                                         bool importFiles) {
  if (jsonLayer["type"] == "paintlayer" || jsonLayer["type"] == "vectorlayer") {
    if (jsonLayer["blendingMode"].toString() != "normal") {
      showImportMessage(
          DVGui::WARNING,
          QObject::tr("Blending mode '%1' not implemented for %2 '%3'")
              .arg(jsonLayer["blendingMode"].toString())
              .arg(jsonLayer["type"].toString())
              .arg(jsonLayer["name"].toString()));
    }

    //
    // Create level from image files
    //
    IoCmd::LoadResourceArguments args;
    args.expose       = false;
    args.importPolicy = importFiles
                            ? IoCmd::LoadResourceArguments::ImportPolicy::IMPORT
                            : IoCmd::LoadResourceArguments::ImportPolicy::LOAD;

    auto frames = jsonLayer["frames"].toArray();
    for (int i = 0; i < frames.size(); i++) {
      auto jsonFrame = frames[i].toObject();
      if (jsonFrame["fileName"].toString() == "") continue;
      TFilePath fp = m_parentDir + TFilePath(jsonFrame["fileName"].toString());
      fp           = fp.getParentDir() + fp.getLevelName();
      args.resourceDatas.push_back(fp);
      break;
    }

    // Do not create levels without images!
    if (args.resourceDatas.empty()) {
      showImportMessage(DVGui::WARNING,
                        QObject::tr("Skipped %1 '%2'. No image file indicated.")
                            .arg(jsonLayer["type"].toString())
                            .arg(jsonLayer["name"].toString()));
      return;
    }

    IoCmd::loadResources(args);

    if (args.loadedLevels.empty()) {
      showImportMessage(DVGui::CRITICAL,
                        QObject::tr("Unable to load images for %1 '%2'.")
                            .arg(jsonLayer["type"].toString())
                            .arg(jsonLayer["name"].toString()));
      return;
    }

    //
    // Set level properties
    //
    TXshLevel *level = *args.loadedLevels.begin();
    if (!level) return;

    TXshSimpleLevel *sl = dynamic_cast<TXshSimpleLevel *>(level);
    if (!sl) return;

    auto resolution = TDimension(m_width, m_height);
    sl->getProperties()->setImageRes(resolution);

    bool formatSpecified = false;
    LevelProperties *lp  = sl->getProperties();
    if (frames.size() > 0) {
      const Preferences &prefs = *Preferences::instance();
      int formatIdx = prefs.matchLevelFormat(TFilePath(level->getPath()));
      if (formatIdx >= 0) {
        lp->options()   = prefs.levelFormat(formatIdx).m_options;
        formatSpecified = true;
      }
    }
    if (formatSpecified) {
      sl->getProperties()->setWhiteTransp(lp->whiteTransp());
      sl->getProperties()->setDoPremultiply(lp->doPremultiply());
      sl->getProperties()->setDoAntialias(lp->antialiasSoftness());
      sl->getProperties()->setDpi(lp->getDpi());
      sl->getProperties()->setDpiPolicy(lp->getDpiPolicy());
      // sl->getProperties()->setImageDpi(lp->getImageDpi()); //?
      sl->getProperties()->setSubsampling(lp->getSubsampling());
      sl->getProperties()->setColorSpaceGamma(lp->colorSpaceGamma());
    } else {
      sl->getProperties()->setWhiteTransp(m_whiteTransp);
      sl->getProperties()->setDoPremultiply(m_doPremultiply);
      sl->getProperties()->setDoAntialias(m_antialiasSoftness);
      // we filmmakers don't use dpi ??
      sl->getProperties()->setDpi(m_dpi);
      sl->getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
    }

    //
    // Load level into xsheet
    //
    int col = m_xsheet->getFirstFreeColumnIndex();

    TXshLevelColumn *column = new TXshLevelColumn();
    m_xsheet->insertColumn(col, column);

    TStageObject *stageColumn =
        m_xsheet->getStageObject(TStageObjectId::ColumnId(col));
    stageColumn->setName(jsonLayer["name"].toString().toStdString());
    column->setOpacity(byteFromFloat(
        jsonLayer["opacity"].toDouble() *
        255.0));  // not sure if byteFromFloat is really necessary...
    column->setPreviewVisible(!jsonLayer["reference"].toBool());
    column->setCamstandVisible(jsonLayer["visible"].toBool());
    column->setColorFilterId(jsonLayer["label"].toInt());

    int lastFrame = 0;
    for (auto frame : jsonLayer["frames"].toArray()) {
      TXshCell cell;
      if (frame.toObject()["name"].toString() == "_blank" ||
          frame.toObject()["fileName"].toString() == "") {
        cell = TXshCell(0, TFrameId::EMPTY_FRAME);
      } else {
        TFilePath fp(frame.toObject()["fileName"].toString());
        TFrameId fid = fp.getFrame();
        if (fp.getDots() != "..")
          fid = jsonLayer["type"] == "vectorlayer" ? 65534 : TFrameId::NO_FRAME;
        cell  = TXshCell(sl, fid);
      }

      int row = frame.toObject()["frameNumber"].toInt();

      int duration = frame.toObject()["duration"].toInt();
      lastFrame += duration;

      for (int i = 0; i < duration; i++) m_xsheet->setCell(row + i, col, cell);
    }

    TApp::instance()->getCurrentLevel()->setLevel(
        level->getSimpleLevel());  // selects the last created level
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
    TApp::instance()->getCurrentScene()->notifyCastChange();
  } else if (jsonLayer["type"] == "grouplayer") {
    showImportMessage(
        DVGui::WARNING,
        QObject::tr(
            "Sub-layers in grouplayer '%1' will be imported without grouping.")
            .arg(jsonLayer["name"].toString()));

    // For now, import the child layers without grouping information
    for (auto layer : jsonLayer["childLayers"].toArray()) {
      importOcaLayer(layer.toObject(), importFiles);
    }
  } else {
    showImportMessage(DVGui::WARNING,
                      QObject::tr("Skipping unimplemented %1 '%2'")
                          .arg(jsonLayer["type"].toString())
                          .arg(jsonLayer["name"].toString()));
  }
}

void OCAIo::OCAInputData::showImportMessage(DVGui::MsgType msgType,
                                            QString msg) {
  if (m_supressImportMessages) return;

  QString checkBoxLabel = QObject::tr("Ignore all future warnings and errors.");
  QStringList buttons;
  buttons << QObject::tr("Ok");
  DVGui::MessageAndCheckboxDialog *importDialog = DVGui::createMsgandCheckbox(
      msgType, msg, checkBoxLabel, buttons, 0, Qt::Unchecked);
  int ret     = importDialog->exec();
  int checked = importDialog->getChecked();
  importDialog->deleteLater();

  if (checked > 0) m_supressImportMessages = true;
}

ExportOCACommand exportOCACommand;