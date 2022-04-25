
#include "exportxsheetpdf.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "toutputproperties.h"
#include "orientation.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"
#include "toonzqt/filefield.h"
#include "toonzqt/colorfield.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/tstageobject.h"
#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"
#include "toonz/txshsoundtextcolumn.h"
#include "toonz/txshsoundtextlevel.h"

// TnzCore includes
#include "tsystem.h"
#include "tlevel_io.h"
#include "tenv.h"

#include <iostream>
#include <QMainWindow>
#include <QPdfWriter>
#include <QPainter>
#include <QFile>
#include <QApplication>
#include <QFontMetrics>
#include <QComboBox>
#include <QCheckBox>
#include <QScrollArea>
#include <QPushButton>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPixmap>
#include <QMouseEvent>
#include <QScrollBar>
#include <QPainterPath>
#include <QFontComboBox>
#include <QTextEdit>
#include <QRadioButton>
#include <QImageReader>
#include <QLineEdit>
#include <QIntValidator>
#include <QDesktopServices>
#include <QGroupBox>
#include <QSettings>

// Template
TEnv::StringVar XShPdfExportTemplate("XShPdfExportTemplate", "B4_6sec");
// output area
TEnv::IntVar XShPdfExportOutputArea("XShPdfExportOutputArea", Area_Cells);
// line color
TEnv::StringVar XShPdfExportLineColor(
    "XShPdfExportLineColor", QColor(128, 196, 128).name().toStdString());
// print datetime
TEnv::IntVar XShPdfExportPrintDateTime("XShPdfExportPrintDateTime", 0);
// print scene path
TEnv::IntVar XShPdfExportPrintScenePath("XShPdfExportPrintScenePath", 0);
// print soundtrack
TEnv::IntVar XShPdfExportPrintSoundtrack("XShPdfExportPrintSoundtrack", 0);
// serial frame number
TEnv::IntVar XShPdfExportSerialFrameNumber("XShPdfExportSerialFrameNumber", 0);
// print level name on the bottom
TEnv::IntVar XShPdfExportLevelNameOnBottom("XShPdfExportLevelNameOnBottom", 0);
// print dialogue
TEnv::IntVar XShPdfExportPrintDialogue("XShPdfExportPrintDialogue", 0);
// print scene name
TEnv::IntVar XShPdfExportPrintSceneName("XShPdfExportPrintSceneName", 0);
// template font
TEnv::StringVar XShPdfExportTemplateFont("XShPdfExportTemplateFont", "");
// output font
TEnv::StringVar XShPdfExportOutputFont("XShPdfExportOutputFont", "");
// logo preferene (0 = text, 1 = image)
TEnv::IntVar XShPdfExportLogoPreference("XShPdfExportLogoPreference", 0);
// logo text
TEnv::StringVar XShPdfExportLogoText("XShPdfExportLogoText", "");
// logo image path
TEnv::StringVar XShPdfExportImgPath("XShPdfExportImgPath", "");
// continuous line threshold
TEnv::IntVar XShPdfExportContinuousLineThres("XShPdfExportContinuousLineThres",
                                             0);

TEnv::IntVar XShPdfExportTick1Id("XShPdfExportTick1Id", -1);
TEnv::IntVar XShPdfExportTick2Id("XShPdfExportTick2Id", -1);
TEnv::IntVar XShPdfExportKeyId("XShPdfExportKeyId", -1);
TEnv::IntVar XShPdfExportTick1Type("XShPdfExportTick1Type", TickMark_Dot);
TEnv::IntVar XShPdfExportTick2Type("XShPdfExportTick2Type", TickMark_Dot);

using namespace XSheetPDFTemplateParamIDs;

namespace {
const int PDF_Resolution = 400;
int mm2px(double mm) {
  return (int)std::round(mm * (double)PDF_Resolution / 25.4);
}

//-----------------------------------------------------------------------------
/*! convert the last one digit of the frame number to alphabet
        Ex.  12 -> 1B    21 -> 2A   30 -> 3
 */
QString getFrameNumberWithLetters(int frame) {
  int letterNum = frame % 10;
  QChar letter;

  switch (letterNum) {
  case 0:
    letter = QChar();
    break;
  case 1:
    letter = 'A';
    break;
  case 2:
    letter = 'B';
    break;
  case 3:
    letter = 'C';
    break;
  case 4:
    letter = 'D';
    break;
  case 5:
    letter = 'E';
    break;
  case 6:
    letter = 'F';
    break;
  case 7:
    letter = 'G';
    break;
  case 8:
    letter = 'H';
    break;
  case 9:
    letter = 'I';
    break;
  default:
    letter = QChar();
    break;
  }

  QString number;
  if (frame >= 10) {
    number = QString::number(frame);
    number.chop(1);
  } else
    number = "0";

  return (QChar(letter).isNull()) ? number : number.append(letter);
}

void decoSceneInfo(QPainter& painter, QRect rect,
                   QMap<XSheetPDFDataType, QRect>& dataRects, bool) {
  dataRects[Data_SceneName] = painter.transform().mapRect(rect);
}

void decoTimeInfo(QPainter& painter, QRect rect,
                  QMap<XSheetPDFDataType, QRect>& dataRects, bool doTranslate) {
  QString plusStr, secStr, frmStr;
  if (doTranslate) {
    plusStr = QObject::tr("+", "XSheetPDF");
    secStr  = QObject::tr("'", "XSheetPDF:second");
    frmStr  = QObject::tr("\"", "XSheetPDF:frame");
  } else {
    plusStr = "+";
    secStr  = "'";
    frmStr  = "\"";
  }

  painter.save();
  {
    QFont font = painter.font();
    font.setPixelSize(rect.height() / 2 - mm2px(1));
    font.setLetterSpacing(QFont::PercentageSpacing, 100);
    while (font.pixelSize() > mm2px(2)) {
      if (QFontMetrics(font).boundingRect(plusStr).width() < rect.width() / 12)
        break;
      font.setPixelSize(font.pixelSize() - mm2px(0.2));
    }
    painter.setFont(font);

    QRect labelRect(0, 0, rect.width() / 2, rect.height() / 2);
    QRect dataRect(0, 0, rect.width() / 2, rect.height());
    painter.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, secStr);
    dataRects[Data_Second] = painter.transform().mapRect(dataRect);
    painter.translate(labelRect.width(), 0);
    painter.drawText(labelRect.adjusted(0, 0, 0, -mm2px(0.5)),
                     Qt::AlignRight | Qt::AlignVCenter, frmStr);
    dataRects[Data_Frame] = painter.transform().mapRect(dataRect);
    painter.translate(0, labelRect.height());
    painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, plusStr);
  }
  painter.restore();
}

// Display the total pages over the current page.
// This format is odd in terms of fraction, but can be seen in many Japanese
// studios.

void doDecoSheetInfo(QPainter& painter, QRect rect,
                     QMap<XSheetPDFDataType, QRect>& dataRects,
                     bool doTranslate, bool inv) {
  QString totStr, thStr;
  if (doTranslate) {
    totStr = QObject::tr("TOT", "XSheetPDF");
    thStr  = QObject::tr("th", "XSheetPDF");
  } else {
    totStr = "TOT";
    thStr  = "th";
  }
  QString upperStr             = (inv) ? totStr : thStr;
  QString bottomStr            = (inv) ? thStr : totStr;
  XSheetPDFDataType upperType  = (inv) ? Data_TotalPages : Data_CurrentPage;
  XSheetPDFDataType bottomType = (inv) ? Data_CurrentPage : Data_TotalPages;

  painter.save();
  {
    QFont font = painter.font();
    font.setPixelSize(rect.height() / 2 - mm2px(1));
    font.setLetterSpacing(QFont::PercentageSpacing, 100);
    while (font.pixelSize() > mm2px(2)) {
      if (QFontMetrics(font).boundingRect(totStr).width() < rect.width() / 6)
        break;
      font.setPixelSize(font.pixelSize() - mm2px(0.2));
    }
    painter.setFont(font);

    painter.drawLine(rect.topRight(), rect.bottomLeft());
    QRect labelRect(0, 0, rect.width() / 2, rect.height() / 2);
    painter.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, upperStr);
    dataRects[upperType] = painter.transform().mapRect(
        labelRect.adjusted(0, 0, -labelRect.width() / 4, mm2px(1)));
    painter.translate(labelRect.width(), labelRect.height());
    painter.drawText(labelRect.adjusted(0, 0, -mm2px(0.5), 0),
                     Qt::AlignRight | Qt::AlignVCenter, bottomStr);
    dataRects[bottomType] = painter.transform().mapRect(
        labelRect.adjusted(0, -mm2px(1), -labelRect.width() / 4, 0));
  }
  painter.restore();
}

void decoSheetInfoInv(QPainter& painter, QRect rect,
                      QMap<XSheetPDFDataType, QRect>& dataRects,
                      bool doTranslate) {
  doDecoSheetInfo(painter, rect, dataRects, doTranslate, true);
}

void decoSheetInfo(QPainter& painter, QRect rect,
                   QMap<XSheetPDFDataType, QRect>& dataRects,
                   bool doTranslate) {
  doDecoSheetInfo(painter, rect, dataRects, doTranslate, false);
}

QPageSize::PageSizeId str2PageSizeId(const QString& str) {
  const QMap<QString, QPageSize::PageSizeId> map = {
      {"JisB4", QPageSize::JisB4},
      {"B4", QPageSize::JisB4},  // return Jis B4 instead of ISO B4
      {"A3", QPageSize::A3},
      {"A4", QPageSize::A4}};

  return map.value(str, QPageSize::JisB4);
}

DecoFunc infoType2DecoFunc(const QString& str) {
  const QMap<QString, DecoFunc> map = {{"Scene", decoSceneInfo},
                                       {"Time", decoTimeInfo},
                                       {"Sheet", decoSheetInfo},
                                       {"SheetInv", decoSheetInfoInv}};

  return map.value(str, nullptr);
}

XSheetPDFDataType dataStr2Type(const QString& str) {
  const QMap<QString, XSheetPDFDataType> map = {
      {"Memo", Data_Memo},
      {"Second", Data_Second},
      {"Frame", Data_Frame},
      {"TotalPages", Data_TotalPages},
      {"CurrentPage", Data_CurrentPage},
      {"DateTimeAndScenePath", Data_DateTimeAndScenePath},
      {"SceneName", Data_SceneName},
      {"Logo", Data_Logo}};

  return map.value(str, Data_Invalid);
}

QIcon getColorChipIcon(TPixel32 color) {
  QPixmap pm(15, 15);
  pm.fill(QColor(color.r, color.g, color.b));
  return QIcon(pm);
}

void refreshCellMarkComboItems(QComboBox* combo) {
  int current = -1;
  if (combo->count()) current = combo->currentData().toInt();

  combo->clear();
  QList<TSceneProperties::CellMark> marks = TApp::instance()
                                                ->getCurrentScene()
                                                ->getScene()
                                                ->getProperties()
                                                ->getCellMarks();
  combo->addItem(QObject::tr("None", "XSheetPDF CellMark"), -1);
  int curId = 0;
  for (auto mark : marks) {
    QString label = QString("%1: %2").arg(curId).arg(mark.name);
    combo->addItem(getColorChipIcon(mark.color), label, curId);
    curId++;
  }

  if (current >= 0) combo->setCurrentIndex(combo->findData(current));
}

QPixmap tickMarkPm(TickMarkType type, int size, bool withBG = false) {
  QPixmap pm(size, size);
  QPointF center(double(size) * 0.5, double(size) * 0.5);

  pm.fill((withBG) ? Qt::white : Qt::transparent);
  QPainter p(&pm);
  QPen pen(Qt::black);
  pen.setWidthF(double(size) / 15.0);
  p.setPen(pen);
  switch (type) {
  case TickMark_Dot: {
    p.setBrush(Qt::black);
    double dotR = double(size) * 0.1;
    p.drawEllipse(center, dotR, dotR);
    break;
  }
  case TickMark_Circle: {
    double circleR = double(size) * 0.4;
    p.drawEllipse(center, circleR, circleR);
    break;
  }
  case TickMark_Filled: {
    p.setBrush(Qt::black);
    double circleR = double(size) * 0.4;
    p.drawEllipse(center, circleR, circleR);
    break;
  }
  case TickMark_Asterisk: {
    QFont font = p.font();
    font.setPixelSize(size);
    p.setFont(font);
    p.drawText(0, 0, size, size, Qt::AlignCenter, "*");
    break;
  }
  }
  return pm;
}

QComboBox* createTickMarkCombo(QWidget* parent) {
  QComboBox* combo = new QComboBox(parent);
  combo->addItem(QIcon(tickMarkPm(TickMark_Dot, 15, true)),
                 QObject::tr("Dot", "XSheetPDF CellMark"), TickMark_Dot);
  combo->addItem(QIcon(tickMarkPm(TickMark_Circle, 15, true)),
                 QObject::tr("Circle", "XSheetPDF CellMark"), TickMark_Circle);
  combo->addItem(QIcon(tickMarkPm(TickMark_Filled, 15, true)),
                 QObject::tr("Filled circle", "XSheetPDF CellMark"),
                 TickMark_Filled);
  combo->addItem(QIcon(tickMarkPm(TickMark_Asterisk, 15, true)),
                 QObject::tr("Asterisk", "XSheetPDF CellMark"),
                 TickMark_Asterisk);
  return combo;
}

void doDrawText(QPainter& p, const QString& str, const QRect rect) {
  QFontMetrics fm(p.font());
  int spaceWidth = fm.boundingRect('A').width();
  // check if spaces can be iserted between letters
  int textWidth = fm.boundingRect(str).width() + spaceWidth * (str.count() - 1);
  if (rect.width() - spaceWidth * 2 <= textWidth) {
    p.drawText(rect, Qt::AlignCenter, str);
    return;
  }
  // check if spaces can be doubled
  int textWidth_s2 =
      fm.boundingRect(str).width() + spaceWidth * 2 * (str.count() - 1);
  if (rect.width() - spaceWidth * 4 > textWidth_s2) textWidth = textWidth_s2;
  QRect textRect(rect.center().x() - textWidth / 2, rect.y(), textWidth,
                 rect.height());
  p.drawText(textRect,
             Qt::TextJustificationForced | Qt::AlignJustify | Qt::AlignVCenter,
             str);
}

void setFontFittingRectWidth(QPainter& p, const QString& str, const QRect rect,
                             double vmargin = 0.5, double hmargin = 1.0) {
  QFont font    = p.font();
  int pixelSize = rect.height() - mm2px(vmargin);
  while (1) {
    font.setPixelSize(pixelSize);
    if (pixelSize <= mm2px(2) || QFontMetrics(font).boundingRect(str).width() <=
                                     rect.width() - mm2px(hmargin))
      break;
    pixelSize -= mm2px(0.1);
  }
  p.setFont(font);
}

}  // namespace
//---------------------------------------------------------

void XSheetPDFTemplate::drawGrid(QPainter& painter, int colAmount, int colWidth,
                                 int blockWidth) {
  // horiontal lines
  painter.save();
  {
    // loop 3 seconds
    for (int sec = 0; sec < 3; sec++) {
      painter.save();
      {
        for (int f = 1; f <= 24; f++) {
          if (f % 6 == 0)
            painter.setPen(thickPen);
          else
            painter.setPen(thinPen);
          painter.translate(0, param(RowHeight));
          painter.drawLine(0, 0, blockWidth, 0);
        }
      }
      painter.restore();

      painter.translate(0, param(OneSecHeight));
      painter.setPen(thickPen);
      painter.drawLine(0, 0, blockWidth, 0);
    }
  }
  painter.restore();
  // vertical lines
  painter.save();
  {
    painter.setPen(thinPen);
    for (int kc = 0; kc < colAmount; kc++) {
      painter.translate(colWidth, 0);
      painter.drawLine(0, 0, 0, param(OneSecHeight) * 3);
    }
  }
  painter.restore();
}

void XSheetPDFTemplate::drawHeaderGrid(QPainter& painter, int colAmount,
                                       int colWidth, int blockWidth) {
  // Cells Block header
  painter.save();
  {
    // horizontal lines
    painter.setPen(thinPen);
    painter.drawLine(0, param(HeaderHeight) / 2, blockWidth,
                     param(HeaderHeight) / 2);
    painter.setPen(thickPen);
    painter.drawLine(0, param(HeaderHeight), blockWidth, param(HeaderHeight));
    // vertical lines
    painter.setPen(thinPen);
    painter.save();
    {
      for (int col = 1; col < colAmount; col++) {
        painter.translate(colWidth, 0);
        painter.drawLine(0, param(HeaderHeight) / 2, 0, param(HeaderHeight));
      }
    }
    painter.restore();
  }
  painter.restore();
}

void XSheetPDFTemplate::registerColLabelRects(QPainter& painter, int colAmount,
                                              int colWidth, int bodyId) {
  painter.save();
  {
    for (int kc = 0; kc < colAmount; kc++) {
      QRect labelRect(0, 0, colWidth, param(HeaderHeight) / 2);
      labelRect = painter.transform().mapRect(labelRect);

      if (bodyId == 0) {
        QList<QRect> labelRects = {labelRect};
        m_colLabelRects.append(labelRects);
      } else
        m_colLabelRects[kc].append(labelRect);

      painter.translate(colWidth, 0);
    }
  }
  painter.restore();

  if (!m_info.drawLevelNameOnBottom) return;

  // register bottom rects
  painter.save();
  {
    painter.translate(0, param(BodyHeight) - param(HeaderHeight) / 2);

    for (int kc = 0; kc < colAmount; kc++) {
      QRect labelRect(0, 0, colWidth, param(HeaderHeight) / 2);
      labelRect = painter.transform().mapRect(labelRect);

      if (bodyId == 0) {
        QList<QRect> labelRects = {labelRect};
        m_colLabelRects_bottom.append(labelRects);
      } else
        m_colLabelRects_bottom[kc].append(labelRect);

      painter.translate(colWidth, 0);
    }
  }
  painter.restore();
}

void XSheetPDFTemplate::registerCellRects(QPainter& painter, int colAmount,
                                          int colWidth, int bodyId) {
  int heightAdj = (param(OneSecHeight) - param(RowHeight) * 24) / 2;
  painter.save();
  {
    for (int kc = 0; kc < colAmount; kc++) {
      QList<QRect> colCellRects;

      painter.save();
      {
        for (int sec = 0; sec < 3; sec++) {
          painter.save();
          {
            for (int f = 1; f <= 24; f++) {
              QRect cellRect(0, 0, colWidth, param(RowHeight));
              // fill gap between the doubled lines between seconds
              if (sec != 0 && f == 1)
                cellRect.adjust(0, -heightAdj, 0, 0);
              else if (sec != 2 && f == 24)
                cellRect.adjust(0, 0, 0, heightAdj);

              colCellRects.append(painter.transform().mapRect(cellRect));
              painter.translate(0, param(RowHeight));
            }
          }
          painter.restore();
          painter.translate(0, param(OneSecHeight));
        }
      }
      painter.restore();

      if (bodyId == 0)
        m_cellRects.append(colCellRects);
      else
        m_cellRects[kc].append(colCellRects);

      painter.translate(colWidth, 0);
    }
  }
  painter.restore();
}

void XSheetPDFTemplate::registerSoundRects(QPainter& painter, int colWidth,
                                           int bodyId) {
  int heightAdj = (param(OneSecHeight) - param(RowHeight) * 24) / 2;
  painter.save();
  {
    painter.save();
    {
      for (int sec = 0; sec < 3; sec++) {
        painter.save();
        {
          for (int f = 1; f <= 24; f++) {
            QRect cellRect(0, 0, colWidth, param(RowHeight));
            // fill gap between the doubled lines between seconds
            if (sec != 0 && f == 1)
              cellRect.adjust(0, -heightAdj, 0, 0);
            else if (sec != 2 && f == 24)
              cellRect.adjust(0, 0, 0, heightAdj);

            m_soundCellRects.append(painter.transform().mapRect(cellRect));
            painter.translate(0, param(RowHeight));
          }
        }
        painter.restore();
        painter.translate(0, param(OneSecHeight));
      }
    }
    painter.restore();
    painter.translate(colWidth, 0);
  }
  painter.restore();
}
// Key Block
void XSheetPDFTemplate::drawKeyBlock(QPainter& painter, int framePage,
                                     const int bodyId) {
  QFont font = painter.font();
  font.setPixelSize(m_p.bodylabelTextSize_Small);
  painter.setFont(font);

  painter.save();
  {
    drawHeaderGrid(painter, param(KeyColAmount), param(KeyColWidth),
                   m_p.keyBlockWidth);

    if (m_info.exportArea == Area_Actions) {
      painter.save();
      {
        painter.translate(0, param(HeaderHeight) / 2);
        // register actions rects.
        registerColLabelRects(painter, columnsInPage(), param(KeyColWidth),
                              bodyId);
      }
      painter.restore();
    }

    QRect labelRect(0, 0, m_p.keyBlockWidth, param(HeaderHeight) / 2);
    QString actionLabel = (param(TranslateBodyLabel, 1) == 1)
                              ? QObject::tr("ACTION", "XSheetPDF")
                              : "ACTION";
    doDrawText(painter, actionLabel, labelRect);

    painter.save();
    {
      painter.translate(0, param(HeaderHeight));

      // Key Animation Block
      drawGrid(painter, param(KeyColAmount), param(KeyColWidth),
               m_p.keyBlockWidth);

      if (m_info.exportArea == Area_Actions)
        // register cell rects.
        registerCellRects(painter, columnsInPage(), param(KeyColWidth), bodyId);

      // frame numbers
      painter.save();
      {
        font.setPixelSize(param(RowHeight) - mm2px(2));
        font.setLetterSpacing(QFont::PercentageSpacing, 100);
        painter.setFont(font);

        if (param(LastKeyColWidth) > 0) {
          painter.translate(param(KeyColAmount) * param(KeyColWidth), 0);
          for (int sec = 0; sec < 3; sec++) {
            painter.save();
            {
              for (int f = 1; f <= 24; f++) {
                if (f % 2 == 0) {
                  int frame = bodyId * 72 + sec * 24 + f;
                  if (m_info.serialFrameNumber)
                    frame += param(FrameLength) * framePage;
                  QRect frameLabelRect(0, 0,
                                       param(LastKeyColWidth) - mm2px(0.5),
                                       param(RowHeight));
                  painter.drawText(frameLabelRect,
                                   Qt::AlignRight | Qt::AlignVCenter,
                                   QString::number(frame));
                }
                painter.translate(0, param(RowHeight));
              }
            }
            painter.restore();
            painter.translate(0, param(OneSecHeight));
          }
        }
        // draw frame numbers on the left side of the block
        else {
          painter.translate(-param(KeyColWidth) * 2, 0);
          for (int sec = 0; sec < 3; sec++) {
            painter.save();
            {
              for (int f = 1; f <= 24; f++) {
                if (f % 2 == 0) {
                  int frame = bodyId * 72 + sec * 24 + f;
                  if (m_info.serialFrameNumber)
                    frame += param(FrameLength) * framePage;
                  QRect frameLabelRect(0, 0,
                                       param(KeyColWidth) * 2 - mm2px(0.5),
                                       param(RowHeight));
                  painter.drawText(frameLabelRect,
                                   Qt::AlignRight | Qt::AlignVCenter,
                                   QString::number(frame));
                }
                painter.translate(0, param(RowHeight));
              }
            }
            painter.restore();
            painter.translate(0, param(OneSecHeight));
          }
        }
      }
      painter.restore();
    }
    painter.restore();

    painter.translate(m_p.keyBlockWidth, 0);
    painter.setPen(blockBorderPen);
    painter.drawLine(0, 0, 0, param(BodyHeight));
  }
  painter.restore();
}

void XSheetPDFTemplate::drawDialogBlock(QPainter& painter, const int framePage,
                                        const int bodyId) {
  QFont font = painter.font();
  font.setPixelSize(m_p.bodylabelTextSize_Large);
  font.setLetterSpacing(QFont::PercentageSpacing, 100);

  QRect labelRect(0, 0, param(DialogColWidth), param(HeaderHeight));
  QString serifLabel =
      (param(TranslateBodyLabel, 1) == 1) ? QObject::tr("S", "XSheetPDF") : "S";
  while (font.pixelSize() > mm2px(1)) {
    if (QFontMetrics(font).boundingRect(serifLabel).width() <
        labelRect.width() - mm2px(1))
      break;
    font.setPixelSize(font.pixelSize() - mm2px(0.5));
  }
  painter.setFont(font);
  painter.drawText(labelRect, Qt::AlignCenter, serifLabel);

  // triangle shapes at every half seconds
  static const QPointF points[3] = {
      QPointF(mm2px(-0.8), 0.0),
      QPointF(mm2px(-2.8), mm2px(1.25)),
      QPointF(mm2px(-2.8), mm2px(-1.25)),
  };
  QRect secLabelRect(0, 0, param(DialogColWidth) - mm2px(1.0),
                     param(RowHeight));

  painter.save();
  {
    font.setPixelSize(param(RowHeight) - mm2px(1.5));
    painter.setFont(font);

    painter.translate(0, param(HeaderHeight));
    painter.save();
    {
      for (int sec = 1; sec <= 3; sec++) {
        painter.save();
        {
          painter.setBrush(painter.pen().color());
          painter.setPen(Qt::NoPen);
          painter.translate(param(DialogColWidth), param(RowHeight) * 12);
          painter.drawPolygon(points, 3);
        }
        painter.restore();
        painter.save();
        {
          int second = bodyId * 3 + sec;
          if (m_info.serialFrameNumber)
            second += framePage * param(FrameLength) / 24;
          painter.translate(0, param(RowHeight) * ((sec == 3) ? 23 : 23.5));
          painter.drawText(secLabelRect, Qt::AlignRight | Qt::AlignVCenter,
                           QString::number(second));
        }
        painter.restore();
        painter.translate(0, param(OneSecHeight));
      }
    }
    painter.restore();

    // register sound cells
    if (m_info.drawSound || m_noteColumn)
      registerSoundRects(painter, param(DialogColWidth), bodyId);
  }
  painter.restore();

  painter.save();
  {
    painter.setPen(blockBorderPen);
    painter.translate(param(DialogColWidth), 0);
    painter.drawLine(0, 0, 0, param(BodyHeight));
  }
  painter.restore();
}

void XSheetPDFTemplate::drawCellsBlock(QPainter& painter, int bodyId) {
  QFont font = painter.font();
  font.setPixelSize(m_p.bodylabelTextSize_Small);
  painter.setFont(font);

  painter.save();
  {
    drawHeaderGrid(painter, param(CellsColAmount), param(CellsColWidth),
                   m_p.cellsBlockWidth);

    if (m_info.exportArea == Area_Cells) {
      painter.save();
      {
        painter.translate(0, param(HeaderHeight) / 2);
        // register cell rects
        registerColLabelRects(painter, columnsInPage(), param(CellsColWidth),
                              bodyId);
      }
      painter.restore();
    }

    if (param(DrawCellsHeaderLabel, 1) == 1) {
      QRect labelRect(0, 0, m_p.cellsBlockWidth, param(HeaderHeight) / 2);
      QString cellsLabel = (param(TranslateBodyLabel, 1) == 1)
                               ? QObject::tr("CELL", "XSheetPDF")
                               : "CELL";
      doDrawText(painter, cellsLabel, labelRect);
    }

    painter.save();
    {
      painter.translate(0, param(HeaderHeight));
      // Cells Block
      drawGrid(painter, param(CellsColAmount) - 1, param(CellsColWidth),
               m_p.cellsBlockWidth);

      if (m_info.exportArea == Area_Cells)
        // register cell rects.
        registerCellRects(painter, columnsInPage(), param(CellsColWidth),
                          bodyId);
    }
    painter.restore();

    painter.setPen(blockBorderPen);
    painter.translate(m_p.cellsBlockWidth, 0);
    painter.drawLine(0, 0, 0, param(BodyHeight));
  }
  painter.restore();
}

void XSheetPDFTemplate::drawCameraBlock(QPainter& painter) {
  QFont font = painter.font();
  font.setPixelSize(m_p.bodylabelTextSize_Large);
  painter.setFont(font);

  painter.save();
  {
    // Camera Block header
    painter.save();
    {
      QString cameraLabel = (param(TranslateBodyLabel, 1) == 1)
                                ? QObject::tr("CAMERA", "XSheetPDF")
                                : "CAMERA";
      if (param(DrawCameraHeaderGrid, 0) == 1) {
        drawHeaderGrid(painter, param(CameraColAmount), param(CameraColWidth),
                       m_p.cameraBlockWidth);
        if (param(DrawCameraHeaderLabel, 1) == 1) {
          font.setPixelSize(m_p.bodylabelTextSize_Small);
          painter.setFont(font);
          QRect labelRect(0, 0, m_p.cameraBlockWidth, param(HeaderHeight) / 2);
          doDrawText(painter, cameraLabel, labelRect);
        }
      } else {
        // horizontal lines
        painter.setPen(thickPen);
        painter.drawLine(0, param(HeaderHeight), m_p.cameraBlockWidth,
                         param(HeaderHeight));

        if (param(DrawCameraHeaderLabel, 1) == 1) {
          font.setPixelSize(m_p.bodylabelTextSize_Large);
          painter.setFont(font);
          QRect labelRect(0, 0, m_p.cameraBlockWidth, param(HeaderHeight));
          doDrawText(painter, cameraLabel, labelRect);
        }
      }
    }
    painter.restore();

    if (param(DrawCameraGrid, 1) != 0 || m_useExtraColumns) {
      painter.save();
      {
        painter.translate(0, param(HeaderHeight));
        // Cells Block
        drawGrid(painter, param(CameraColAmount) - 1, param(CameraColWidth),
                 m_p.cameraBlockWidth);
      }
      painter.restore();
    }
  }
  painter.restore();
}

void XSheetPDFTemplate::drawXsheetBody(QPainter& painter, int framePage,
                                       int bodyId) {
  // Body
  painter.save();
  {
    painter.setPen(bodyOutlinePen);
    painter.drawRect(QRect(0, 0, param(BodyWidth), param(BodyHeight)));

    drawKeyBlock(painter, framePage, bodyId);
    painter.translate(m_p.keyBlockWidth, 0);
    drawDialogBlock(painter, framePage, bodyId);
    painter.translate(param(DialogColWidth), 0);
    drawCellsBlock(painter, bodyId);
    painter.translate(m_p.cellsBlockWidth, 0);
    drawCameraBlock(painter);
  }
  painter.restore();
}

void XSheetPDFTemplate::drawInfoHeader(QPainter& painter) {
  painter.save();
  {
    painter.translate(param(InfoOriginLeft), param(InfoOriginTop));
    QFont font = painter.font();
    font.setPixelSize(param(InfoTitleHeight) - mm2px(2));
    painter.setFont(font);
    bool isLeftMost = true;
    // draw each info
    for (auto info : m_p.array_Infos) {
      painter.setPen((isLeftMost) ? bodyOutlinePen : thinPen);
      isLeftMost = false;
      // vertical line
      painter.drawLine(0, 0, 0, m_p.infoHeaderHeight);
      // 3 horizontal lines
      painter.setPen(thinPen);
      painter.drawLine(0, param(InfoTitleHeight), info.width,
                       param(InfoTitleHeight));
      painter.setPen(bodyOutlinePen);
      painter.drawLine(0, 0, info.width, 0);
      painter.drawLine(0, m_p.infoHeaderHeight, info.width,
                       m_p.infoHeaderHeight);

      painter.setPen(thinPen);
      // label
      QRect labelRect(0, 0, info.width, param(InfoTitleHeight));
      doDrawText(painter, info.label, labelRect);

      if (info.decoFunc) {
        painter.save();
        {
          painter.translate(0, param(InfoTitleHeight));
          (*info.decoFunc)(painter,
                           QRect(0, 0, info.width, param(InfoBodyHeight)),
                           m_dataRects, param(TranslateInfoLabel, 1));
        }
        painter.restore();
      }

      // translate
      painter.translate(info.width, 0);
    }
    // vertical line at the rightmost edge
    painter.setPen(bodyOutlinePen);
    painter.drawLine(0, 0, 0, m_p.infoHeaderHeight);
  }
  painter.restore();
}

void XSheetPDFTemplate::addInfo(int w, QString lbl, DecoFunc f) {
  XSheetPDF_InfoFormat info;
  info.width    = w;
  info.label    = lbl;
  info.decoFunc = f;
  m_p.array_Infos.append(info);
};

void XSheetPDFTemplate::drawContinuousLine(QPainter& painter, QRect rect,
                                           bool isEmpty) {
  if (isEmpty) {
    int offset = rect.height() / 4;
    QPoint p0(rect.center().x(), rect.top());
    QPoint p3(rect.center().x(), rect.bottom());
    QPoint p1 = p0 + QPoint(-offset, offset);
    QPoint p2 = p3 + QPoint(offset, -offset);
    QPainterPath path(p0);
    path.cubicTo(p1, p2, p3);
    painter.drawPath(path);
  } else
    painter.drawLine(rect.center().x(), rect.top(), rect.center().x(),
                     rect.bottom());
}

void XSheetPDFTemplate::drawCellNumber(QPainter& painter, QRect rect,
                                       TXshCell& cell, bool isKey) {
  QFont font = painter.font();
  font.setPixelSize(param(RowHeight) - mm2px(1));
  font.setLetterSpacing(QFont::PercentageSpacing, 100);
  painter.setFont(font);

  if (cell.isEmpty()) {
    painter.drawLine(rect.topLeft(), rect.bottomRight());
    painter.drawLine(rect.topRight(), rect.bottomLeft());
  } else {
    QString str;
    if (Preferences::instance()->isShowFrameNumberWithLettersEnabled()) {
      str = getFrameNumberWithLetters(cell.m_frameId.getNumber());
    } else {
      str = QString::number(cell.m_frameId.getNumber());
      if (!cell.m_frameId.getLetter().isEmpty())
        str += cell.m_frameId.getLetter();
    }
    painter.drawText(rect, Qt::AlignCenter, str);
    if (isKey) {
      QPen keep(painter.pen());
      QPen circlePen(keep);
      circlePen.setWidth(mm2px(0.3));
      painter.setPen(circlePen);
      QFontMetrics fm(font);
      int keyR_width =
          std::max(param(RowHeight), fm.horizontalAdvance(str) + mm2px(1));
      QRect keyR(0, 0, keyR_width, param(RowHeight));
      keyR.moveCenter(rect.center());
      painter.drawEllipse(keyR);
      painter.setPen(keep);
    }
  }
}

void XSheetPDFTemplate::drawTickMark(QPainter& painter, QRect rect,
                                     TickMarkType type) {
  QRect tickR(0, 0, rect.height(), rect.height());
  tickR.moveCenter(rect.center());
  painter.drawPixmap(tickR, tickMarkPm(type, rect.height()));
}

void XSheetPDFTemplate::drawEndMark(QPainter& painter, QRect upperRect) {
  QRect rect = upperRect.translated(0, upperRect.height());

  painter.drawLine(rect.topLeft(), rect.topRight());

  painter.drawLine(rect.center().x(), rect.top(), rect.left(),
                   rect.center().y());
  painter.drawLine(rect.topRight(), rect.bottomLeft());
  painter.drawLine(rect.right(), rect.center().y(), rect.center().x(),
                   rect.bottom());
}

void XSheetPDFTemplate::drawLevelName(QPainter& painter, QRect rect,
                                      QString name, bool isBottom) {
  QFont font = painter.font();
  font.setPixelSize(rect.height());
  font.setLetterSpacing(QFont::PercentageSpacing, 100);
  painter.setFont(font);

  // if it can fit in the rect, then just draw it
  if (QFontMetrics(font).boundingRect(name).width() < rect.width()) {
    painter.drawText(rect, Qt::AlignCenter, name);
    return;
  }

  // if it can fit with 90% sized font
  QFont altFont(font);
  altFont.setPixelSize(font.pixelSize() * 0.9);
  if (QFontMetrics(altFont).boundingRect(name).width() < rect.width()) {
    painter.setFont(altFont);
    painter.drawText(rect, Qt::AlignCenter, name);
    return;
  }

  // or, draw level name vertically
  // insert line breaks to every characters
  for (int i = 1; i < name.size(); i += 2) name.insert(i, '\n');
  // QFontMetrics::boundingRect(const QString &text) does NOT process newline
  // characters as linebreaks.
  QRect boundingRect = QFontMetrics(font).boundingRect(
      QRect(0, 0, mm2px(100), mm2px(100)), Qt::AlignTop | Qt::AlignLeft, name);
  if (!isBottom) {
    boundingRect.moveCenter(
        QPoint(rect.center().x(), rect.bottom() - boundingRect.height() / 2));
  } else {
    boundingRect.moveCenter(
        QPoint(rect.center().x(), rect.top() + boundingRect.height() / 2));
  }

  // fill background of the label
  painter.fillRect(boundingRect, Qt::white);
  painter.drawText(boundingRect, Qt::AlignCenter, name);
}

void XSheetPDFTemplate::drawLogo(QPainter& painter) {
  if (!m_dataRects.contains(Data_Logo)) return;
  if (m_info.logoText.isEmpty() && m_info.logoPixmap.isNull()) return;
  QRect logoRect = m_dataRects.value(Data_Logo);

  if (!m_info.logoText.isEmpty()) {
    QFont font = painter.font();
    font.setPixelSize(logoRect.height() - mm2px(2));
    while (1) {
      if (QFontMetrics(font).boundingRect(m_info.logoText).width() <
              logoRect.width() &&
          QFontMetrics(font).boundingRect(m_info.logoText).height() <
              logoRect.height())
        break;
      if (font.pixelSize() <= mm2px(1)) break;
      font.setPixelSize(font.pixelSize() - mm2px(1));
    }
    painter.setFont(font);
    painter.drawText(logoRect, Qt::AlignTop | Qt::AlignLeft, m_info.logoText);
  }

  else if (!m_info.logoPixmap.isNull()) {
    painter.drawPixmap(logoRect.topLeft(), m_info.logoPixmap);
  }
}

void XSheetPDFTemplate::drawSound(QPainter& painter, int framePage) {
  if (!m_info.drawSound || m_soundColumns.isEmpty() ||
      m_soundCellRects.isEmpty())
    return;

  // obtain united range of the sound columns
  int r0 = INT_MAX, r1 = -1;
  for (auto soundCol : m_soundColumns) {
    int tmpR0, tmpR1;
    soundCol->getRange(tmpR0, tmpR1);
    r0 = std::min(r0, tmpR0);
    r1 = std::max(r1, tmpR1);
  }

  // obtain frame range to be printed in the current page
  int printFrameR0 = framePage * param(FrameLength);
  int printFrameR1 = printFrameR0 + param(FrameLength) - 1;

  // return if the current page is out of range
  if (r1 < printFrameR0 || printFrameR1 < r0) return;

  const Orientation* o = Orientations::instance().topToBottom();
  int oneFrameHeight   = o->dimension(PredefinedDimension::FRAME);
  int trackWidth = o->layerSide(o->rect(PredefinedRect::SOUND_TRACK)).length();

  painter.setRenderHints(QPainter::Antialiasing |
                         QPainter::SmoothPixmapTransform);

  // frame range to be printed
  for (int f = std::max(r0, printFrameR0); f <= std::min(r1, printFrameR1);
       f++) {
    int rectId = f - printFrameR0;

    QPixmap img(trackWidth, oneFrameHeight);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setPen(QPen(m_info.lineColor, 1, Qt::SolidLine, Qt::FlatCap));
    p.translate(trackWidth / 2, 0);
    for (auto soundCol : m_soundColumns) {
      TXshCell cell = soundCol->getSoundCell(f);
      if (soundCol->isCellEmpty(f) || cell.isEmpty() ||
          f > soundCol->getMaxFrame() + 1 || f < soundCol->getFirstRow())
        continue;

      TXshSoundLevelP soundLevel = cell.getSoundLevel();

      int soundPixel = cell.getFrameId().getNumber() *
                       oneFrameHeight;  // pixels since start of clip

      for (int i = 0; i < oneFrameHeight; i++) {
        DoublePair minmax;
        soundLevel->getValueAtPixel(o, soundPixel, minmax);
        p.drawLine(minmax.first, i, minmax.second, i);
        soundPixel++;
      }
    }
    p.end();

    QRect rect = m_soundCellRects.at(rectId);

    painter.drawPixmap(rect, img.scaled(rect.size()));
  }
}

void XSheetPDFTemplate::drawDialogue(QPainter& painter, int framePage) {
  if (!m_noteColumn || m_soundCellRects.isEmpty()) return;

  QFont font = painter.font();
  font.setPixelSize(m_soundCellRects[0].height());
  painter.setFont(font);

  QFont smallFont(font);
  smallFont.setPixelSize(font.pixelSize() * 2 / 3);

  QFont largeFont(font);
  largeFont.setPixelSize(font.pixelSize() * 13 / 10);
  int heightThres = QFontMetrics(largeFont).height();

  int r0, r1;
  m_noteColumn->getRange(r0, r1);

  // obtain frame range to be printed in the current page
  int printFrameR0 = framePage * param(FrameLength);
  int printFrameR1 = printFrameR0 + param(FrameLength) - 1;

  // compute for each body
  int framesPerBody = 72;
  int bodyFrameR0   = printFrameR0;
  int bodyFrameR1   = bodyFrameR0 + framesPerBody - 1;
  while (bodyFrameR1 <= printFrameR1) {
    // move to next body if the current body is out of range
    if (r1 < bodyFrameR0 || bodyFrameR1 < r0) {
      // to next body
      bodyFrameR0 = bodyFrameR1 + 1;
      bodyFrameR1 = bodyFrameR0 + framesPerBody - 1;
      continue;
    }

    // frame range to be printed
    int drawStart = std::max(r0, bodyFrameR0);
    int drawEnd   = std::min(r1, std::min(bodyFrameR1, printFrameR1));

    int rStart = drawStart;
    int rEnd   = drawEnd;
    // obtain top row of the fist note block
    if (!m_noteColumn->getCell(drawStart).isEmpty()) {
      while (rStart > 0 && m_noteColumn->getCell(rStart - 1) ==
                               m_noteColumn->getCell(drawStart)) {
        rStart--;
      }
    }
    // obtain bottom row of the last note block
    if (!m_noteColumn->getCell(drawEnd).isEmpty()) {
      while (m_noteColumn->getCell(rEnd + 1) ==
             m_noteColumn->getCell(drawEnd)) {
        rEnd++;
      }
    }

    for (int row = rStart; row <= drawEnd; row++) {
      TXshCell cell = m_noteColumn->getCell(row);
      if (cell.isEmpty()) continue;

      // check how long the same content continues
      int rowTo = row;
      while (m_noteColumn->getCell(rowTo + 1) == cell) {
        rowTo++;
      }
      int blockLength = rowTo - row + 1;

      QString text = cell.getSoundTextLevel()->getFrameText(
          cell.m_frameId.getNumber() - 1);
      int textCount = text.count();
      // separate text if it overflows the body
      if (row < drawStart) {
        int partialBlockLength = rowTo - drawStart + 1;
        int partialTextCount   = (int)std::round(
              (double)(textCount * partialBlockLength) / (double)blockLength);
        text        = text.mid(textCount - partialTextCount);
        textCount   = partialTextCount;
        row         = drawStart;
        blockLength = partialBlockLength;
      }
      // draw start mark
      else {
        int topRectId = row - printFrameR0;
        QRect rect    = m_soundCellRects.at(topRectId);
        painter.drawLine(rect.topLeft(), rect.topRight());
      }

      if (rowTo > drawEnd) {
        int partialBlockLength = drawEnd - row + 1;
        int partialTextCount   = (int)std::round(
              (double)(textCount * partialBlockLength) / (double)blockLength);
        text      = text.mid(0, partialTextCount);
        textCount = partialTextCount;
        rowTo     = drawEnd;
      }
      // draw end mark
      else if (m_noteColumn->getCell(rowTo + 1).isEmpty()) {
        int bottomRectId = rowTo - printFrameR0;
        QRect rect       = m_soundCellRects.at(bottomRectId);
        drawEndMark(painter, rect);
      }
      if (text.isEmpty()) {
        row = rowTo;
        continue;
      }

      int normalLettersPerChunk = textCount * m_soundCellRects[0].width() /
                                  QFontMetrics(font).boundingRect(text).width();
      int maxLettersPerChunk =
          textCount * m_soundCellRects[0].width() /
          QFontMetrics(smallFont).boundingRect(text).width();

      int lettersPerChunk =
          (int)std::ceil((double)textCount / ((double)blockLength));
      lettersPerChunk = std::min(lettersPerChunk, maxLettersPerChunk);
      int chunkCount =
          (int)std::ceil((double)textCount / (double)(lettersPerChunk));
      chunkCount = std::min(chunkCount, (int)((double)(blockLength)*1.5));

      int topRectId    = row - printFrameR0;
      int bottomRectId = rowTo - printFrameR0;
      // unite the cell rects and divide by the amount of chunks
      QRect unitedRect = m_soundCellRects.at(topRectId).united(
          m_soundCellRects.at(bottomRectId));
      // check if the large font is available
      if (lettersPerChunk == 1 && unitedRect.height() / textCount > heightThres)
        painter.setFont(largeFont);
      else if (lettersPerChunk > normalLettersPerChunk)
        painter.setFont(smallFont);
      else
        painter.setFont(font);
      // draw text
      for (int c = 0; c < chunkCount; c++) {
        int y0 = unitedRect.top() + unitedRect.height() * c / chunkCount;
        int y1 = unitedRect.top() + unitedRect.height() * (c + 1) / chunkCount;
        QRect tmpRect(unitedRect.left(), y0, unitedRect.width(), y1 - y0 + 1);
        painter.drawText(tmpRect, Qt::AlignCenter,
                         text.mid(c * lettersPerChunk, lettersPerChunk));
      }

      row = rowTo;
    }
    // to next body
    bodyFrameR0 = bodyFrameR1 + 1;
    bodyFrameR1 = bodyFrameR0 + framesPerBody - 1;
  }
}

XSheetPDFTemplate::XSheetPDFTemplate(
    const QList<QPair<TXshLevelColumn*, QString>>& columns, const int duration)
    : m_columns(columns), m_duration(duration), m_useExtraColumns(false) {}

void XSheetPDFTemplate::setInfo(const XSheetPDFFormatInfo& info) {
  m_info         = info;
  thinPen        = QPen(info.lineColor, param(ThinLineWidth, mm2px(0.25)),
                        Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
  thickPen       = QPen(info.lineColor, param(ThickLineWidth, mm2px(0.5)),
                        Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
  bodyOutlinePen = QPen(info.lineColor, param(BodyOutlineWidth, mm2px(0.5)),
                        Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
  blockBorderPen = (param(IsBlockBorderThick, 0) > 0) ? thickPen : thinPen;
  // check if it should use extra columns
  if (info.exportArea == Area_Cells && param(ExtraCellsColAmount, 0) > 0) {
    int colsInScene   = m_columns.size();
    int colsInPage    = param(CellsColAmount);
    int colsInPage_Ex = param(CellsColAmount) + param(ExtraCellsColAmount);
    auto getPageNum   = [&](int cip) {
      int ret = colsInScene / cip;
      if (colsInScene % cip != 0 || colsInScene == 0) ret += 1;
      return ret;
    };
    if (getPageNum(colsInPage) > getPageNum(colsInPage_Ex))
      m_useExtraColumns = true;
  }
}

void XSheetPDFTemplate::drawXsheetTemplate(QPainter& painter, int framePage,
                                           bool isPreview) {
  // clear rects
  m_colLabelRects.clear();
  m_colLabelRects_bottom.clear();
  m_cellRects.clear();
  m_soundCellRects.clear();

  // painter.setFont(QFont("Times New Roman"));
  painter.setFont(m_info.templateFontFamily);
  painter.setPen(thinPen);

  painter.save();
  {
    if (isPreview)
      painter.translate(mm2px(m_p.documentMargin.left()),
                        mm2px(m_p.documentMargin.top()));

    drawLogo(painter);

    // draw Info header
    drawInfoHeader(painter);

    painter.translate(0, param(BodyTop));
    for (int bId = 0; bId < param(BodyAmount); bId++) {
      drawXsheetBody(painter, framePage, bId);
      painter.translate(param(BodyWidth) + param(BodyHMargin), 0);
    }
  }
  painter.restore();
}

void XSheetPDFTemplate::drawXsheetContents(QPainter& painter, int framePage,
                                           int parallelPage, bool isPreview) {
  auto checkContinuous = [&](const TXshLevelColumn* column, int f, int r) {
    if (m_info.continuousLineMode == Line_Always)
      return true;
    else if (m_info.continuousLineMode == Line_None)
      return false;
    TXshCell cell = column->getCell(f);
    // check subsequent cells and see if more than 3 cells continue.
    int tmp_r = r + 1;
    for (int tmp_f = f + 1; tmp_f <= f + 3; tmp_f++, tmp_r++) {
      if (tmp_f == m_duration) return false;
      if (tmp_r % 72 == 0) return false;  // step over to the next body
      if (column->getCell(tmp_f) != cell) return false;
      // tickmark breaks continuous line
      int markId = column->getCellMark(tmp_f);
      if (markId >= 0 &&
          (m_info.tick1MarkId == markId || m_info.tick2MarkId == markId))
        return false;
    }
    return true;
  };

  // draw soundtrack
  drawSound(painter, framePage);

  painter.setPen(
      QPen(Qt::black, mm2px(0.5), Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
  painter.setFont(m_info.contentsFontFamily);

  // draw dialogue
  drawDialogue(painter, framePage);

  int colsInPage = columnsInPage();
  int startColId = colsInPage * parallelPage;
  int startFrame = param(FrameLength) * framePage;
  int c          = 0, r;
  for (int colId = startColId; c < colsInPage; c++, colId++) {
    if (colId == m_columns.size()) break;

    TXshLevelColumn* column = m_columns.at(colId).first;
    // obtain level in this column
    int r0, r1;
    column->getRange(r0, r1);
    TXshLevelP level = column->getCell(r0).m_level;

    QString columnName = m_columns.at(colId).second;
    if (columnName.isEmpty())
      columnName = QString::fromStdWString(level->getName());

    TXshCell prevCell;
    bool drawCLFlag;

    r = 0;
    for (int f = startFrame; r < param(FrameLength); r++, f++) {
      // draw level name
      if (r % 72 == 0)
        drawLevelName(painter, m_colLabelRects[c][r / 72], columnName);
      // draw level name on bottom
      if (m_info.drawLevelNameOnBottom && r % 72 == 37 &&
          startFrame + 72 != m_duration)
        drawLevelName(painter, m_colLabelRects_bottom[c][r / 72], columnName,
                      true);

      TXshCell cell = column->getCell(f);
      if (cell.m_level != level) cell.m_level = nullptr;

      int markId = column->getCellMark(f);

      // draw tick mark
      if ((prevCell == cell || cell.isEmpty()) && markId >= 0 &&
          (m_info.tick1MarkId == markId || m_info.tick2MarkId == markId)) {
        if (m_info.tick1MarkId == markId)
          drawTickMark(painter, m_cellRects[c][r], m_info.tick1MarkType);
        else
          drawTickMark(painter, m_cellRects[c][r], m_info.tick2MarkType);
        drawCLFlag = checkContinuous(column, f, r);
      }
      // cotinuous line
      else if (r != 0 && r != 72 && prevCell == cell) {
        if (drawCLFlag)
          drawContinuousLine(painter, m_cellRects[c][r], cell.isEmpty());
      }
      // draw cell
      else {
        bool drawKeyMark = (markId >= 0 && m_info.keyMarkId == markId);
        drawCellNumber(painter, m_cellRects[c][r], cell, drawKeyMark);
        drawCLFlag = checkContinuous(column, f, r);
      }
      prevCell = cell;

      if (f == m_duration - 1) {
        // break after drawing the end mark
        drawEndMark(painter, m_cellRects[c][r]);
        break;
      }
    }
  }

  painter.setFont(m_info.templateFontFamily);
  QFont font = painter.font();
  font.setLetterSpacing(QFont::PercentageSpacing, 100);

  // draw data
  painter.save();
  {
    if (isPreview)
      painter.translate(mm2px(m_p.documentMargin.left()),
                        mm2px(m_p.documentMargin.top()));

    if (m_dataRects.contains(Data_Memo) && !m_info.memoText.isEmpty() &&
        framePage == 0) {
      // define the preferable font size
      int lines =
          std::max(m_info.memoText.count("\n") + 1, param(MemoLinesAmount));
      int lineSpacing = m_dataRects.value(Data_Memo).height() / lines;
      int pixelSize   = lineSpacing;
      while (1) {
        font.setPixelSize(pixelSize);
        if (pixelSize <= mm2px(2) ||
            QFontMetrics(font).lineSpacing() <= lineSpacing)
          break;
        pixelSize -= mm2px(0.2);
      }
      painter.setFont(font);
      painter.drawText(m_dataRects.value(Data_Memo),
                       Qt::AlignLeft | Qt::AlignTop, m_info.memoText);
    }

    QString dateTime_scenePath = m_info.dateTimeText;
    if (!m_info.scenePathText.isEmpty()) {
      if (!dateTime_scenePath.isEmpty()) dateTime_scenePath += "\n";
      dateTime_scenePath += m_info.scenePathText;
    }
    if (m_dataRects.contains(Data_DateTimeAndScenePath) &&
        !dateTime_scenePath.isEmpty()) {
      font.setPixelSize(m_p.bodylabelTextSize_Small);
      painter.setFont(font);
      painter.drawText(m_dataRects.value(Data_DateTimeAndScenePath),
                       Qt::AlignRight | Qt::AlignTop, dateTime_scenePath);
    }
  }
  painter.restore();

  if (m_dataRects.contains(Data_Second) && m_duration >= 24) {
    QString str = QString::number(m_duration / 24);
    setFontFittingRectWidth(painter, str, m_dataRects.value(Data_Second));
    painter.drawText(m_dataRects.value(Data_Second), Qt::AlignCenter, str);
  }
  if (m_dataRects.contains(Data_Frame) && m_duration > 0) {
    QString str = QString::number(m_duration % 24);
    setFontFittingRectWidth(painter, str, m_dataRects.value(Data_Frame));
    painter.drawText(m_dataRects.value(Data_Frame), Qt::AlignCenter, str);
  }

  if (m_dataRects.contains(Data_TotalPages)) {
    QString totStr = QString::number(framePageCount());
    if (parallelPageCount() > 1)
      totStr += "x" + QString::number(parallelPageCount());

    setFontFittingRectWidth(painter, totStr,
                            m_dataRects.value(Data_TotalPages));
    painter.drawText(m_dataRects.value(Data_TotalPages), Qt::AlignCenter,
                     totStr);
  }
  if (m_dataRects.contains(Data_CurrentPage)) {
    QString curStr = QString::number(framePage + 1);
    if (parallelPageCount() > 1) curStr += QChar('A' + parallelPage);

    setFontFittingRectWidth(painter, curStr,
                            m_dataRects.value(Data_CurrentPage));
    painter.drawText(m_dataRects.value(Data_CurrentPage),
                     Qt::AlignLeft | Qt::AlignVCenter, curStr);
  }
  if (m_dataRects.contains(Data_SceneName) && !m_info.sceneNameText.isEmpty()) {
    QRect rect = m_dataRects.value(Data_SceneName);
    setFontFittingRectWidth(painter, m_info.sceneNameText, rect);
    painter.drawText(rect, Qt::AlignCenter, m_info.sceneNameText);
  }
}

void XSheetPDFTemplate::initializePage(QPdfWriter& writer) {
  QPageLayout pageLayout;
  pageLayout.setUnits(QPageLayout::Millimeter);
  pageLayout.setPageSize(
      QPageSize(m_p.documentPageSize));  // ïÅí ÇÃB4ÇÕISO B4(250x353mm)
                                         // ì˙ñ{ÇÃB4ÇÕJIS B4(257x364mm)
  pageLayout.setOrientation(QPageLayout::Portrait);
  pageLayout.setMargins(m_p.documentMargin);
  writer.setPageLayout(pageLayout);
  writer.setResolution(PDF_Resolution);
}

QPixmap XSheetPDFTemplate::initializePreview() {
  QSizeF size = QPageSize::definitionSize(m_p.documentPageSize);
  QSize pxSize;
  // convert to px
  switch (QPageSize::definitionUnits(m_p.documentPageSize)) {
  case QPageSize::Millimeter:
    pxSize = QSize(mm2px(size.width()), mm2px(size.height()));
    break;
  case QPageSize::Inch:
    pxSize =
        QSize(size.width() * PDF_Resolution, size.height() * PDF_Resolution);
    break;
  default:
    std::cout << "unsupported unit" << std::endl;
    return QPixmap();
  }
  QPixmap retPm(pxSize);
  retPm.fill(Qt::white);

  return retPm;
}

int XSheetPDFTemplate::framePageCount() {
  int ret = m_duration / param(FrameLength);
  if (m_duration % param(FrameLength) != 0 || m_duration == 0) ret += 1;
  return ret;
}

int XSheetPDFTemplate::parallelPageCount() {
  int colsInPage  = columnsInPage();
  int colsInScene = m_columns.size();

  int ret = colsInScene / colsInPage;
  if (colsInScene % colsInPage != 0 || colsInScene == 0) ret += 1;
  return ret;
}

int XSheetPDFTemplate::columnsInPage() {
  if (m_info.exportArea == Area_Actions) {
    int colsInPage = param(KeyColAmount);
    if (param(LastKeyColWidth) > 0) colsInPage++;
    return colsInPage;
  }

  // CELLS
  if (m_useExtraColumns)
    return param(CellsColAmount) + param(ExtraCellsColAmount, 0);
  return param(CellsColAmount);
}

QSize XSheetPDFTemplate::logoPixelSize() {
  if (!m_dataRects.contains(Data_Logo)) return QSize();
  return m_dataRects.value(Data_Logo).size();
}

void XSheetPDFTemplate::setLogoPixmap(QPixmap pm) { m_info.logoPixmap = pm; }

//---------------------------------------------------------
XSheetPDFTemplate_B4_6sec::XSheetPDFTemplate_B4_6sec(
    const QList<QPair<TXshLevelColumn*, QString>>& columns, const int duration)
    : XSheetPDFTemplate(columns, duration) {
  m_p.documentPageSize = QPageSize::JisB4;               // 257 * 364 mm
  m_p.documentMargin   = QMarginsF(9.5, 6.0, 8.5, 6.0);  // millimeters

  m_params.insert("BodyWidth", mm2px(117));
  m_params.insert("BodyHeight", mm2px(315.5));
  m_params.insert("BodyAmount", 2);
  m_params.insert("BodyHMargin", mm2px(5.0));
  m_params.insert("BodyTop", mm2px(36.5));
  m_params.insert("HeaderHeight", mm2px(6.5));
  m_params.insert("KeyColWidth", mm2px(5.0));
  m_params.insert("KeyColAmount", 4);
  m_params.insert("LastKeyColWidth", mm2px(7));
  m_params.insert("DialogColWidth", mm2px(10));
  m_params.insert("CellsColWidth", mm2px(10));
  m_params.insert("CellsColAmount", 5);
  m_params.insert("CameraColWidth", mm2px(10));
  m_params.insert("CameraColAmount", 3);
  m_params.insert("RowHeight", mm2px(102.0 / 24.0));
  m_params.insert("1SecHeight", mm2px(103.0));
  m_params.insert("FrameLength", 144);
  m_params.insert("InfoOriginLeft", mm2px(84));
  m_params.insert("InfoOriginTop", mm2px(0));
  m_params.insert("InfoTitleHeight", mm2px(4.5));
  m_params.insert("InfoBodyHeight", mm2px(8.5));
  m_params.insert("MemoLinesAmount", 3);
  m_params.insert("ExtraCellsColAmount", 3);

  addInfo(mm2px(26), QObject::tr("EPISODE", "XSheetPDF"));
  addInfo(mm2px(18.5), QObject::tr("SEQ.", "XSheetPDF"));
  addInfo(mm2px(18.5), QObject::tr("SCENE", "XSheetPDF"), decoSceneInfo);
  addInfo(mm2px(40), QObject::tr("TIME", "XSheetPDF"), decoTimeInfo);
  addInfo(mm2px(26), QObject::tr("NAME", "XSheetPDF"));
  addInfo(mm2px(26), QObject::tr("SHEET", "XSheetPDF"), decoSheetInfo);

  m_p.keyBlockWidth =
      param(KeyColWidth) * param(KeyColAmount) + param(LastKeyColWidth);
  m_p.cellsBlockWidth         = param(CellsColWidth) * param(CellsColAmount);
  m_p.cameraBlockWidth        = param(CameraColWidth) * param(CameraColAmount);
  m_p.infoHeaderHeight        = param(InfoTitleHeight) + param(InfoBodyHeight);
  m_p.bodylabelTextSize_Large = param(HeaderHeight) - mm2px(3);
  m_p.bodylabelTextSize_Small = param(HeaderHeight) / 2 - mm2px(1);

  // data rects
  int infoBottom =
      param(InfoOriginTop) + param(InfoTitleHeight) + param(InfoBodyHeight);
  m_dataRects[Data_Memo] = QRect(mm2px(0), infoBottom + mm2px(1), mm2px(239),
                                 param(BodyTop) - infoBottom - mm2px(3));
  m_dataRects[Data_DateTimeAndScenePath] = m_dataRects[Data_Memo];
  m_dataRects[Data_Logo] =
      QRect(mm2px(0), mm2px(0), param(InfoOriginLeft), infoBottom);
}

//---------------------------------------------------------
// read from the settings file
XSheetPDFTemplate_Custom::XSheetPDFTemplate_Custom(
    const QString& fp, const QList<QPair<TXshLevelColumn*, QString>>& columns,
    const int duration)
    : XSheetPDFTemplate(columns, duration), m_valid(false) {
  QSettings s(fp, QSettings::IniFormat);
  if (!s.childGroups().contains("XSheetPDFTemplate")) return;

  s.beginGroup("XSheetPDFTemplate");
  {
    QString pageStr      = s.value("PageSize").toString();
    m_p.documentPageSize = str2PageSizeId(pageStr);

    QString marginStr = s.value("Margin").toString();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList m = marginStr.split(QLatin1Char(','), Qt::SkipEmptyParts);
#else
    QStringList m = marginStr.split(QLatin1Char(','), QString::SkipEmptyParts);
#endif
    assert(m.size() == 4);
    if (m.size() == 4)
      m_p.documentMargin = QMarginsF(m[0].toDouble(), m[1].toDouble(),
                                     m[2].toDouble(), m[3].toDouble());

    s.beginGroup("Number");
    {
      for (auto key : s.childKeys())
        m_params.insert(key.toStdString(), s.value(key).toInt());
    }
    s.endGroup();

    s.beginGroup("Length");
    {
      for (auto key : s.childKeys())
        m_params.insert(key.toStdString(), mm2px(s.value(key).toDouble()));
    }
    s.endGroup();

    bool translateInfo = param(TranslateInfoLabel, 1) == 1;
    int size           = s.beginReadArray("InfoFormats");
    for (int i = 0; i < size; ++i) {
      s.setArrayIndex(i);
      XSheetPDF_InfoFormat infoFormat;
      infoFormat.width = mm2px(s.value("width").toDouble());
      if (translateInfo)
        infoFormat.label =
            QObject::tr(s.value("label").toString().toLocal8Bit(), "XSheetPDF");
      else
        infoFormat.label = s.value("label").toString();

      infoFormat.decoFunc =
          infoType2DecoFunc(s.value("infoType", "").toString());
      m_p.array_Infos.append(infoFormat);
    }
    s.endArray();

    s.beginGroup("DataFields");
    {
      for (auto key : s.childKeys()) {
        QString rectStr = s.value(key).toString();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList r = rectStr.split(QLatin1Char(','), Qt::SkipEmptyParts);
#else
        QStringList r =
            rectStr.split(QLatin1Char(','), QString::SkipEmptyParts);
#endif
        assert(r.size() == 4);
        if (r.size() == 4)
          m_dataRects[dataStr2Type(key)] =
              QRect(mm2px(r[0].toDouble()), mm2px(r[1].toDouble()),
                    mm2px(r[2].toDouble()), mm2px(r[3].toDouble()));
      }
    }
    s.endGroup();
  }
  s.endGroup();

  m_p.keyBlockWidth =
      param(KeyColWidth) * param(KeyColAmount) + param(LastKeyColWidth);
  m_p.cellsBlockWidth         = param(CellsColWidth) * param(CellsColAmount);
  m_p.cameraBlockWidth        = param(CameraColWidth) * param(CameraColAmount);
  m_p.infoHeaderHeight        = param(InfoTitleHeight) + param(InfoBodyHeight);
  m_p.bodylabelTextSize_Large = param(HeaderHeight) - mm2px(3);
  m_p.bodylabelTextSize_Small = param(HeaderHeight) / 2 - mm2px(1);

  m_valid = true;
}

//-----------------------------------------------------------------------------
XsheetPdfPreviewPane::XsheetPdfPreviewPane(QWidget* parent)
    : QWidget(parent), m_scaleFactor(1.0) {}

void XsheetPdfPreviewPane::paintEvent(QPaintEvent* event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  painter.setRenderHint(QPainter::Antialiasing, true);
  QSize pmSize((double)m_pixmap.width() * m_scaleFactor,
               (double)m_pixmap.height() * m_scaleFactor);
  painter.drawPixmap(
      0, 0,
      m_pixmap.scaled(pmSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void XsheetPdfPreviewPane::setPixmap(QPixmap pm) {
  m_pixmap = pm;
  resize(pm.size() * m_scaleFactor);
  update();
}

void XsheetPdfPreviewPane::doZoom(double d_scale) {
  m_scaleFactor += d_scale;
  if (m_scaleFactor > 1.0)
    m_scaleFactor = 1.0;
  else if (m_scaleFactor < 0.1)
    m_scaleFactor = 0.1;

  resize(m_pixmap.size() * m_scaleFactor);
  update();
}

void XsheetPdfPreviewPane::fitScaleTo(QSize size) {
  double tmp_scaleFactor =
      std::min((double)size.width() / (double)m_pixmap.width(),
               (double)size.height() / (double)m_pixmap.height());

  m_scaleFactor = tmp_scaleFactor;
  if (m_scaleFactor > 1.0)
    m_scaleFactor = 1.0;
  else if (m_scaleFactor < 0.1)
    m_scaleFactor = 0.1;

  resize(m_pixmap.size() * m_scaleFactor);
  update();
}

//-----------------------------------------------------------------------------
void XsheetPdfPreviewArea::mousePressEvent(QMouseEvent* e) {
  m_mousePos = e->pos();
}

void XsheetPdfPreviewArea::mouseMoveEvent(QMouseEvent* e) {
  QPoint d = m_mousePos - e->pos();
  horizontalScrollBar()->setValue(horizontalScrollBar()->value() + d.x());
  verticalScrollBar()->setValue(verticalScrollBar()->value() + d.y());
  m_mousePos = e->pos();
}

void XsheetPdfPreviewArea::contextMenuEvent(QContextMenuEvent* event) {
  QMenu* menu        = new QMenu(this);
  QAction* fitAction = menu->addAction(tr("Fit To Window"));
  connect(fitAction, SIGNAL(triggered()), this, SLOT(fitToWindow()));
  menu->exec(event->globalPos());
}

void XsheetPdfPreviewArea::fitToWindow() {
  dynamic_cast<XsheetPdfPreviewPane*>(widget())->fitScaleTo(rect().size());
}

void XsheetPdfPreviewArea::wheelEvent(QWheelEvent* event) {
  int delta = 0;
  switch (event->source()) {
  case Qt::MouseEventNotSynthesized: {
    delta = event->angleDelta().y();
  }
  case Qt::MouseEventSynthesizedBySystem: {
    QPoint numPixels  = event->pixelDelta();
    QPoint numDegrees = event->angleDelta() / 8;
    if (!numPixels.isNull()) {
      delta = event->pixelDelta().y();
    } else if (!numDegrees.isNull()) {
      QPoint numSteps = numDegrees / 15;
      delta           = numSteps.y();
    }
    break;
  }

  default:  // Qt::MouseEventSynthesizedByQt,
            // Qt::MouseEventSynthesizedByApplication
  {
    std::cout << "not supported event: Qt::MouseEventSynthesizedByQt, "
                 "Qt::MouseEventSynthesizedByApplication"
              << std::endl;
    break;
  }

  }  // end switch

  if (delta == 0) {
    event->accept();
    return;
  }

  dynamic_cast<XsheetPdfPreviewPane*>(widget())->doZoom((delta > 0) ? 0.1
                                                                    : -0.1);

  event->accept();
}

//-----------------------------------------------------------------------------

ExportXsheetPdfPopup::ExportXsheetPdfPopup()
    : DVGui::Dialog(TApp::instance()->getMainWindow(), false, false,
                    "ExportXsheetPdf")
    , m_currentTmpl(nullptr) {
  setWindowTitle(tr("Export Xsheet PDF"));

  m_previewPane         = new XsheetPdfPreviewPane(this);
  m_pathFld             = new DVGui::FileField();
  m_fileNameFld         = new QLineEdit(this);
  m_templateCombo       = new QComboBox(this);
  m_exportAreaCombo     = new QComboBox(this);
  m_continuousLineCombo = new QComboBox(this);

  m_pageInfoLbl  = new QLabel(this);
  m_lineColorFld = new DVGui::ColorField(this, false, TPixel32(128, 128, 128));
  m_templateFontCB = new QFontComboBox(this);
  m_contentsFontCB = new QFontComboBox(this);
  m_addDateTimeCB  = new QCheckBox(tr("Print Export DateTime"), this);
  m_addScenePathCB = new QCheckBox(tr("Print Scene Path"), this);
  m_drawSoundCB    = new QCheckBox(tr("Print Soundtrack"), this);
  m_addSceneNameCB = new QCheckBox(tr("Print Scene Name"), this);
  m_serialFrameNumberCB =
      new QCheckBox(tr("Put Serial Frame Numbers Over Pages"), this);
  m_levelNameOnBottomCB =
      new QCheckBox(tr("Print Level Names On The Bottom"), this);
  m_drawDialogueCB   = new QCheckBox(tr("Print Dialogue"), this);
  m_dialogueColCombo = new QComboBox();
  m_sceneNameEdit    = new QLineEdit(this);
  m_memoEdit         = new QTextEdit(this);

  m_logoTxtRB        = new QRadioButton(tr("Text"), this);
  m_logoImgRB        = new QRadioButton(tr("Image"), this);
  m_logoTextEdit     = new QLineEdit(this);
  m_logoImgPathField = new DVGui::FileField(this);

  m_previewArea     = new XsheetPdfPreviewArea(this);
  m_currentPageEdit = new QLineEdit(this);
  m_totalPageCount  = 1;
  m_prev            = new QPushButton(tr("< Prev"), this);
  m_next            = new QPushButton(tr("Next >"), this);

  QPushButton* exportBtn    = new QPushButton(tr("Export PDF"), this);
  QPushButton* exportPngBtn = new QPushButton(tr("Export PNG"), this);
  QPushButton* cancelBtn    = new QPushButton(tr("Cancel"), this);

  m_tick1IdCombo = new QComboBox(this);
  m_tick2IdCombo = new QComboBox(this);
  m_keyIdCombo   = new QComboBox(this);
  refreshCellMarkComboItems(m_tick1IdCombo);
  refreshCellMarkComboItems(m_tick2IdCombo);
  refreshCellMarkComboItems(m_keyIdCombo);
  m_tick1MarkCombo = createTickMarkCombo(this);
  m_tick2MarkCombo = createTickMarkCombo(this);

  //------
  QStringList pdfFileTypes = {"pdf"};
  m_pathFld->setFilters(pdfFileTypes);
  m_pathFld->setFileMode(QFileDialog::DirectoryOnly);
  m_fileNameFld->setFixedWidth(100);
  m_previewArea->setWidget(m_previewPane);
  m_previewArea->setAlignment(Qt::AlignCenter);
  m_previewArea->setBackgroundRole(QPalette::Dark);
  m_previewArea->setStyleSheet("background-color:black;");
  m_exportAreaCombo->addItem(tr("ACTIONS"), Area_Actions);
  m_exportAreaCombo->addItem(tr("CELLS"), Area_Cells);
  // m_exportAreaCombo->setCurrentIndex(1);
  m_memoEdit->setAcceptRichText(false);
  m_memoEdit->setStyleSheet(
      "background:white;\ncolor:black;\nborder:1 solid black;");
  m_memoEdit->setFixedHeight(150);
  m_dialogueColCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  m_sceneNameEdit->setFixedWidth(100);

  m_continuousLineCombo->addItem(tr("Always"), Line_Always);
  m_continuousLineCombo->addItem(tr("More Than 3 Continuous Cells"),
                                 Line_MoreThan3s);
  m_continuousLineCombo->addItem(tr("None"), Line_None);

  QStringList filters;
  for (QByteArray& format : QImageReader::supportedImageFormats())
    filters += format;
  m_logoImgPathField->setFilters(filters);
  m_logoImgPathField->setFileMode(QFileDialog::ExistingFile);

  m_currentPageEdit->setValidator(new QIntValidator(1, m_totalPageCount, this));
  m_currentPageEdit->setText("1");
  m_currentPageEdit->setObjectName("LargeSizedText");
  m_currentPageEdit->setFixedWidth(100);
  m_prev->setDisabled(true);
  m_next->setDisabled(true);
  exportBtn->setObjectName("LargeSizedText");

  QHBoxLayout* mainLay = new QHBoxLayout();
  mainLay->setMargin(0);
  mainLay->setSpacing(5);
  {
    QVBoxLayout* previewLay = new QVBoxLayout();
    previewLay->setMargin(0);
    previewLay->setSpacing(0);
    {
      previewLay->addWidget(m_previewArea, 1);
      QHBoxLayout* prevBtnLay = new QHBoxLayout();
      prevBtnLay->setMargin(15);
      prevBtnLay->setSpacing(10);
      {
        prevBtnLay->addStretch(1);
        prevBtnLay->addWidget(m_prev, 0);
        prevBtnLay->addWidget(m_currentPageEdit, 0);
        prevBtnLay->addWidget(m_next, 0);
        prevBtnLay->addStretch(1);
      }
      previewLay->addLayout(prevBtnLay, 0);
    }
    mainLay->addLayout(previewLay, 1);

    QVBoxLayout* rightLay = new QVBoxLayout();
    rightLay->setMargin(0);
    rightLay->setSpacing(10);
    {
      QScrollArea* scrollArea = new QScrollArea(this);
      scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      QWidget* scrollPanel    = new QWidget(this);
      QVBoxLayout* controlLay = new QVBoxLayout();
      controlLay->setMargin(20);
      controlLay->setSpacing(10);
      {
        QGroupBox* tmplGBox = new QGroupBox(tr("Template Settings"), this);

        QGridLayout* tmplLay = new QGridLayout();
        tmplLay->setMargin(10);
        tmplLay->setHorizontalSpacing(5);
        tmplLay->setVerticalSpacing(10);
        {
          tmplLay->addWidget(new QLabel(tr("Template:"), this), 0, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
          tmplLay->addWidget(m_templateCombo, 0, 1, 1, 2,
                             Qt::AlignLeft | Qt::AlignVCenter);

          tmplLay->addWidget(new QLabel(tr("Line color:"), this), 1, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
          tmplLay->addWidget(m_lineColorFld, 1, 1, 1, 2);

          tmplLay->addWidget(new QLabel(tr("Template font:"), this), 2, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
          tmplLay->addWidget(m_templateFontCB, 2, 1, 1, 2,
                             Qt::AlignLeft | Qt::AlignVCenter);

          tmplLay->addWidget(new QLabel(tr("Logo:"), this), 3, 0,
                             Qt::AlignRight | Qt::AlignTop);
          tmplLay->addWidget(m_logoTxtRB, 3, 1);
          tmplLay->addWidget(m_logoTextEdit, 3, 2);
          tmplLay->addWidget(m_logoImgRB, 4, 1);
          tmplLay->addWidget(m_logoImgPathField, 4, 2);

          tmplLay->addWidget(m_serialFrameNumberCB, 5, 0, 1, 3);
        }
        tmplLay->setColumnStretch(2, 1);
        tmplGBox->setLayout(tmplLay);
        controlLay->addWidget(tmplGBox, 0);

        QGroupBox* exportGBox = new QGroupBox(tr("Export Settings"), this);

        QGridLayout* exportLay = new QGridLayout();
        exportLay->setMargin(10);
        exportLay->setHorizontalSpacing(5);
        exportLay->setVerticalSpacing(10);
        {
          exportLay->addWidget(new QLabel(tr("Output area:"), this), 0, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
          exportLay->addWidget(m_exportAreaCombo, 0, 1);
          exportLay->addWidget(m_pageInfoLbl, 0, 2);

          exportLay->addWidget(new QLabel(tr("Output font:"), this), 1, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
          exportLay->addWidget(m_contentsFontCB, 1, 1, 1, 2,
                               Qt::AlignLeft | Qt::AlignVCenter);

          exportLay->addWidget(new QLabel(tr("Continuous line:"), this), 2, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
          exportLay->addWidget(m_continuousLineCombo, 2, 1, 1, 2,
                               Qt::AlignLeft | Qt::AlignVCenter);

          QGridLayout* checksLay = new QGridLayout();
          checksLay->setMargin(0);
          checksLay->setHorizontalSpacing(10);
          checksLay->setVerticalSpacing(10);
          {
            checksLay->addWidget(m_addDateTimeCB, 0, 0);
            checksLay->addWidget(m_addScenePathCB, 0, 1, 1, 2);
            checksLay->addWidget(m_drawSoundCB, 1, 0);
            checksLay->addWidget(m_addSceneNameCB, 1, 1);
            checksLay->addWidget(m_sceneNameEdit, 1, 2,
                                 Qt::AlignLeft | Qt::AlignVCenter);
            checksLay->addWidget(m_levelNameOnBottomCB, 2, 0);
            checksLay->addWidget(m_drawDialogueCB, 2, 1);
            checksLay->addWidget(m_dialogueColCombo, 2, 2,
                                 Qt::AlignLeft | Qt::AlignVCenter);
          }
          checksLay->setColumnStretch(0, 2);
          checksLay->setColumnStretch(1, 1);
          checksLay->setColumnStretch(2, 1);
          exportLay->addLayout(checksLay, 3, 0, 1, 3);

          exportLay->addWidget(new QLabel(tr("Inbetween mark 1:"), this), 4, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
          exportLay->addWidget(m_tick1IdCombo, 4, 1);
          exportLay->addWidget(m_tick1MarkCombo, 4, 2,
                               Qt::AlignLeft | Qt::AlignVCenter);
          exportLay->addWidget(new QLabel(tr("Inbetween mark 2:"), this), 5, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
          exportLay->addWidget(m_tick2IdCombo, 5, 1);
          exportLay->addWidget(m_tick2MarkCombo, 5, 2,
                               Qt::AlignLeft | Qt::AlignVCenter);
          exportLay->addWidget(new QLabel(tr("Keyframe mark:"), this), 6, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
          exportLay->addWidget(m_keyIdCombo, 6, 1);

          exportLay->addWidget(new QLabel(tr("Memo:"), this), 7, 0,
                               Qt::AlignRight | Qt::AlignTop);
          exportLay->addWidget(m_memoEdit, 7, 1, 1, 2);
        }
        exportLay->setColumnStretch(2, 1);
        exportGBox->setLayout(exportLay);
        controlLay->addWidget(exportGBox, 0);

        controlLay->addStretch(1);
      }
      scrollPanel->setLayout(controlLay);
      scrollArea->setWidget(scrollPanel);
      rightLay->addWidget(scrollArea, 1);

      QGridLayout* saveLay = new QGridLayout();
      saveLay->setMargin(15);
      saveLay->setHorizontalSpacing(5);
      saveLay->setVerticalSpacing(10);
      {
        saveLay->addWidget(new QLabel(tr("Save in:"), this), 0, 0,
                           Qt::AlignRight | Qt::AlignVCenter);
        saveLay->addWidget(m_pathFld, 0, 1);

        saveLay->addWidget(new QLabel(tr("Name:"), this), 1, 0,
                           Qt::AlignRight | Qt::AlignVCenter);
        saveLay->addWidget(m_fileNameFld, 1, 1,
                           Qt::AlignLeft | Qt::AlignVCenter);
      }
      rightLay->addLayout(saveLay, 0);

      QHBoxLayout* btnLay = new QHBoxLayout();
      btnLay->setMargin(10);
      btnLay->setSpacing(10);
      {
        btnLay->addStretch(1);
        btnLay->addWidget(exportBtn, 0);
        btnLay->addWidget(exportPngBtn, 0);
        btnLay->addWidget(cancelBtn, 0);
      }
      rightLay->addLayout(btnLay, 0);
    }
    mainLay->addLayout(rightLay, 0);
  }
  m_topLayout->addLayout(mainLay, 1);

  loadPresetItems();
  loadSettings();

  connect(exportBtn, SIGNAL(clicked()), this, SLOT(onExport()));
  connect(exportPngBtn, SIGNAL(clicked()), this, SLOT(onExportPNG()));
  connect(cancelBtn, SIGNAL(clicked()), this, SLOT(close()));

  connect(m_templateCombo, SIGNAL(activated(int)), this, SLOT(initTemplate()));

  connect(m_exportAreaCombo, SIGNAL(activated(int)), this,
          SLOT(updatePreview()));
  connect(m_continuousLineCombo, SIGNAL(activated(int)), this,
          SLOT(updatePreview()));
  connect(m_lineColorFld, SIGNAL(colorChanged(const TPixel32&, bool)), this,
          SLOT(updatePreview()));
  connect(m_templateFontCB, SIGNAL(currentFontChanged(const QFont&)), this,
          SLOT(updatePreview()));
  connect(m_contentsFontCB, SIGNAL(currentFontChanged(const QFont&)), this,
          SLOT(updatePreview()));
  connect(m_addDateTimeCB, SIGNAL(clicked(bool)), this, SLOT(updatePreview()));
  connect(m_addScenePathCB, SIGNAL(clicked(bool)), this, SLOT(updatePreview()));
  connect(m_drawSoundCB, SIGNAL(clicked(bool)), this, SLOT(updatePreview()));
  connect(m_serialFrameNumberCB, SIGNAL(clicked(bool)), this,
          SLOT(updatePreview()));
  connect(m_levelNameOnBottomCB, SIGNAL(clicked(bool)), this,
          SLOT(updatePreview()));
  connect(m_drawDialogueCB, SIGNAL(clicked(bool)), this, SLOT(updatePreview()));
  connect(m_drawDialogueCB, SIGNAL(clicked(bool)), m_dialogueColCombo,
          SLOT(setEnabled(bool)));
  connect(m_dialogueColCombo, SIGNAL(activated(int)), this,
          SLOT(updatePreview()));
  connect(m_addSceneNameCB, SIGNAL(clicked(bool)), this, SLOT(updatePreview()));
  connect(m_addSceneNameCB, SIGNAL(clicked(bool)), m_sceneNameEdit,
          SLOT(setEnabled(bool)));
  connect(m_sceneNameEdit, SIGNAL(editingFinished()), this,
          SLOT(updatePreview()));
  connect(m_memoEdit, SIGNAL(textChanged()), this, SLOT(updatePreview()));

  connect(m_logoTxtRB, SIGNAL(toggled(bool)), this, SLOT(onLogoModeToggled()));
  connect(m_logoImgRB, SIGNAL(toggled(bool)), this, SLOT(onLogoModeToggled()));
  connect(m_logoTextEdit, SIGNAL(editingFinished()), this,
          SLOT(updatePreview()));
  connect(m_logoImgPathField, SIGNAL(pathChanged()), this,
          SLOT(onLogoImgPathChanged()));
  connect(m_currentPageEdit, SIGNAL(editingFinished()), this,
          SLOT(updatePreview()));
  connect(m_prev, SIGNAL(clicked(bool)), this, SLOT(onPrev()));
  connect(m_next, SIGNAL(clicked(bool)), this, SLOT(onNext()));

  connect(m_tick1IdCombo, SIGNAL(activated(int)), this,
          SLOT(onTickIdComboActivated()));
  connect(m_tick2IdCombo, SIGNAL(activated(int)), this,
          SLOT(onTickIdComboActivated()));
  connect(m_keyIdCombo, SIGNAL(activated(int)), this, SLOT(updatePreview()));
  connect(m_tick1MarkCombo, SIGNAL(activated(int)), this,
          SLOT(updatePreview()));
  connect(m_tick2MarkCombo, SIGNAL(activated(int)), this,
          SLOT(updatePreview()));

  // The following lines are "translation word book" listing the words which may
  // appear in the template

  // info item labels
  QObject::tr("EPISODE", "XSheetPDF");
  QObject::tr("SEQ.", "XSheetPDF");
  QObject::tr("SCENE", "XSheetPDF");
  QObject::tr("TIME", "XSheetPDF");
  QObject::tr("NAME", "XSheetPDF");
  QObject::tr("SHEET", "XSheetPDF");
  QObject::tr("TITLE", "XSheetPDF");
  QObject::tr("CAMERAMAN", "XSheetPDF");
  // template name
  tr("B4 size, 3 seconds sheet");
  tr("B4 size, 6 seconds sheet");
  tr("A3 size, 3 seconds sheet");
  tr("A3 size, 6 seconds sheet");
}

ExportXsheetPdfPopup::~ExportXsheetPdfPopup() {
  if (m_currentTmpl) delete m_currentTmpl;
}

void ExportXsheetPdfPopup::loadPresetItems() {
  // check in the preset folder
  TFilePath presetFolderPath =
      ToonzFolder::getLibraryFolder() + "xsheetpdfpresets";
  QDir presetFolder(presetFolderPath.getQString());
  QStringList filters;
  filters << "*.ini";
  presetFolder.setNameFilters(filters);
  presetFolder.setFilter(QDir::Files);
  TFilePathSet pathSet;
  try {
    TSystem::readDirectory(pathSet, presetFolder, false);
  } catch (...) {
    return;
  }

  for (auto fp : pathSet) {
    QSettings s(fp.getQString(), QSettings::IniFormat);
    if (!(s.childGroups().contains("XSheetPDFTemplate"))) continue;
    s.beginGroup("XSheetPDFTemplate");
    QString labelStr =
        s.value("Label", QString::fromStdString(fp.getName())).toString();
    m_templateCombo->addItem(tr(labelStr.toLocal8Bit()), fp.getQString());
  }

  if (m_templateCombo->count() == 0)
    m_templateCombo->addItem(tr("B4 size, 6 seconds sheet"), "");
}

void ExportXsheetPdfPopup::initialize() {
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();

  // default save destination
  QString initialPath, initialFileName;
  if (scene->isUntitled()) {
    initialPath     = "+scenes";
    initialFileName = "untitled";
  } else {
    initialPath     = scene->getScenePath().getParentDir().getQString();
    initialFileName = QString::fromStdWString(scene->getSceneName());
  }
  m_pathFld->setPath(initialPath);
  m_fileNameFld->setText(initialFileName);

  // gather columns to be exported
  // TODO: consider if the top xsheet should be choosable when the current
  // xsheet is sub xsheet
  TXsheet* xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
  // if the current xsheet is top xsheet in the scene and the output
  // frame range is specified, set the "to" frame value as duration

  TOutputProperties* oprop = scene->getProperties()->getOutputProperties();
  int from, to, step;
  if (scene->getTopXsheet() == xsheet && oprop->getRange(from, to, step))
    m_duration = to + 1;
  else
    m_duration = xsheet->getFrameCount();

  m_columns.clear();
  m_soundColumns.clear();
  m_noteColumns.clear();
  for (int col = 0; col < xsheet->getColumnCount(); col++) {
    if (xsheet->isColumnEmpty(col)) continue;

    TXshSoundColumn* soundColumn = xsheet->getColumn(col)->getSoundColumn();
    if (soundColumn) {
      if (soundColumn->isPreviewVisible()) m_soundColumns.append(soundColumn);
      continue;
    }

    TXshSoundTextColumn* noteColumn =
        xsheet->getColumn(col)->getSoundTextColumn();
    if (noteColumn) {
      m_noteColumns.insert(col, noteColumn);
      continue;
    }

    TXshLevelColumn* column = xsheet->getColumn(col)->getLevelColumn();
    if (!column) continue;
    // do not export if the "eye" (render) button is off
    if (!column->isPreviewVisible()) continue;
    // skip column containing single-framed level
    TFrameId firstFid = column->getCell(column->getFirstRow()).getFrameId();
    if (firstFid.getNumber() == TFrameId::EMPTY_FRAME ||
        firstFid.getNumber() == TFrameId::NO_FRAME)
      continue;

    TStageObject* columnObject =
        xsheet->getStageObject(TStageObjectId::ColumnId(col));
    QString colName = (columnObject->hasSpecifiedName())
                          ? QString::fromStdString(columnObject->getName())
                          : QString();
    m_columns.append(QPair<TXshLevelColumn*, QString>(column, colName));
  }

  m_addScenePathCB->setDisabled(scene->isUntitled());
  m_drawSoundCB->setDisabled(m_soundColumns.isEmpty());
  m_sceneNameEdit->setText(
      (scene->isUntitled()) ? ""
                            : QString::fromStdWString(scene->getSceneName()));
  m_drawDialogueCB->setDisabled(m_noteColumns.isEmpty());
  m_dialogueColCombo->setEnabled(!m_noteColumns.isEmpty() &&
                                 m_drawDialogueCB->isChecked());
  m_dialogueColCombo->clear();
  for (auto colId : m_noteColumns.keys())
    m_dialogueColCombo->addItem(tr("Col%1").arg(colId + 1), colId);

  initTemplate();

  m_previewPane->fitScaleTo(m_previewArea->size());

  refreshCellMarkComboItems(m_tick1IdCombo);
  refreshCellMarkComboItems(m_tick2IdCombo);
  refreshCellMarkComboItems(m_keyIdCombo);
}

// register settings to the user env file on close
void ExportXsheetPdfPopup::saveSettings() {
  XShPdfExportTemplate =
      m_templateCombo->currentData().toString().toStdString();
  XShPdfExportOutputArea     = m_exportAreaCombo->currentData().toInt();
  TPixel32 col               = m_lineColorFld->getColor();
  XShPdfExportLineColor      = QColor(col.r, col.g, col.b).name().toStdString();
  XShPdfExportPrintDateTime  = (m_addDateTimeCB->isChecked()) ? 1 : 0;
  XShPdfExportPrintScenePath = (m_addScenePathCB->isChecked()) ? 1 : 0;
  XShPdfExportPrintSoundtrack   = (m_drawSoundCB->isChecked()) ? 1 : 0;
  XShPdfExportPrintSceneName    = (m_addSceneNameCB->isChecked()) ? 1 : 0;
  XShPdfExportSerialFrameNumber = (m_serialFrameNumberCB->isChecked()) ? 1 : 0;
  XShPdfExportLevelNameOnBottom = (m_levelNameOnBottomCB->isChecked()) ? 1 : 0;
  XShPdfExportPrintDialogue     = (m_drawDialogueCB->isChecked()) ? 1 : 0;
  XShPdfExportTemplateFont =
      m_templateFontCB->currentFont().family().toStdString();
  XShPdfExportOutputFont =
      m_contentsFontCB->currentFont().family().toStdString();
  XShPdfExportLogoPreference = (m_logoTxtRB->isChecked()) ? 0 : 1;
  XShPdfExportLogoText       = m_logoTextEdit->text().toStdString();
  XShPdfExportImgPath        = m_logoImgPathField->getPath().toStdString();

  ContinuousLineMode clMode =
      (ContinuousLineMode)(m_continuousLineCombo->currentData().toInt());
  XShPdfExportContinuousLineThres = (clMode == Line_Always) ? 0
                                    : (clMode == Line_None) ? -1
                                                            : 3;

  XShPdfExportTick1Id   = m_tick1IdCombo->currentData().toInt();
  XShPdfExportTick2Id   = m_tick2IdCombo->currentData().toInt();
  XShPdfExportKeyId     = m_keyIdCombo->currentData().toInt();
  XShPdfExportTick1Type = m_tick1MarkCombo->currentData().toInt();
  XShPdfExportTick2Type = m_tick2MarkCombo->currentData().toInt();
}

// load settings from the user env file on ctor
void ExportXsheetPdfPopup::loadSettings() {
  int tmplId =
      m_templateCombo->findData(QString::fromStdString(XShPdfExportTemplate));
  m_templateCombo->setCurrentIndex((tmplId >= 0) ? tmplId : 0);
  int areaId = XShPdfExportOutputArea;
  m_exportAreaCombo->setCurrentIndex(m_exportAreaCombo->findData(areaId));
  QColor lineColor(QString::fromStdString(XShPdfExportLineColor));
  m_lineColorFld->setColor(
      TPixel32(lineColor.red(), lineColor.green(), lineColor.blue()));
  m_addDateTimeCB->setChecked(XShPdfExportPrintDateTime != 0);
  m_addScenePathCB->setChecked(XShPdfExportPrintScenePath != 0);
  m_drawSoundCB->setChecked(XShPdfExportPrintSoundtrack != 0);
  m_addSceneNameCB->setChecked(XShPdfExportPrintSceneName != 0);
  m_serialFrameNumberCB->setChecked(XShPdfExportSerialFrameNumber != 0);
  m_levelNameOnBottomCB->setChecked(XShPdfExportLevelNameOnBottom != 0);
  m_drawDialogueCB->setChecked(XShPdfExportPrintDialogue != 0);

  QString tmplFont = QString::fromStdString(XShPdfExportTemplateFont);
  if (!tmplFont.isEmpty()) m_templateFontCB->setCurrentFont(QFont(tmplFont));
  QString outFont = QString::fromStdString(XShPdfExportOutputFont);
  if (!outFont.isEmpty()) m_contentsFontCB->setCurrentFont(QFont(outFont));

  m_logoTxtRB->setChecked(XShPdfExportLogoPreference == 0);
  m_logoImgRB->setChecked(XShPdfExportLogoPreference == 1);
  m_logoTextEdit->setText(QString::fromStdString(XShPdfExportLogoText));
  m_logoImgPathField->setPath(QString::fromStdString(XShPdfExportImgPath));

  ContinuousLineMode clMode =
      (XShPdfExportContinuousLineThres == 0)    ? Line_Always
      : (XShPdfExportContinuousLineThres == -1) ? Line_None
                                                : Line_MoreThan3s;
  m_continuousLineCombo->setCurrentIndex(
      m_continuousLineCombo->findData(clMode));

  m_logoTextEdit->setEnabled(m_logoTxtRB->isChecked());
  m_logoImgPathField->setEnabled(m_logoImgRB->isChecked());
  m_sceneNameEdit->setEnabled(m_addSceneNameCB->isChecked());
  m_dialogueColCombo->setEnabled(m_drawDialogueCB->isChecked() &&
                                 m_drawDialogueCB->isEnabled());

  int id = XShPdfExportTick1Id;
  m_tick1IdCombo->setCurrentIndex(m_tick1IdCombo->findData(id));
  m_tick1MarkCombo->setEnabled(id != -1);
  id = XShPdfExportTick2Id;
  m_tick2IdCombo->setCurrentIndex(m_tick2IdCombo->findData(id));
  m_tick2MarkCombo->setEnabled(id != -1);
  id = XShPdfExportKeyId;
  m_keyIdCombo->setCurrentIndex(m_keyIdCombo->findData(id));
  int type = XShPdfExportTick1Type;
  m_tick1MarkCombo->setCurrentIndex(m_tick1MarkCombo->findData(type));
  type = XShPdfExportTick2Type;
  m_tick2MarkCombo->setCurrentIndex(m_tick2MarkCombo->findData(type));
}

void ExportXsheetPdfPopup::initTemplate() {
  if (m_currentTmpl) {
    delete m_currentTmpl;
    m_currentTmpl = nullptr;
  }

  QString tmplPath = m_templateCombo->currentData().toString();
  if (tmplPath.isEmpty()) {
    m_currentTmpl = new XSheetPDFTemplate_B4_6sec(m_columns, m_duration);
  } else {
    XSheetPDFTemplate_Custom* ret =
        new XSheetPDFTemplate_Custom(tmplPath, m_columns, m_duration);

    if (ret->isValid())
      m_currentTmpl = ret;
    else {
      DVGui::MsgBoxInPopup(
          DVGui::CRITICAL,
          tr("The preset file %1 is not valid.").arg(tmplPath));
      delete ret;
      m_currentTmpl = new XSheetPDFTemplate_B4_6sec(m_columns, m_duration);
    }
  }

  if (!m_soundColumns.isEmpty()) m_currentTmpl->setSoundColumns(m_soundColumns);

  updatePreview();
}

void ExportXsheetPdfPopup::setInfo() {
  XSheetPDFFormatInfo info;

  TPixel32 col   = m_lineColorFld->getColor();
  info.lineColor = QColor(col.r, col.g, col.b);
  info.dateTimeText =
      (m_addDateTimeCB->isChecked())
          ? QDateTime::currentDateTime().toString(Qt::DefaultLocaleLongDate)
          : "";
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  info.scenePathText =
      (m_addScenePathCB->isEnabled() && m_addScenePathCB->isChecked())
          ? scene->getScenePath().getQString()
          : "";
  info.sceneNameText =
      (m_addSceneNameCB->isChecked()) ? m_sceneNameEdit->text() : "";

  info.exportArea = (ExportArea)(m_exportAreaCombo->currentData().toInt());
  info.continuousLineMode =
      (ContinuousLineMode)(m_continuousLineCombo->currentData().toInt());
  info.templateFontFamily = m_templateFontCB->currentFont().family();
  info.contentsFontFamily = m_contentsFontCB->currentFont().family();
  info.memoText           = m_memoEdit->toPlainText();

  info.logoText  = (m_logoTxtRB->isChecked()) ? m_logoTextEdit->text() : "";
  info.drawSound = m_drawSoundCB->isEnabled() && m_drawSoundCB->isChecked();
  info.serialFrameNumber     = m_serialFrameNumberCB->isChecked();
  info.drawLevelNameOnBottom = m_levelNameOnBottomCB->isChecked();

  info.tick1MarkId   = m_tick1IdCombo->currentData().toInt();
  info.tick2MarkId   = m_tick2IdCombo->currentData().toInt();
  info.keyMarkId     = m_keyIdCombo->currentData().toInt();
  info.tick1MarkType = (TickMarkType)(m_tick1MarkCombo->currentData().toInt());
  info.tick2MarkType = (TickMarkType)(m_tick2MarkCombo->currentData().toInt());

  m_currentTmpl->setInfo(info);

  if (m_drawDialogueCB->isChecked() && m_drawDialogueCB->isEnabled())
    m_currentTmpl->setNoteColumn(m_noteColumns.value(
        m_dialogueColCombo->currentData().toInt(), nullptr));
  else
    m_currentTmpl->setNoteColumn(nullptr);

  if (!m_logoImgRB->isChecked()) return;

  // prepare logo image
  QSize logoPixSize = m_currentTmpl->logoPixelSize();
  if (logoPixSize.isEmpty()) return;

  TFilePath decodedPath =
      scene->decodeFilePath(TFilePath(m_logoImgPathField->getPath()));
  QImage img(decodedPath.getQString());
  img = img.scaled(logoPixSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  QImage alphaImg = img;
  alphaImg.invertPixels();
  img.setAlphaChannel(alphaImg);
  QImage logoImg(img.size(), QImage::Format_ARGB32_Premultiplied);
  logoImg.fill(info.lineColor);
  QPainter p(&logoImg);
  p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
  p.drawImage(QPoint(0, 0), img);
  p.end();

  m_currentTmpl->setLogoPixmap(QPixmap::fromImage(logoImg));
}

void ExportXsheetPdfPopup::updatePreview() {
  setInfo();

  int framePageCount    = m_currentTmpl->framePageCount();
  int parallelPageCount = m_currentTmpl->parallelPageCount();
  int totalPage         = framePageCount * parallelPageCount;

  int currentPage = m_currentPageEdit->text().toInt();
  // if the page count is changed
  if (m_totalPageCount != totalPage) {
    m_currentPageEdit->setValidator(new QIntValidator(1, totalPage, this));
    if (m_totalPageCount > totalPage) {
      m_currentPageEdit->setText(QString::number(totalPage));
      currentPage = totalPage;
    }
    m_totalPageCount = totalPage;

    m_prev->setDisabled(currentPage == 1);
    m_next->setDisabled(currentPage == totalPage);
  }
  m_serialFrameNumberCB->setDisabled(framePageCount == 1);
  m_levelNameOnBottomCB->setEnabled(m_duration > 36);

  int fPage = (currentPage - 1) / parallelPageCount;
  int pPage = (currentPage - 1) % parallelPageCount;

  QPixmap pm = m_currentTmpl->initializePreview();
  QPainter painter(&pm);
  m_currentTmpl->drawXsheetTemplate(painter, fPage, true);
  m_currentTmpl->drawXsheetContents(painter, fPage, pPage, true);
  m_previewPane->setPixmap(pm);

  QColor infoColor;
  if (parallelPageCount == 1) {
    m_pageInfoLbl->setText(tr("%n page(s)", "", framePageCount));
    infoColor = QColor(Qt::green);
  } else {
    m_pageInfoLbl->setText(
        tr("%1 x %2 pages").arg(framePageCount).arg(parallelPageCount));
    infoColor = QColor(Qt::yellow);
  }
  m_pageInfoLbl->setStyleSheet(QString("QLabel{color: %1;}\
                                          QLabel QWidget{ color: black;}")
                                   .arg(infoColor.name()));
}

void ExportXsheetPdfPopup::onExport() {
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();

  if (m_fileNameFld->text().isEmpty()) {
    DVGui::MsgBoxInPopup(DVGui::WARNING, tr("Please specify the file name."));
    return;
  }

  TFilePath fp(m_pathFld->getPath());
  fp += m_fileNameFld->text().toStdString() + ".pdf";
  fp = scene->decodeFilePath(fp);

  if (TSystem::doesExistFileOrLevel(fp)) {
    QString question =
        tr("The file %1 already exists.\nDo you want to overwrite it?")
            .arg(fp.getQString());
    int ret = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                            QObject::tr("Cancel"));
    if (ret == 0 || ret == 2) {
      return;
    }
  }
  if (!TFileStatus(fp.getParentDir()).doesExist()) {
    QString question =
        tr("A folder %1 does not exist.\nDo you want to create it?")
            .arg(fp.getParentDir().getQString());
    int ret = DVGui::MsgBox(question, QObject::tr("Create folder"),
                            QObject::tr("Cancel"));
    if (ret == 0 || ret == 2) {
      return;
    }

    if (!TSystem::touchParentDir(fp)) {
      DVGui::MsgBoxInPopup(DVGui::CRITICAL,
                           tr("Failed to create folder %1.")
                               .arg(fp.getParentDir().getQString()));
      return;
    }
  }

  QFile pdfFile(fp.getQString());

  pdfFile.open(QIODevice::WriteOnly);
  QPdfWriter writer(&pdfFile);

  initTemplate();
  setInfo();

  m_currentTmpl->initializePage(writer);
  QPainter painter(&writer);

  int framePageCount    = m_currentTmpl->framePageCount();
  int parallelPageCount = m_currentTmpl->parallelPageCount();
  for (int framePage = 0; framePage < framePageCount; framePage++) {
    for (int parallelPage = 0; parallelPage < parallelPageCount;
         parallelPage++) {
      m_currentTmpl->drawXsheetTemplate(painter, framePage);
      m_currentTmpl->drawXsheetContents(painter, framePage, parallelPage);
      if (framePage != framePageCount - 1 ||
          parallelPage != parallelPageCount - 1)
        writer.newPage();
    }
  }
  painter.end();

  pdfFile.close();

  onExportFinished(fp);
}

void ExportXsheetPdfPopup::onExportPNG() {
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();

  if (m_fileNameFld->text().isEmpty()) {
    DVGui::MsgBoxInPopup(DVGui::WARNING, tr("Please specify the file name."));
    return;
  }

  TFilePath fp(m_pathFld->getPath());
  fp += m_fileNameFld->text().toStdString() + "..png";
  fp = scene->decodeFilePath(fp);

  if (TSystem::doesExistFileOrLevel(fp)) {
    QString question =
        tr("The file %1 already exists.\nDo you want to overwrite it?")
            .arg(fp.getQString());
    int ret = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                            QObject::tr("Cancel"));
    if (ret == 0 || ret == 2) {
      return;
    }
  }
  if (!TFileStatus(fp.getParentDir()).doesExist()) {
    QString question =
        tr("A folder %1 does not exist.\nDo you want to create it?")
            .arg(fp.getParentDir().getQString());
    int ret = DVGui::MsgBox(question, QObject::tr("Create folder"),
                            QObject::tr("Cancel"));
    if (ret == 0 || ret == 2) {
      return;
    }

    if (!TSystem::touchParentDir(fp)) {
      DVGui::MsgBoxInPopup(DVGui::CRITICAL,
                           tr("Failed to create folder %1.")
                               .arg(fp.getParentDir().getQString()));
      return;
    }
  }

  initTemplate();
  setInfo();
  QPixmap tmplPm = m_currentTmpl->initializePreview();

  int framePageCount    = m_currentTmpl->framePageCount();
  int parallelPageCount = m_currentTmpl->parallelPageCount();
  for (int framePage = 0; framePage < framePageCount; framePage++) {
    for (int parallelPage = 0; parallelPage < parallelPageCount;
         parallelPage++) {
      QPixmap pm(tmplPm);
      QPainter painter(&pm);
      m_currentTmpl->drawXsheetTemplate(painter, framePage, true);
      m_currentTmpl->drawXsheetContents(painter, framePage, parallelPage, true);
      painter.end();

      if (parallelPageCount == 1) {
        pm.save(fp.withFrame(framePage + 1).getQString());
      } else {
        pm.save(fp.withFrame(framePage + 1, 'a' + parallelPage).getQString());
      }
    }
  }

  onExportFinished(fp);
}

void ExportXsheetPdfPopup::onExportFinished(const TFilePath& fp) {
  close();
  QString str = QObject::tr("The file %1 has been exported successfully.")
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

void ExportXsheetPdfPopup::onLogoModeToggled() {
  m_logoTextEdit->setEnabled(m_logoTxtRB->isChecked());
  m_logoImgPathField->setEnabled(m_logoImgRB->isChecked());
  updatePreview();
}

void ExportXsheetPdfPopup::onLogoImgPathChanged() {
  TFilePath fp(m_logoImgPathField->getPath());
  if (fp.isLevelName()) {
    ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
    TLevelReaderP lr(scene->decodeFilePath(fp));
    TLevelP level;
    if (lr) level = lr->loadInfo();
    if (level.getPointer() && level->getTable()->size() > 0) {
      TFrameId firstFrame = level->begin()->first;
      fp                  = fp.withFrame(firstFrame);
      m_logoImgPathField->setPath(fp.getQString());
    }
  }
  updatePreview();
}

void ExportXsheetPdfPopup::onPrev() {
  int current = m_currentPageEdit->text().toInt();
  assert(current > 1);
  current -= 1;
  m_currentPageEdit->setText(QString::number(current));

  m_prev->setDisabled(current == 1);
  m_next->setDisabled(current == m_totalPageCount);
  updatePreview();
}

void ExportXsheetPdfPopup::onNext() {
  int current = m_currentPageEdit->text().toInt();
  assert(current < m_totalPageCount);
  current += 1;
  m_currentPageEdit->setText(QString::number(current));

  m_prev->setDisabled(current == 1);
  m_next->setDisabled(current == m_totalPageCount);
  updatePreview();
}

void ExportXsheetPdfPopup::onTickIdComboActivated() {
  QComboBox* combo = qobject_cast<QComboBox*>(sender());
  if (combo == m_tick1IdCombo)
    m_tick1MarkCombo->setEnabled(m_tick1IdCombo->currentData().toInt() != -1);
  else if (combo == m_tick2IdCombo)
    m_tick2MarkCombo->setEnabled(m_tick2IdCombo->currentData().toInt() != -1);
  updatePreview();
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<ExportXsheetPdfPopup> openExportXsheetPdfPopup(
    MI_ExportXsheetPDF);