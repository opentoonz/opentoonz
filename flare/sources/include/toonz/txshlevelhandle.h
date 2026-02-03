#pragma once

#include <QObject>
#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class TXshLevel;
class TXshSimpleLevel;

//=============================================================================
// TXshLevelHandle
//-----------------------------------------------------------------------------

class DVAPI TXshLevelHandle final : public QObject {
  Q_OBJECT

  TXshLevel *m_level;

public:
  TXshLevelHandle();
  ~TXshLevelHandle();

  TXshLevel *getLevel() const;
  TXshSimpleLevel *getSimpleLevel() const;  // helper function:
                                            // getLevel()->getSimpleLevel(),
                                            // controllando che getLevel() !=0

  void setLevel(TXshLevel *level);

  void notifyLevelChange() { emit xshLevelChanged(); }
  void notifyLevelViewChange() { emit xshLevelViewChanged(); }
  void notifyLevelTitleChange() { emit xshLevelTitleChanged(); }
  void notifyCanvasSizeChange() { emit xshCanvasSizeChanged(); }

signals:
  void xshLevelSwitched(TXshLevel *oldLevel);
  void xshLevelChanged();
  void xshLevelViewChanged();
  void xshLevelTitleChanged();
  void xshCanvasSizeChanged();
};
