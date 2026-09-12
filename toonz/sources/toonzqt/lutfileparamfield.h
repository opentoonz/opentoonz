#pragma once

#include "toonzqt/paramfield.h"

class QLabel;

class LutFileParamField final : public FilePathParamField {
  QLabel *m_error;
  void showError(const QString &message);

public:
  LutFileParamField(QWidget *parent, QString name,
                    const TFilePathParamP &param);
  QSize getPreferredSize() override;
  void update(int frame) override;

protected:
  void onChange() override;
};
