#pragma once

#ifndef FONT_STYLE_TRANSLATION_INCLUDED
#define FONT_STYLE_TRANSLATION_INCLUDED

#include "tcommon.h"
#include <QStringList>

#undef DVAPI
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#else
#define DVAPI DV_IMPORT_API
#endif

class QComboBox;

namespace DVGui {

// These are display labels only. Font selection and persistence use the
// original names returned by QFontDatabase.
QString DVAPI fontStyleDisplayName(const QString &style);
QStringList DVAPI fontStyleDisplayNames(const QStringList &styles);
void DVAPI populateFontStyleCombo(QComboBox *combo, const QStringList &styles,
                                  const QString &currentStyle);

}  // namespace DVGui

#endif
