#pragma once

#ifndef FILLHOLESDIALOG_H
#define FILLHOLESDIALOG_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/intfield.h"
#include <QProgressBar>

#include <memory>  // for std::unique_ptr

// Forward declaration
namespace DVGui {
class ProgressDialog;
}

class FillHolesDialog final : public DVGui::Dialog {
  Q_OBJECT

  DVGui::IntField* m_size;
  std::unique_ptr<DVGui::ProgressDialog> m_progressDialog;  // automatic cleanup

public:
  explicit FillHolesDialog();
  ~FillHolesDialog() = default;  // unique_ptr handles destruction

  // Prohibit copying and moving
  FillHolesDialog(const FillHolesDialog&)            = delete;
  FillHolesDialog& operator=(const FillHolesDialog&) = delete;
  FillHolesDialog(FillHolesDialog&&)                 = delete;
  FillHolesDialog& operator=(FillHolesDialog&&)      = delete;

protected slots:
  void apply();
};

#endif  // FILLHOLESDIALOG_H
