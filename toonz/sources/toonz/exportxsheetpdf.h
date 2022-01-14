#pragma once

#ifndef EXPORTXSHEETPDF_H
#define EXPORTXSHEETPDF_H

#include "toonzqt/dvdialog.h"
#include "toonz/txshcell.h"

#include <QMarginsF>
#include <QSize>
#include <QPoint>
#include <QPainter>
#include <QList>
#include <QPageSize>
#include <QPdfWriter>
#include <QScrollArea>
#include <QPair>

// forward declaration
class QComboBox;
class QCheckBox;
class TXshLevelColumn;
class TXshSoundColumn;
class TXshSoundTextColumn;
namespace DVGui {
class FileField;
class ColorField;
}  // namespace DVGui
class XSheetPDFTemplate;
class QFontComboBox;
class QTextEdit;
class QRadioButton;
class QLineEdit;
class QIntValidator;

// parameters which can be defined in template files
namespace XSheetPDFTemplateParamIDs {
// numbers
const std::string BodyAmount            = "BodyAmount";
const std::string KeyColAmount          = "KeyColAmount";
const std::string CellsColAmount        = "CellsColAmount";
const std::string CameraColAmount       = "CameraColAmount";
const std::string FrameLength           = "FrameLength";
const std::string MemoLinesAmount       = "MemoLinesAmount";
const std::string ExtraCellsColAmount   = "ExtraCellsColAmount";
const std::string DrawCameraGrid        = "DrawCameraGrid";
const std::string DrawCameraHeaderGrid  = "DrawCameraHeaderGrid";
const std::string DrawCameraHeaderLabel = "DrawCameraHeaderLabel";
const std::string DrawCellsHeaderLabel  = "DrawCellsHeaderLabel";
const std::string TranslateBodyLabel    = "TranslateBodyLabel";
const std::string TranslateInfoLabel    = "TranslateInfoLabel";
const std::string IsBlockBorderThick    = "IsBlockBorderThick";

// lengths
const std::string BodyWidth        = "BodyWidth";
const std::string BodyHeight       = "BodyHeight";
const std::string BodyHMargin      = "BodyHMargin";
const std::string BodyTop          = "BodyTop";
const std::string HeaderHeight     = "HeaderHeight";
const std::string KeyColWidth      = "KeyColWidth";
const std::string LastKeyColWidth  = "LastKeyColWidth";
const std::string DialogColWidth   = "DialogColWidth";
const std::string CellsColWidth    = "CellsColWidth";
const std::string CameraColWidth   = "CameraColWidth";
const std::string RowHeight        = "RowHeight";
const std::string OneSecHeight     = "1SecHeight";
const std::string InfoOriginLeft   = "InfoOriginLeft";
const std::string InfoOriginTop    = "InfoOriginTop";
const std::string InfoTitleHeight  = "InfoTitleHeight";
const std::string InfoBodyHeight   = "InfoBodyHeight";
const std::string ThinLineWidth    = "ThinLineWidth";
const std::string ThickLineWidth   = "ThickLineWidth";
const std::string BodyOutlineWidth = "BodyOutlineWidth";
};  // namespace XSheetPDFTemplateParamIDs

// ids for various information area
enum XSheetPDFDataType {
  Data_Memo = 0,
  Data_Second,
  Data_Frame,
  Data_TotalPages,
  Data_CurrentPage,
  Data_DateTimeAndScenePath,
  Data_SceneName,
  Data_Logo,
  Data_Invalid
};

typedef void (*DecoFunc)(QPainter&, QRect, QMap<XSheetPDFDataType, QRect>&,
                         bool);

enum ExportArea { Area_Actions = 0, Area_Cells };
enum ContinuousLineMode { Line_Always = 0, Line_MoreThan3s, Line_None };
enum TickMarkType {
  TickMark_Dot = 0,
  TickMark_Circle,
  TickMark_Filled,
  TickMark_Asterisk
};

struct XSheetPDFFormatInfo {
  QColor lineColor;
  QString dateTimeText;
  QString scenePathText;
  QString sceneNameText;
  ExportArea exportArea;
  QString templateFontFamily;
  QString contentsFontFamily;
  QString memoText;
  QString logoText;
  QPixmap logoPixmap;
  bool drawSound;
  bool serialFrameNumber;
  bool drawLevelNameOnBottom;
  ContinuousLineMode continuousLineMode;
  int tick1MarkId;
  int tick2MarkId;
  int keyMarkId;
  TickMarkType tick1MarkType;
  TickMarkType tick2MarkType;
};

class XSheetPDFTemplate {
protected:
  struct XSheetPDF_InfoFormat {
    int width;
    QString label;
    DecoFunc decoFunc = nullptr;
  };

  struct XSheetPDFTemplateParams {
    QPageSize::PageSizeId documentPageSize;
    QMarginsF documentMargin;
    QList<XSheetPDF_InfoFormat> array_Infos;
    int bodylabelTextSize_Large;
    int bodylabelTextSize_Small;
    int keyBlockWidth;
    int cellsBlockWidth;
    int cameraBlockWidth;
    int infoHeaderHeight;
  } m_p;

  QMap<std::string, int> m_params;

  QPen thinPen, thickPen, blockBorderPen, bodyOutlinePen;

  XSheetPDFFormatInfo m_info;

  QList<QList<QRect>> m_colLabelRects;
  QList<QList<QRect>> m_colLabelRects_bottom;
  QList<QList<QRect>> m_cellRects;
  QList<QRect> m_soundCellRects;
  QMap<XSheetPDFDataType, QRect> m_dataRects;

  // column and column name (if manually specified)
  QList<QPair<TXshLevelColumn*, QString>> m_columns;
  QList<TXshSoundColumn*> m_soundColumns;
  TXshSoundTextColumn* m_noteColumn;

  int m_duration;
  bool m_useExtraColumns;

  void adjustSpacing(QPainter& painter, const int width, const QString& label,
                     const double ratio = 0.8);

  void drawGrid(QPainter& painter, int colAmount, int colWidth, int blockWidth);
  void drawHeaderGrid(QPainter& painter, int colAmount, int colWidth,
                      int blockWidth);

  void registerColLabelRects(QPainter& painter, int colAmount, int colWidth,
                             int bodyId);
  void registerCellRects(QPainter& painter, int colAmount, int colWidth,
                         int bodyId);
  void registerSoundRects(QPainter& painter, int colWidth, int bodyId);

  // Key Block
  void drawKeyBlock(QPainter& painter, int framePage, const int bodyId);

  void drawDialogBlock(QPainter& painter, const int framePage,
                       const int bodyId);

  void drawCellsBlock(QPainter& painter, int bodyId);

  void drawCameraBlock(QPainter& painter);

  void drawXsheetBody(QPainter& painter, int framePage, int bodyId);

  void drawInfoHeader(QPainter& painter);

  void addInfo(int w, QString lbl, DecoFunc f = nullptr);

  void drawContinuousLine(QPainter& painter, QRect rect, bool isEmpty);
  void drawCellNumber(QPainter& painter, QRect rect, TXshCell& cell,
                      bool isKey);
  void drawTickMark(QPainter& painter, QRect rect, TickMarkType type);
  void drawEndMark(QPainter& painter, QRect upperRect);
  void drawLevelName(QPainter& painter, QRect rect, QString name,
                     bool isBottom = false);
  void drawLogo(QPainter& painter);
  void drawSound(QPainter& painter, int framePage);
  void drawDialogue(QPainter& painter, int framePage);

  int param(const std::string& id, int defaultValue = 0) {
    if (!m_params.contains(id)) std::cout << id << std::endl;
    return m_params.value(id, defaultValue);
  }

public:
  XSheetPDFTemplate(const QList<QPair<TXshLevelColumn*, QString>>& columns,
                    const int duration);
  void drawXsheetTemplate(QPainter& painter, int framePage,
                          bool isPreview = false);
  void drawXsheetContents(QPainter& painter, int framePage, int prallelPage,
                          bool isPreview = false);
  void initializePage(QPdfWriter& writer);
  QPixmap initializePreview();
  int framePageCount();
  int parallelPageCount();
  int columnsInPage();
  QSize logoPixelSize();
  void setLogoPixmap(QPixmap pm);
  void setSoundColumns(const QList<TXshSoundColumn*>& soundColumns) {
    m_soundColumns = soundColumns;
  }
  void setNoteColumn(TXshSoundTextColumn* noteColumn) {
    m_noteColumn = noteColumn;
  }
  void setInfo(const XSheetPDFFormatInfo& info);
};

class XSheetPDFTemplate_B4_6sec : public XSheetPDFTemplate {
public:
  XSheetPDFTemplate_B4_6sec(
      const QList<QPair<TXshLevelColumn*, QString>>& columns,
      const int duration);
};

class XSheetPDFTemplate_Custom : public XSheetPDFTemplate {
  bool m_valid;

public:
  XSheetPDFTemplate_Custom(
      const QString& fp, const QList<QPair<TXshLevelColumn*, QString>>& columns,
      const int duration);
  bool isValid() { return m_valid; }
};

//-----------------------------------------------------------------------------

class XsheetPdfPreviewPane final : public QWidget {
  Q_OBJECT
  QPixmap m_pixmap;
  double m_scaleFactor;

protected:
  void paintEvent(QPaintEvent* event) override;

public:
  XsheetPdfPreviewPane(QWidget* parent);
  void setPixmap(QPixmap pm);
  void doZoom(double d_scale);
  void fitScaleTo(QSize size);
};

class XsheetPdfPreviewArea final : public QScrollArea {
  Q_OBJECT
  QPoint m_mousePos;

protected:
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

public:
  XsheetPdfPreviewArea(QWidget* parent) : QScrollArea(parent) {}
protected slots:
  void fitToWindow();
};

//-----------------------------------------------------------------------------

class ExportXsheetPdfPopup final : public DVGui::Dialog {
  Q_OBJECT
  XsheetPdfPreviewPane* m_previewPane;
  XsheetPdfPreviewArea* m_previewArea;
  DVGui::FileField* m_pathFld;
  QLineEdit* m_fileNameFld;
  QComboBox *m_templateCombo, *m_exportAreaCombo, *m_continuousLineCombo,
      *m_dialogueColCombo;
  DVGui::ColorField* m_lineColorFld;

  QCheckBox *m_addDateTimeCB, *m_addScenePathCB, *m_drawSoundCB,
      *m_addSceneNameCB, *m_serialFrameNumberCB, *m_levelNameOnBottomCB,
      *m_drawDialogueCB;

  QFontComboBox *m_templateFontCB, *m_contentsFontCB;
  QTextEdit* m_memoEdit;

  QLabel* m_pageInfoLbl;

  QRadioButton *m_logoTxtRB, *m_logoImgRB;
  QLineEdit *m_logoTextEdit, *m_sceneNameEdit;
  DVGui::FileField* m_logoImgPathField;

  QLineEdit* m_currentPageEdit;
  int m_totalPageCount;
  QPushButton *m_prev, *m_next;

  QComboBox *m_tick1IdCombo, *m_tick2IdCombo, *m_keyIdCombo;
  QComboBox *m_tick1MarkCombo, *m_tick2MarkCombo;

  // column and column name (if manually specified)
  QList<QPair<TXshLevelColumn*, QString>> m_columns;
  QList<TXshSoundColumn*> m_soundColumns;
  QMap<int, TXshSoundTextColumn*> m_noteColumns;
  int m_duration;

  XSheetPDFTemplate* m_currentTmpl;

  // enum XSheetTemplateId { XSheetTemplate_B4_6sec = 0, XSheetTemplate_B4_3sec
  // };

  void initialize();
  void saveSettings();
  void loadSettings();
  void onExportFinished(const TFilePath&);
  void loadPresetItems();

protected:
  void showEvent(QShowEvent* event) override { initialize(); }
  void closeEvent(QCloseEvent* event) override { saveSettings(); }

public:
  ExportXsheetPdfPopup();
  ~ExportXsheetPdfPopup();
protected slots:
  void onExport();
  void onExportPNG();

  void initTemplate();
  void setInfo();
  void updatePreview();
  void onLogoModeToggled();
  void onLogoImgPathChanged();
  void onPrev();
  void onNext();

  void onTickIdComboActivated();
};

#endif