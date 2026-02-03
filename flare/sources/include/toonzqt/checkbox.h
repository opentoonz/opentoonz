#pragma once

#ifndef CHECKBOX_H
#define CHECKBOX_H

#include "tcommon.h"

#include <QCheckBox>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================

namespace DVGui {

//=============================================================================
/*! \brief It's a \b QCheckBox that change state also with click in text.

                Inherits \b QCheckBox.
*/

class DVAPI CheckBox : public QCheckBox {
  Q_OBJECT

public:
  CheckBox(QWidget *parent = 0);
  CheckBox(const QString &text, QWidget *parent = 0);

  ~CheckBox() {}

protected:
  void mousePressEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *) override;
};

//-----------------------------------------------------------------------------
}  // namespace DVGui
//-----------------------------------------------------------------------------

#endif  // CHECKBOX_H
