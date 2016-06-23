#pragma once

#ifndef STYLENAMEEDITOR_H
#define STYLENAMEEDITOR_H

#include <QDialog>

class QLineEdit;
class QPushButton;
class TPaletteHandle;

class StyleNameEditor : public QDialog  // singleton
{
  Q_OBJECT

  TPaletteHandle *m_paletteHandle;

  QLineEdit *m_styleName;
  QPushButton *m_okButton, *m_applyButton, *m_cancelButton;

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
};

#endif