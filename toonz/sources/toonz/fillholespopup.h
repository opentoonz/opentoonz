#include "toonzqt/dvdialog.h"
#include "toonzqt/intfield.h"
#include <QProgressBar>

class FillHolesDialog final : public DVGui::Dialog {
  Q_OBJECT

  DVGui::IntField* m_size;
  DVGui::ProgressDialog* m_progressDialog;

public:
  FillHolesDialog();

protected slots:
  void apply();
};