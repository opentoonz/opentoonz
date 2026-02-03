#pragma once

#ifndef CACHEFXCOMMAND_INCLUDE
#define CACHEFXCOMMAND_INCLUDE

#include "tfx.h"

#include <QObject>

//=============================================================================
//    CacheFxCommand
//-----------------------------------------------------------------------------

class CacheFxCommand final : public QObject {
  Q_OBJECT

public:
  CacheFxCommand();
  ~CacheFxCommand();

  static CacheFxCommand *instance();

  void onNewScene();
  void onSceneLoaded();

  void onLevelChanged(const std::string &levelName);

public slots:

  void onFxChanged();
  void onXsheetChanged();
  void onObjectChanged();
};

//=============================================================================
//    Misc Stuff
//-----------------------------------------------------------------------------

void buildTreeDescription(std::string &desc, const TFxP &root);

#endif
