#pragma once

#ifndef SAVE_LOAD_QSETTINGS_INCLUDED
#define SAVE_LOAD_QSETTINGS_INCLUDED

#include <QSettings>

class QSettings;

//! An interface that claims: this object wants to save / load something
//! into / from provided qsettings
class SaveLoadQSettings {
public:
  virtual void save(QSettings &settings) const = 0;
  virtual void load(QSettings &settings)       = 0;
};

#endif
