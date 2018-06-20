

#include "curveio.h"
#include "tsystem.h"
#include "tstream.h"
#include "tdoubleparam.h"
#include "tfilepath_io.h"
#include "toonz/tproject.h"
#include "tconvert.h"
#include "filebrowserpopup.h"
#include "filebrowser.h"
#include "tundo.h"

//=============================================================================

class LoadCurveUndo final : public TUndo {
  TDoubleParamP m_curve;
  TDoubleParamP m_oldCurve, m_newCurve;

public:
  LoadCurveUndo(TDoubleParam *curve) : m_curve(curve) {
    m_oldCurve = static_cast<TDoubleParam *>(curve->clone());
  }
  void onAdd() override {
    m_newCurve = static_cast<TDoubleParam *>(m_curve->clone());
  }

  void undo() const override { m_curve->copy(m_oldCurve.getPointer()); }
  void redo() const override { m_curve->copy(m_newCurve.getPointer()); }
  int getSize() const override {
    return sizeof(*this) + 2 * sizeof(TDoubleParam);  // not very accurate
  }
};

//=============================================================================

class CurvePopup : public FileBrowserPopup {
  TFilePath m_folderPath;
  TDoubleParam *m_curve;

public:
  CurvePopup(const QString name, const TFilePath folderPath,
             TDoubleParam *curve)
      : FileBrowserPopup(name), m_folderPath(folderPath), m_curve(curve) {
    curve->addRef();
    m_browser->enableGlobalSelection(false);
  }

  ~CurvePopup() { m_curve->release(); }

  void setFilterType(QString type) {
    QStringList sl;
    sl << type;
    setFilterTypes(sl);
  }

  void initFolder() override { setFolder(m_folderPath); }

  bool checkOverride(const TFilePath &fp) const {
    if (TFileStatus(fp).doesExist()) {
      QString question = QObject::tr("Are you sure you want to override ") +
                         QString::fromStdWString(fp.getLevelNameW()) + "?";
      int ret = DVGui::MsgBox(question, QObject::tr("Override"),
                              QObject::tr("Cancel"), 1);
      return ret == 1;
    } else
      return true;
  }

  TDoubleParam *getCurve() const { return m_curve; }
};

//=============================================================================

class SaveCurvePopup final : public CurvePopup {
public:
  SaveCurvePopup(const TFilePath folderPath, TDoubleParam *curve)
      : CurvePopup(tr("Save Curve"), folderPath, curve) {
    setOkText(tr("Save"));
    setFilterType("curve");
  }

  bool execute() override;
};

//-----------------------------------------------------------------------------

bool SaveCurvePopup::execute() {
  if (m_selectedPaths.empty()) return false;

  TFilePath fp(*m_selectedPaths.begin());

  if (fp.getType() == "") fp = fp.withType("curve");

  if (!checkOverride(fp)) return false;

  try {
    TSystem::touchParentDir(fp);
    TOStream os(fp);
    getCurve()->saveData(os);
    return true;
  } catch (...) {
    DVGui::warning(QObject::tr("It is not possible to save the curve."));
    return false;
  }
}

//=============================================================================

class LoadCurvePopup final : public CurvePopup {
public:
  LoadCurvePopup(const TFilePath folderPath, TDoubleParam *curve)
      : CurvePopup(tr("Load Curve"), folderPath, curve) {
    setOkText(tr("Load"));
    setFilterType("curve");
  }

  bool execute() override;
};

//-----------------------------------------------------------------------------

bool LoadCurvePopup::execute() {
  if (m_selectedPaths.empty()) return false;

  TFilePath fp(*m_selectedPaths.begin());

  if (fp.getType() == "") fp = fp.withType("curve");

  if (!TFileStatus(fp).doesExist()) return false;

  try {
    TIStream is(fp);
    // default value must be kept the same!!!
    TDoubleParam *curve = getCurve();
    double defaultValue = curve->getDefaultValue();
    LoadCurveUndo *undo = new LoadCurveUndo(curve);
    curve->loadData(is);
    curve->setDefaultValue(defaultValue);
    TUndoManager::manager()->add(undo);
  } catch (...) {
    DVGui::warning(QObject::tr("It is not possible to load the curve."));
    return false;
  }

  return true;
}

//=============================================================================

class ExportCurvePopup final : public CurvePopup {
  std::string m_name;

public:
  ExportCurvePopup(const TFilePath folderPath, TDoubleParam *curve,
                   const std::string &name)
      : CurvePopup(tr("Export Curve"), folderPath, curve), m_name(name) {
    setOkText(tr("Export"));
    setFilterType("dat");
  }

  bool execute() override;
};

//-----------------------------------------------------------------------------

bool ExportCurvePopup::execute() {
  if (m_selectedPaths.empty()) return false;

  TFilePath fp(*m_selectedPaths.begin());

  if (fp.getType() == "") fp = fp.withType("dat");

  if (!checkOverride(fp)) return false;

  try {
    TSystem::touchParentDir(fp);
    Tofstream os(fp);
    os << "# RELEASE:      5.0" << std::endl;
    os << "# FILE NAME:    " << ::to_string(fp) << std::endl;
    os << "# COMPOSED BY:  " << TSystem::getUserName().toStdString()
       << std::endl;
    os << "# MACHINE NAME: " << TSystem::getHostName().toStdString()
       << std::endl;
    // os << "# DATE:         " << TSystem::getCurrentTime() << endl;
    os << std::endl << std::endl;

    int frameCount      = 1;
    TDoubleParam *curve = getCurve();
    if (curve->getKeyframeCount() > 0)
      frameCount =
          curve->keyframeIndexToFrame(curve->getKeyframeCount() - 1) + 1;

    os << "# Dump of:     " << m_name << std::endl;
    os << "# Frame range: 1 " << frameCount << std::endl;
    os << std::endl;

    for (int i = 0; i < frameCount; i++) {
      os.setf(std::ios::fixed, std::ios::floatfield);
      os.setf(std::ios::showpoint);
      os.precision(5);
      os.width(12);
      os << curve->getValue(i) << std::endl;
    }
  } catch (...) {
    DVGui::warning(QObject::tr("It is not possible to export data."));
    return false;
  }

  return true;
}

//------------------------------------------

void saveCurve(TDoubleParam *curve) {
  TFilePath folderPath =
      TProjectManager::instance()->getCurrentProject()->getScenesPath();
  SaveCurvePopup popup(folderPath, curve);
  popup.exec();
}

//------------------------------------------

void loadCurve(TDoubleParam *curve) {
  TFilePath folderPath =
      TProjectManager::instance()->getCurrentProject()->getScenesPath();
  LoadCurvePopup popup(folderPath, curve);
  popup.exec();
}

//------------------------------------------

void exportCurve(TDoubleParam *curve, const std::string &name) {
  TFilePath folderPath =
      TProjectManager::instance()->getCurrentProject()->getScenesPath();
  ExportCurvePopup popup(folderPath, curve, name);
  popup.exec();
}
