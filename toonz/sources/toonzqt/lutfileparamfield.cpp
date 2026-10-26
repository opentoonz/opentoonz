#include "lutfileparamfield.h"

#include "toonz/lut3d.h"
#include "toonz/preferences.h"
#include "toonzqt/lineedit.h"
#include "toonzqt/lutcalibrator.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

LutFileParamField::LutFileParamField(QWidget *parent, QString name,
                                     const TFilePathParamP &param)
    : FilePathParamField(parent, name, param) {
  auto column = new QVBoxLayout();
  column->setContentsMargins(0, 0, 0, 0);
  auto controls = new QHBoxLayout();
  while (auto item = m_layout->takeAt(0)) controls->addItem(item);
  column->addLayout(controls);
  auto copy = new QPushButton(tr("Copy Preferences LUT"), this);
  column->addWidget(copy);
  m_error = new QLabel(this);
  m_error->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  m_error->setWordWrap(true);
  m_error->setTextFormat(Qt::PlainText);
  m_error->hide();
  column->addWidget(m_error);
  m_layout->addLayout(column);
  m_textFld->setToolTip(
      tr("Bake a .cube or Lustre .3dl LUT into rendered colors. "
         "An empty path leaves the image unchanged."));
  copy->setToolTip(
      tr("Copy the LUT path configured for the current monitor, even if "
         "display calibration is disabled. This does not embed the file or "
         "follow later Preferences changes. Monitor calibration normally "
         "belongs on the display, not in rendered output."));
  connect(copy, &QPushButton::clicked, this, [this]() {
    const QString path = Preferences::instance()->getColorCalibrationLutPath(
        LutManager::instance()->getMonitorName());
    if (path.isEmpty()) {
      showError(
          tr("No LUT file is configured for this monitor in Preferences."));
      return;
    }
    setPath(path);
  });
}

void LutFileParamField::showError(const QString &message) {
  // Long filenames need wrap opportunities without changing the stored path.
  QString wrapped;
  int run = 0;
  for (QChar ch : message) {
    wrapped += ch;
    run = ch.isSpace() ? 0 : run + 1;
    if (run >= 16 && !ch.isHighSurrogate()) {
      wrapped += QChar(0x200b);
      run = 0;
    }
  }
  m_error->setText(wrapped);
  m_error->setToolTip(message);
  m_error->show();
  updateGeometry();
}

QSize LutFileParamField::getPreferredSize() {
  return sizeHint().expandedTo(QSize(300, 60));
}

void LutFileParamField::update(int frame) {
  FilePathParamField::update(frame);
  m_error->hide();
  updateGeometry();
}

void LutFileParamField::onChange() {
  if (!m_actualParam) return;
  const QString path = m_textFld->text();
  if (!path.isEmpty()) {
    Lut3D lut;
    QString error;
    if (!lut.load(path, &error)) {
      // Restore the committed path before showing the rejected entry's error.
      FilePathParamField::update(0);
      showError(tr("Cannot use LUT: %1\n%2").arg(path, error));
      return;
    }
    m_textFld->setText(QFileInfo(path).absoluteFilePath());
  }
  m_error->hide();
  FilePathParamField::onChange();
  updateGeometry();
}
