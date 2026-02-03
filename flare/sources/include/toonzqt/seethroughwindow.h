#pragma once

#include "toonzqt/dvdialog.h"

#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QBoxLayout>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class SeeThroughWindowMode;

class SeeThroughWindowPopup final : public DVGui::Dialog {
  Q_OBJECT

  SeeThroughWindowMode *m_mode;

  int m_backupValue;
  QWidget *m_mainWindow;
  QBoxLayout *m_layout;

  QSlider *m_opacitySlider;
  QPushButton *m_opacityBtn;
  QPushButton *m_closeBtn;
  QString m_suffixTxtSlider;

  QIcon m_seeThroughIcon_off;
  QIcon m_seeThroughIcon_on;

public:
  SeeThroughWindowPopup(SeeThroughWindowMode *mode, QWidget *mainWindow);
  QWidget *getMainWindow();

  void toggleMode();

  int getOpacitySlider();
  int setOpacitySlider(int opacity);

  void changeOpacity(int value);

private:
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;
  void resizeEvent(QResizeEvent *) override;

private slots:
  void sliderChanged(int value);
  void opacityToggle();
};

class DVAPI SeeThroughWindowMode final : public QObject {
  Q_OBJECT

  SeeThroughWindowPopup *m_dialog;

public:
  static SeeThroughWindowMode *instance() {
    static SeeThroughWindowMode _instance;
    return &_instance;
  }

  void toggleMode(QWidget *mainWindow);
  QWidget *getMainWindow();
  int getOpacity();
  void refreshOpacity();

signals:
  void opacityChanged(int value, bool &hideMain);

private:
  SeeThroughWindowMode();

  void toggleMode_m(QWidget *mainWindow);
};
