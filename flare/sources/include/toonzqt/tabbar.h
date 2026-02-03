#pragma once

#ifndef TABBAR_H
#define TABBAR_H

#include "tcommon.h"
#include <QTabBar>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4251)
#endif

//=============================================================================

namespace DVGui {

//=============================================================================
/*! \brief The TabBar class allows to show a tabar with icon in tab.

                Inherits \b QTabar.

                This object, more than \b QTabBar, allows to show icon in tab
   using \b addIconTab(),
                it's anyhow possible add text tab using \b addSimpleTab().
*/
class DVAPI TabBar final : public QTabBar {
  Q_OBJECT

  std::vector<QPixmap> m_pixmaps;

public:
  TabBar(QWidget *parent = 0);
  ~TabBar();

  void addIconTab(const char *iconPrefixName, const QString &tooltip);
  void addSimpleTab(const QString &text);
  void clearTabBar();

protected:
  void paintEvent(QPaintEvent *event);
};

//-----------------------------------------------------------------------------
}  // namespace DVGui
//-----------------------------------------------------------------------------

#endif  // TABBAR_H
