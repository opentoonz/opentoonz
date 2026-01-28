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
#include <QComboBox>
#include <QLineEdit>

// Forward declarations
class CustomPanelUIField;
class CommandListTree;

enum UiType { Button = 0, Scroller_Back, Scroller_Fore, TypeCount };

struct UiEntry {
  QString objectName;  // Object name before editing
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
  explicit CustomPanelUIField(int id, const QString& objectName,
                              QWidget* parent = nullptr, bool isFirst = true);

  QString commandId() const { return m_commandId; }
  bool setCommand(const QString& commandId);

protected:
  void enterEvent(QEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragLeaveEvent(QDragLeaveEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

signals:
  void highlight(int id);
  void commandChanged(const QString& oldCmdId, const QString& newCmdId);

public:
  void notifyCommandChanged(const QString& oldCmdId, const QString& newCmdId) {
    emit commandChanged(oldCmdId, newCmdId);
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

  void onMove(const QPoint& pos);

public:
  explicit UiPreviewWidget(const QPixmap& uiPixmap,
                           const QList<UiEntry>& uiEntries,
                           QWidget* parent = nullptr);

  void onViewerResize(const QSize& size);
  void highlightUi(int objId);

  void setUiPixmap(const QPixmap& uiPixmap) {
    m_uiPixmap = uiPixmap;
    update();
  }

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

signals:
  void clicked(int id);
  void dropped(int id, const QString& cmdId, bool fromTree);
};

//=============================================================================
// UiPreviewArea
//-----------------------------------------------------------------------------

class UiPreviewArea final : public QScrollArea {
  Q_OBJECT

public:
  explicit UiPreviewArea(QWidget* parent = nullptr);

protected:
  void resizeEvent(QResizeEvent* event) override;
};

//=============================================================================
// CustomPanelEditorPopup
//-----------------------------------------------------------------------------

class CustomPanelEditorPopup final : public DVGui::Dialog {
  Q_OBJECT

private:
  CommandListTree* m_commandListTree;
  QWidget* m_UiFieldsContainer;
  UiPreviewArea* m_previewArea;
  QComboBox* m_templateCombo;
  QLineEdit* m_panelNameEdit;
  QList<UiEntry> m_uiEntries;

  // Helper methods
  QList<int> entryIdByObjName(const QString& objName);
  bool loadTemplateList();
  void createFields();
  void replaceObjectNames(QDomElement& element);
  void buildEntries(QWidget* customWidget);  // Create entries from UI file
  void updateControls(
      QWidget* customWidget);  // Update widget using current entries

public:
  explicit CustomPanelEditorPopup();

private slots:
  void onTemplateSwitched();
  void onHighlight(int id);
  void onCommandChanged(const QString& oldCmdId, const QString& newCmdId);
  void onPreviewClicked(int id);
  void onPreviewDropped(int id, const QString& cmdId, bool fromTree);
  void onRegister();
  void onSearchTextChanged(const QString& text);
};

#endif  // CUSTOMPANELEDITORPOPUP_H
