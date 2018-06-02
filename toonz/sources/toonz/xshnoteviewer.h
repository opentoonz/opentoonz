#pragma once

#ifndef XSHNOTEVIEWER_H
#define XSHNOTEVIEWER_H

#include <memory>

#include "toonz/txsheet.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/dvtextedit.h"
#include "toonzqt/colorfield.h"

#include <QFrame>
#include <QScrollArea>

#include "layerheaderpanel.h"

//-----------------------------------------------------------------------------

// forward declaration
class XsheetViewer;
class QTextEdit;
class TColorStyle;
class QToolButton;
class QPushButton;
class QComboBox;
class Orientation;

//-----------------------------------------------------------------------------

namespace XsheetGUI {

//=============================================================================
// NotePopup
//-----------------------------------------------------------------------------

class NotePopup final : public DVGui::Dialog {
  Q_OBJECT
  XsheetViewer *m_viewer;
  int m_noteIndex;
  DVGui::DvTextEdit *m_textEditor;
  int m_currentColorIndex;
  QList<DVGui::ColorField *> m_colorFields;
  //! Used to avoid double click in discard or post button.
  bool m_isOneButtonPressed;

public:
  NotePopup(XsheetViewer *viewer, int noteIndex);
  ~NotePopup() {}

  void setCurrentNoteIndex(int index);

  void update();

protected:
  TXshNoteSet *getNotes();
  QList<TPixel32> getColors();

  void onColorFieldEditingChanged(const TPixel32 &color, bool isEditing,
                                  int index);

  DVGui::ColorField *createColorField(int index);
  void updateColorField();

  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

protected slots:
  void onColor1Switched(const TPixel32 &, bool isDragging);
  void onColor2Switched(const TPixel32 &, bool isDragging);
  void onColor3Switched(const TPixel32 &, bool isDragging);
  void onColor4Switched(const TPixel32 &, bool isDragging);
  void onColor5Switched(const TPixel32 &, bool isDragging);
  void onColor6Switched(const TPixel32 &, bool isDragging);
  void onColor7Switched(const TPixel32 &, bool isDragging);

  void onColorChanged(const TPixel32 &, bool isDragging);
  void onNoteAdded();
  void onNoteDiscarded();
  void onTextEditFocusIn();

  void onXsheetSwitched();
};

//=============================================================================
// NoteWidget
//-----------------------------------------------------------------------------

class NoteWidget final : public QWidget {
  Q_OBJECT
  XsheetViewer *m_viewer;
  int m_noteIndex;
  std::unique_ptr<NotePopup> m_noteEditor;
  bool m_isHovered;

public:
  NoteWidget(XsheetViewer *parent = 0, int noteIndex = -1);

  int getNoteIndex() const { return m_noteIndex; }
  void setNoteIndex(int index) {
    m_noteIndex = index;
    if (m_noteEditor) m_noteEditor->setCurrentNoteIndex(index);
  }

  void paint(QPainter *painter, QPoint pos = QPoint(), bool isCurrent = false);

  void openNotePopup();

protected:
  void paintEvent(QPaintEvent *event) override;
};

//=============================================================================
// NoteArea
//-----------------------------------------------------------------------------

class NoteArea final : public QFrame {
  Q_OBJECT

  std::unique_ptr<NotePopup> m_newNotePopup;  // Popup used to create new note
  XsheetViewer *m_viewer;

  QPushButton *m_flipOrientationButton;

  QToolButton *m_noteButton;
  QToolButton *m_nextNoteButton;
  QToolButton *m_precNoteButton;

  QComboBox *m_frameDisplayStyleCombo;

  LayerHeaderPanel *m_layerHeaderPanel;

public:
#if QT_VERSION >= 0x050500
  NoteArea(XsheetViewer *parent = 0, Qt::WindowFlags flags = 0);
#else
  NoteArea(XsheetViewer *parent = 0, Qt::WFlags flags = 0);
#endif

  void updatePopup() { m_newNotePopup->update(); }
  void updateButtons();

protected slots:
  void flipOrientation();
  void toggleNewNote();
  void nextNote();
  void precNote();

  void onFrameDisplayStyleChanged(int id);
  void onXsheetOrientationChanged(const Orientation *orientation);

protected:
  void removeLayout();
  void createLayout();
};

}  // namespace XsheetGUI;

#endif  // XSHNOTEVIEWER_H
