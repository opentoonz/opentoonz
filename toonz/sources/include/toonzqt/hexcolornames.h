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
#include <QMap>
#include <QTreeWidget>
#include <QStyledItemDelegate>

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

//! Manage all Hex and named colors conversions.
class HexColorNames final : public QObject {
  Q_OBJECT
  HexColorNames();

  static QMap<QString, QString> s_maincolornames;
  static QMap<QString, QString> s_usercolornames;
  static QMap<QString, QString> s_tempcolornames;

  static void loadColorTableXML(QMap<QString, QString> &table,
                                const TFilePath &fp);
  static void saveColorTableXML(QMap<QString, QString> &table,
                                const TFilePath &fp);
  static bool parseHexInternal(QString text, TPixel &outPixel);

public:

  //! iterator for accessing user entries
  class iterator {
    QMap<QString, QString>::iterator m_it;
    bool m_mutable;

  public:
    inline iterator() : m_it(nullptr), m_mutable(false) {}
    inline iterator(iterator *it)
        : m_it(it->m_it), m_mutable(it->m_mutable) {}
    inline iterator(QMap<QString, QString>::iterator it, bool isMutable)
        : m_it(it), m_mutable(isMutable) {}

    inline const QString &name() const { return m_it.key(); }
    inline const QString &value() const { return m_it.value(); }
    inline void setValue(const QString &value) { if (m_mutable) m_it.value() = value; }
    inline bool operator==(const iterator &o) const { return m_it == o.m_it; }
    inline bool operator!=(const iterator &o) const { return m_it != o.m_it; }
    inline iterator &operator++() {
      ++m_it;
      return *this;
    }
    inline iterator &operator--() {
      --m_it;
      return *this;
    }
    inline iterator operator++(int) {
      iterator r = *this;
      m_it++;
      return r;
    }
    inline iterator operator--(int) {
      iterator r = *this;
      m_it--;
      return r;
    }
  };

  //! Load entries from main colornames.txt file
  static bool loadMainFile(bool reload);

  //! Load entries from user colornames.txt file
  static bool loadUserFile(bool reload);

  //! Load temporary entries from custom file
  static bool loadTempFile(const TFilePath& fp);

  //! Verify if there's personal colornames.txt file
  static bool hasUserFile();

  //! Save entries to user colornames.txt file
  static bool saveUserFile();

  //! Load temporary entries from custom file
  static bool saveTempFile(const TFilePath &fp);

  //! Clear all user entries
  static void clearUserEntries();

  //! Clear all user entries
  static void clearTempEntries();

  //! Set single user entry
  static void setUserEntry(const QString &name, const QString &hex);

  //! Set single user entry
  static void setTempEntry(const QString &name, const QString &hex);

  //! Get first main entry
  static inline iterator beginMain() {
    return iterator(s_maincolornames.begin(), false);
  }

  //! Get last main entry
  static inline iterator endMain() {
    return iterator(s_maincolornames.end(), false);
  }

  //! Get first user entry
  static inline iterator beginUser() {
    return iterator(s_usercolornames.begin(), true);
  }

  //! Get last user entry
  static inline iterator endUser() {
    return iterator(s_usercolornames.end(), true);
  }

  //! Get first user entry
  static inline iterator beginTemp() {
    return iterator(s_tempcolornames.begin(), true);
  }

  //! Get last user entry
  static inline iterator endTemp() {
    return iterator(s_tempcolornames.end(), true);
  }

  //! Parse raw text into RGBA color
  static bool parseText(QString text, TPixel &outPixel);

  //! Parse hex format into RGBA color
  static bool parseHex(QString text, TPixel &outPixel);

  //! Generate hex format from RGBA color
  static QString generateHex(TPixel pixel);

  //! To access singleton for signaling
  static HexColorNames *instance();

  //! Emit that user entries were changed
  inline void emitChanged() { emit colorsChanged(); }
  inline void emitAutoComplete(bool enable) { emit autoCompleteChanged(enable); }

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

  QCompleter *getCompleter();

  bool m_editing;
  TPixel m_color;
  QCompleter *m_completer;

protected slots:
  void onAutoCompleteChanged(bool);
  void onColorsChanged();
};

//-----------------------------------------------------------------------------

//! Editing delegate for editor
class HexColorNamesEditingDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  HexColorNamesEditingDelegate(QObject *parent = nullptr)
      : QStyledItemDelegate(parent) {
    connect(this,
            SIGNAL(closeEditor(QWidget *,
                               QAbstractItemDelegate::EndEditHint)),
            this,
            SLOT(onCloseEditor(QWidget *, QAbstractItemDelegate::EndEditHint)));
  }
  virtual QWidget *createEditor(QWidget *parent,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const {
    QWidget *widget = QStyledItemDelegate::createEditor(parent, option, index);
    emit editingStarted(index);
    return widget;
  }
  virtual void setModelData(QWidget *editor, QAbstractItemModel *model,
                            const QModelIndex &index) const {
    QStyledItemDelegate::setModelData(editor, model, index);
    emit editingFinished(index);
  }

signals:
  void editingStarted(const QModelIndex &) const;
  void editingFinished(const QModelIndex &) const;
  void editingClosed() const;

private slots:
  inline void onCloseEditor(QWidget *editor,
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

  QTreeWidgetItem *addEntry(QTreeWidget *tree, const QString &name,
                            const QString &hex, bool editable);
  bool updateUserHexEntry(QTreeWidgetItem *treeItem, const TPixel32 &color);
  bool nameValid(const QString& name);
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
