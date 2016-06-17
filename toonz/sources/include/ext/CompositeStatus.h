#pragma once

#ifndef COMPOSITESTATUS_H
#define COMPOSITESTATUS_H

/*****************************************************************************\
*                                                                             *
*                           Author Fabrizio Morciano                          *
*                                                                             *
\*****************************************************************************/

#include "tcommon.h"
#include "tstroke.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include <vector>
#include <string>

namespace ToonzExt {

/**
   * This class mantains interal data for Dragger manipulator.
   */
class DVAPI CompositeStatus {
private:
  std::map<std::string, CompositeStatus *> *dbImpl_;

  std::map<std::string, CompositeStatus *> &db_;

  typedef std::map<std::string, CompositeStatus *>::iterator iterator;
  typedef std::map<std::string, CompositeStatus *>::const_iterator
      const_iterator;

public:
  CompositeStatus();
  virtual ~CompositeStatus();

  void add(CompositeStatus *, const std::string &name);

  void remove(const std::string &name);

  CompositeStatus *find(const std::string &name) const;
};

//---------------------------------------------------------------------------
}
#endif /* COMPOSITESTATUS_H */
