#pragma once
#ifndef OCAIO_H
#define OCAIO_H

#include "toonzqt/dvdialog.h"
#include "tfilepath.h"
#include "toonz/txshlevelhandle.h"
#include "toonzqt/menubarcommand.h"

#include <QString>
#include <QList>
#include <QMap>

#include <QThread>
#include <QJsonObject>
#include <QJsonArray>

class ToonzScene;
class TXshCellColumn;
class TXsheet;
class TXshSimpleLevel;
class TFrameId;
class TFilePath;
class TOutputProperties;
class TXshLevelHandle;

namespace OCAIo {

struct OCAAsset {
  int width;
  int height;
  QString fileName;
  QString fileExt;
};

class OCAData {
protected:
  QString m_path;
  QString m_name;
  double m_framerate;
  int m_width, m_height;
  int m_startTime, m_endTime;
  double m_bgRed, m_bgGreen, m_bgBlue, m_bgAlpha;
  QJsonArray m_layers;
  int m_subId;
  QMap<QString, OCAAsset> m_assets;
  bool m_raEXR, m_veSVG;

  DVGui::ProgressDialog *m_progressDialog;

public:
  void write(QJsonObject &json) const;
  bool isBlank(TXshCellColumn *column, int row);
  bool getLayerName(TXshCellColumn *column, QString &out);
  bool getCellName(TXshCellColumn *column, int row, QString &out);
  bool saveCell(TXshCellColumn *column, int row, const QString &cellname,
                OCAAsset &out);
  int frameLen(TXshCellColumn *column, const QList<int> &rows, int index);
  bool isGroup(TXshCellColumn *column);
  bool buildGroup(QJsonObject &json, const QList<int> &rows,
                  TXshCellColumn *column, bool exportReferences);
  bool buildLayer(QJsonObject &json, const QList<int> &rows,
                  TXshCellColumn *column);

  void setProgressDialog(DVGui::ProgressDialog *dialog);
  void build(ToonzScene *scene, TXsheet *xsheet, QString name, QString path,
             bool useEXR, bool vectorAsSVG, bool exportReferences);
  bool isEmpty() { return m_layers.isEmpty(); }
};

class OCAInputData : public OCAData {
  // json objects
  QString m_colorDepth;
  QString m_originApp;
  QString m_originAppVersion;
  QString m_ocaVersion;
  // toonz objects
  ToonzScene *m_scene;
  TXsheet *m_xsheet;
  TOutputProperties *m_oprop;
  TFilePath m_parentDir;
  int m_dpi;
  float m_antialiasSoftness;
  bool m_whiteTransp, m_doPremultiply;

  bool m_supressImportMessages;

public:
  OCAInputData(float antialiasSoftness = (float)0.0, bool whiteTransp = true,
               bool doPremultiply = false);
  bool read(QString path, QJsonObject &json);
  void getSceneData();
  void load(const QJsonObject &json, bool importFiles = true);
  void setSceneData();
  void importOcaLayer(const QJsonObject &jsonLayer, bool importFiles);

  void showImportMessage(DVGui::MsgType type, QString msg);
};
}  // namespace OCAIo

class ExportOCACommand final : public MenuItemHandler {
  Q_OBJECT
public:
  ExportOCACommand();
  void execute() override;
};

#endif
