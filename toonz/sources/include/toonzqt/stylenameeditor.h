#pragma once

#ifndef STYLENAMEEDITOR_H
#define STYLENAMEEDITOR_H

#include "dvdialog.h"

#include <QWidget>
#include <QStringList>
#include <QPushButton>

class QLineEdit;
class QPushButton;
class TPaletteHandle;
class QGridLayout;
class QScrollArea;

const int WORD_COLUMN_AMOUNT = 3;

//------------------------------------------------------------

class NewWordDialog : public QDialog {
  Q_OBJECT
  QLineEdit *m_lineEdit;

public:
  NewWordDialog(QWidget *parent = nullptr);
  QString getName();
};

//------------------------------------------------------------

class WordButton : public QPushButton {
  Q_OBJECT
public:
  WordButton(const QString &text, QWidget *parent = nullptr);

protected:
  void contextMenuEvent(QContextMenuEvent *) override;

protected slots:
  virtual void onClicked();
  void onRemove();

signals:
  void clicked(const QString &);
  void removeWord(const QString &);
};

//------------------------------------------------------------

class AddWordButton final : public WordButton {
  Q_OBJECT
  int m_column;

public:
  AddWordButton(const int col, QWidget *parent = nullptr);

protected slots:
  void onClicked() override;
  void contextMenuEvent(QContextMenuEvent *) override{};

signals:
  void clicked(const int);
};

//------------------------------------------------------------

class EasyInputArea final : public QWidget {
  Q_OBJECT

  QStringList m_wordList[WORD_COLUMN_AMOUNT];
  QGridLayout *m_wordLayout[WORD_COLUMN_AMOUNT];
  QScrollArea *m_scrollArea[WORD_COLUMN_AMOUNT];

  void loadList();
  void saveList();
  void updatePanelSize(int columnId);

public:
  EasyInputArea(QWidget *parent = 0);

protected:
  void enterEvent(QEvent *) override;
  void resizeEvent(QResizeEvent *) override;

protected slots:
  void addWordButtonClicked(const int);
  void onRemoveWord(const QString &);

signals:
  void wordClicked(const QString &);
  void mouseEnter();
};

//------------------------------------------------------------

class StyleNameEditor final : public DVGui::Dialog {
  Q_OBJECT

  TPaletteHandle *m_paletteHandle;

  QLineEdit *m_styleName;
  QPushButton *m_okButton, *m_applyButton, *m_cancelButton;

  int m_selectionStart, m_selectionLength;

public:
  StyleNameEditor(QWidget *parent = 0);
  void setPaletteHandle(TPaletteHandle *ph);

protected:
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;
  void enterEvent(QEvent *) override;

protected slots:
  void onStyleSwitched();
  void onOkPressed();
  void onApplyPressed();
  void onCancelPressed();
  void onWordClicked(const QString &);
  void storeSelectionInfo();
};

#endif