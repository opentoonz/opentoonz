#pragma once

#ifndef CUSTOMPANELEDITORPOPUP_H
#define CUSTOMPANELEDITORPOPUP_H

#include "toonzqt/dvdialog.h"
// ToonzQt
#include "toonzqt/menubarcommand.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMap>
#include <QPair>
#include <QLabel>

#include <QScrollArea>
#include <QDomElement>
class QComboBox;
class CustomPanelUIField;
class CommandListTree;

enum UiType { Button = 0, Scroller_Back, Scroller_Fore, TypeCount };
struct UiEntry {
  QString objectName;  // object name before editing
  UiType type;
  QRect rect;
  CustomPanelUIField* field;
  Qt::Orientation orientation = Qt::Horizontal;
};

//=============================================================================
// CustomPanelUIField
//-----------------------------------------------------------------------------
class CustomPanelUIField : public QLabel {
  Q_OBJECT
  int m_id;
  QString m_commandId;

public:
  CustomPanelUIField(const int id, const QString objectName,
                     QWidget* parent = nullptr, bool isFirst = true);
  QString commandId() { return m_commandId; }
  bool setCommand(QString commandId);

protected:
  void enterEvent(QEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragLeaveEvent(QDragLeaveEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
signals:
  void highlight(int);
  void commandChanged(QString, QString);

public:
  void notifyCommandChanged(QString oldCmdID, QString newCmdId) {
    emit commandChanged(oldCmdID, newCmdId);
  }
};
//=============================================================================
// UiPreviewWidget
//-----------------------------------------------------------------------------

class UiPreviewWidget final : public QWidget {
  Q_OBJECT
  int m_highlightUiId;
  QList<QRect> m_rectTable;
  QPixmap m_uiPixmap;

  void onMove(const QPoint pos);

public:
  UiPreviewWidget(QPixmap uiPixmap, QList<UiEntry>& uiEntries,
                  QWidget* parent = nullptr);
  void onViewerResize(QSize size);
  void highlightUi(const int objId);
  void setUiPixmap(QPixmap uiPixmap) {
    m_uiPixmap = uiPixmap;
    update();
  }

protected:
  void paintEvent(QPaintEvent*) override;

  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

signals:
  void clicked(int id);
  void dropped(int id, QString cmdId, bool fromTree);
};

class UiPreviewArea final : public QScrollArea {
  Q_OBJECT
public:
  UiPreviewArea(QWidget* parent = nullptr);

protected:
  void resizeEvent(QResizeEvent* event) override;
};

//=============================================================================
// CustomPanelEditorPopup
//-----------------------------------------------------------------------------

class CustomPanelEditorPopup : public DVGui::Dialog {
  Q_OBJECT
private:
  CommandListTree* m_commandListTree;
  QWidget* m_UiFieldsContainer;
  UiPreviewArea* m_previewArea;

  QComboBox* m_templateCombo;
  QLineEdit* m_panelNameEdit;
  QList<UiEntry> m_uiEntries;

  QList<int> entryIdByObjName(const QString objName);

  bool loadTemplateList();
  void createFields();

  void replaceObjectNames(QDomElement& element);

  // create entries from a widget just loaded from .ui file
  void buildEntries(QWidget* customWidget);
  // update widget using the current entries
  void updateControls(QWidget* customWidget);

public:
  CustomPanelEditorPopup();
protected slots:
  void onTemplateSwitched();
  void onHighlight(int id);
  void onCommandChanged(QString, QString);
  void onPreviewClicked(int id);
  void onPreviewDropped(int id, QString cmdId, bool fromTree);
  void onRegister();
  void onSearchTextChanged(const QString& text);
};

#endif