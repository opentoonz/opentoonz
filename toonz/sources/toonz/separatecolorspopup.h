#pragma once
//=============================================================================
// SeparateColorsPopup
//-----------------------------------------------------------------------------
#ifndef SEPARATECOLORSPOPUP_H
#define SEPARATECOLORSPOPUP_H

#include "tfilepath.h"
#include "tpixel.h"
#include "traster.h"
#include "toonzqt/dvdialog.h"

#include <QVector>
#include <QPair>
#include <QRunnable>
#include <QVector3D>
#include <QVector4D>

class QLabel;
class QCheckBox;
class QGroupBox;
class QComboBox;

class SeparateSwatch;
namespace DVGui {
class IntLineEdit;
class FileField;
class LineEdit;
class ColorField;
class ProgressDialog;
class CheckBox;
class DoubleField;
class IntField;
}  // namespace DVGui

namespace ImageUtils {
class FrameTaskNotifier;
}

using namespace DVGui;
using namespace ImageUtils;

//----------------------------------

class Separate4ColorsTask : public QRunnable {
  int m_from, m_to;
  QVector3D* m_src;
  QVector4D* m_out;
  const QVector3D m_paper_xyz, m_main_xyz, m_sub1_xyz, m_sub2_xyz, m_sub3_xyz;
  const QVector4D m_pane_m12, m_pane_m13, m_pane_m23, m_pane_123;
  const QVector3D m_R;
  const float m_mainAdjust;
  const float m_borderRatio;

  void run();

public:
  Separate4ColorsTask(int from, int to, QVector3D* src, QVector4D* out,
                      const QVector3D paper_xyz, const QVector3D main_xyz,
                      const QVector3D sub1_xyz, const QVector3D sub2_xyz,
                      const QVector3D sub3_xyz, const QVector4D pane_m12,
                      const QVector4D pane_m13, const QVector4D pane_m23,
                      const QVector4D pane_123, const QVector3D R,
                      const float mainAdjust, const float borderRatio)
      : m_from(from)
      , m_to(to)
      , m_src(src)
      , m_out(out)
      , m_paper_xyz(paper_xyz)
      , m_main_xyz(main_xyz)
      , m_sub1_xyz(sub1_xyz)
      , m_sub2_xyz(sub2_xyz)
      , m_sub3_xyz(sub3_xyz)
      , m_pane_m12(pane_m12)
      , m_pane_m13(pane_m13)
      , m_pane_m23(pane_m23)
      , m_pane_123(pane_123)
      , m_R(R)
      , m_mainAdjust(mainAdjust)
      , m_borderRatio(borderRatio) {}
};

//----------------------------------

class Separate3ColorsTask : public QRunnable {
  int m_from, m_to;
  QVector3D *m_src, *m_out;
  const QVector3D m_paper_xyz, m_main_xyz, m_sub1_xyz, m_sub2_xyz;
  const QVector4D m_pane;
  const float m_mainAdjust;
  const float m_borderRatio;

  void run();

public:
  Separate3ColorsTask(int from, int to, QVector3D* src, QVector3D* out,
                      const QVector3D paper_xyz, const QVector3D main_xyz,
                      const QVector3D sub1_xyz, const QVector3D sub2_xyz,
                      const QVector4D pane, const float mainAdjust,
                      const float borderRatio)
      : m_from(from)
      , m_to(to)
      , m_src(src)
      , m_out(out)
      , m_paper_xyz(paper_xyz)
      , m_main_xyz(main_xyz)
      , m_sub1_xyz(sub1_xyz)
      , m_sub2_xyz(sub2_xyz)
      , m_pane(pane)
      , m_mainAdjust(mainAdjust)
      , m_borderRatio(borderRatio) {}
};

//----------------------------------

class SeparateColorsPopup : public DVGui::Dialog {
  Q_OBJECT

  FrameTaskNotifier* m_notifier;
  QPushButton *m_previewBtn, *m_okBtn, *m_cancelBtn, *m_autoBtn;
  IntLineEdit *m_fromFld, *m_toFld;
  FileField* m_saveInFileFld;
  QComboBox* m_fileFormatCombo;

  DoubleField *m_subColorAdjustFld, *m_borderSmoothnessFld;

  QGroupBox* m_matteGroupBox;
  DoubleField* m_matteThreshold;
  IntField* m_matteRadius;

  ColorField *m_paperColorField, *m_mainColorField, *m_subColor1Field,
      *m_subColor2Field, *m_subColor3Field;

  QCheckBox *m_outMainCB, *m_outSub1CB, *m_outSub2CB, *m_outSub3CB;
  LineEdit *m_mainSuffixEdit, *m_sub1SuffixEdit, *m_sub2SuffixEdit,
      *m_sub3SuffixEdit;

  IntField* m_previewFrameField;
  QLabel* m_previewFrameLabel;
  QVector<QPair<TFilePath, TFrameId>> m_srcFrames;

  QPushButton *m_pickBtn, *m_showMatteBtn, *m_showAlphaBtn;
  SeparateSwatch* m_separateSwatch;

  ProgressDialog* m_progressDialog;
  std::vector<TFilePath> m_srcFilePaths;

  QString m_lastAcceptedSaveInPath;

  bool m_isConverting;

  void doSeparate(const TFilePath& source, int from, int to, int framerate,
                  FrameTaskNotifier* frameNotifier, bool do4Colors);

  void doCompute(TRaster32P raster, TDimensionI& dim, TRaster32P ras_m,
                 TRaster32P ras_c1, TRaster32P ras_c2, bool isPreview = false);

  void doCompute(TRaster32P raster, TDimensionI& dim, TRaster32P ras_m,
                 TRaster32P ras_c1, TRaster32P ras_c2, TRaster32P ras_c3,
                 bool isPreview = false);

  void doPreview(TRaster32P& orgRas, TRaster32P& mainRas, TRaster32P& sub1Ras,
                 TRaster32P& sub2Ras, TRaster32P& sub3Ras);

public:
  SeparateColorsPopup();
  ~SeparateColorsPopup();
  void setFiles(const std::vector<TFilePath>& fps);
  bool isConverting();

protected:
  void showEvent(QShowEvent* e);
  // store the current value to user env file
  void hideEvent(QHideEvent* e);

public slots:
  // starts the conversion
  void separate();
  void onSeparateFinished();
  void onPreviewBtnPressed();
  void onPreviewFrameFieldValueChanged(bool isDragging);

  void onChange(bool isDragging);
  void onToggle() { onChange(false); }
  void onColorChange(const TPixel32&, bool isDragging) { onChange(isDragging); }

  void onSaveSettings();
  void onLoadSettings();

  void doSaveSettings(const TFilePath&);
  void doLoadSettings(const TFilePath&, bool loadAll);

  void onSaveInPathChanged();
};

#endif