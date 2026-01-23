#pragma once

#ifndef HEXCOLORNAMES_H
#define HEXCOLORNAMES_H

// TnzCore includes
#include "tcommon.h"
#include "tfilepath.h"
#include "tpixel.h"
#include "tpalette.h"
#include "tenv.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/colorfield.h"

// Qt includes
#include <QString>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QCompleter>
#include <QHash>
#include <QTreeWidget>
#include <QStyledItemDelegate>
#include <QStringListModel>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

namespace DVGui {

//-----------------------------------------------------------------------------

//! Manages all Hex and named colors conversions.
class HexColorNames final : public QObject {
  Q_OBJECT
  HexColorNames();

  static QHash<QString, QString> s_maincolornames;
  static QHash<QString, QString> s_usercolornames;
  static QHash<QString, QString> s_tempcolornames;

  static bool loadColorTableXML(QHash<QString, QString> &table,
                                const TFilePath &fp);
  static bool saveColorTableXML(QHash<QString, QString> &table,
                                const TFilePath &fp);
  static bool parseHexInternal(const QString &text, TPixel &outPixel);

public:
  static bool loadMainFile(bool reload);
  static bool loadUserFile(bool reload);
  static bool loadTempFile(const TFilePath &fp);
  static bool hasUserFile();
  static bool saveUserFile();
  static bool saveTempFile(const TFilePath &fp);

  static void clearUserEntries();
  static void clearTempEntries();

  static void setUserEntry(const QString &name, const QString &hex);
  static void setTempEntry(const QString &name, const QString &hex);

  // Return native QHash iterators (no custom class)
  static inline QHash<QString, QString>::const_iterator beginMain() {
    return s_maincolornames.cbegin();
  }
  static inline QHash<QString, QString>::const_iterator endMain() {
    return s_maincolornames.cend();
  }

  static inline QHash<QString, QString>::iterator beginUser() {
    return s_usercolornames.begin();
  }
  static inline QHash<QString, QString>::iterator endUser() {
    return s_usercolornames.end();
  }

  static inline QHash<QString, QString>::iterator beginTemp() {
    return s_tempcolornames.begin();
  }
  static inline QHash<QString, QString>::iterator endTemp() {
    return s_tempcolornames.end();
  }

  static bool parseText(const QString &text, TPixel &outPixel);
  static bool parseHex(const QString &text, TPixel &outPixel);
  static QString generateHex(TPixel pixel);

  static HexColorNames *instance();

  inline void emitChanged() { emit colorsChanged(); }
  inline void emitAutoComplete(bool enable) {
    emit autoCompleteChanged(enable);
  }

signals:
  void autoCompleteChanged(bool);
  void colorsChanged();
};

//-----------------------------------------------------------------------------

//! Hex line-edit widget
class DVAPI HexLineEdit : public QLineEdit {
  Q_OBJECT

public:
  HexLineEdit(const QString &contents, QWidget *parent);
  ~HexLineEdit() {}

  void setStyle(TColorStyle &style, int index);
  void updateColor();
  void setColor(TPixel color);
  TPixel getColor() { return m_color; }
  bool fromText(QString text);
  bool fromHex(QString text);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;
  void showEvent(QShowEvent *event) override;

  bool m_editing;
  TPixel m_color;
  QCompleter *m_completer;
  QStringListModel *m_completerModel;

private slots:
  void onAutoCompleteChanged(bool);
  void onColorsChanged();
  void updateCompleterList();
};

//-----------------------------------------------------------------------------

//! Editing delegate for editor
class HexColorNamesEditingDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  HexColorNamesEditingDelegate(QObject *parent = nullptr)
      : QStyledItemDelegate(parent) {
    // Modern Qt5 connection syntax
    connect(this, &HexColorNamesEditingDelegate::closeEditor, this,
            &HexColorNamesEditingDelegate::onCloseEditor);
  }

  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                        const QModelIndex &index) const override {
    QWidget *widget = QStyledItemDelegate::createEditor(parent, option, index);
    emit editingStarted(index);
    return widget;
  }

  void setModelData(QWidget *editor, QAbstractItemModel *model,
                    const QModelIndex &index) const override {
    QStyledItemDelegate::setModelData(editor, model, index);
    emit editingFinished(index);
  }

signals:
  void editingStarted(const QModelIndex &) const;
  void editingFinished(const QModelIndex &) const;
  void editingClosed() const;

private slots:
  void onCloseEditor(QWidget *editor,
                     QAbstractItemDelegate::EndEditHint hint = NoHint) {
    emit editingClosed();
  }
};

//-----------------------------------------------------------------------------

//! Dialog: Hex color names editor
class HexColorNamesEditor final : public DVGui::Dialog {
  Q_OBJECT

  HexColorNamesEditingDelegate *m_userEditingDelegate;
  QTreeWidget *m_userTreeWidget;
  QTreeWidget *m_mainTreeWidget;
  QCheckBox *m_autoCompleteCb;
  QPushButton *m_addColorButton;
  QPushButton *m_delColorButton;
  QPushButton *m_unsColorButton;
  QPushButton *m_importButton;
  QPushButton *m_exportButton;
  HexLineEdit *m_hexLineEdit;
  ColorField *m_colorField;
  bool m_autoComplete;

  bool m_newEntry;
  int m_selectedColn;
  QTreeWidgetItem *m_selectedItem;
  QString m_selectedName, m_selectedHex;
  bool m_processingItemFinished = false;

  QTreeWidgetItem *addEntry(QTreeWidget *tree, const QString &name,
                            const QString &hex, bool editable);
  bool updateUserHexEntry(QTreeWidgetItem *treeItem, const TPixel32 &color);
  bool nameValid(const QString &name);
  bool nameExists(const QString &name, QTreeWidgetItem *self);
  void deselectItem(bool treeFocus);
  void deleteCurrentItem(bool deselect);
  void populateMainList(bool reload);
  void populateUserList(bool reload);

  void showEvent(QShowEvent *e) override;
  void keyPressEvent(QKeyEvent *event) override;
  void focusInEvent(QFocusEvent *e) override;
  void focusOutEvent(QFocusEvent *e) override;
  bool eventFilter(QObject *obj, QEvent *e) override;

private slots:
  void onCurrentItemChanged(QTreeWidgetItem *current,
                            QTreeWidgetItem *previous);
  void onEditingStarted(const QModelIndex &);
  void onEditingFinished(const QModelIndex &);
  void onEditingClosed();
  void onItemStarted(QTreeWidgetItem *item, int column);
  void onItemFinished(QTreeWidgetItem *item, int column);
  void onColorFieldChanged(const TPixel32 &color, bool isDragging);
  void onHexChanged();
  void onAddColor();
  void onDelColor();
  void onDeselect();
  void onImport();
  void onExport();
  void onOK();
  void onApply();

public:
  HexColorNamesEditor(QWidget *parent);
};

//-----------------------------------------------------------------------------
}  // namespace DVGui
//-----------------------------------------------------------------------------

#endif  // HEXCOLORNAMES_H
