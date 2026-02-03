#pragma once

#ifndef PERMISSIONSMANAGER_INCLUDED
#define PERMISSIONSMANAGER_INCLUDED

#include <memory>

// TnzCore includes
#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZBASE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

extern "C" typedef void (*LICENSE_OBSERVER_CB)(char *);

//************************************************************************
//    PermissionsManager  declaration
//************************************************************************

class DVAPI PermissionsManager  // singleton
{
public:
  static PermissionsManager *instance();
  std::string getSVNUserName(int index) const;
  std::string getSVNPassword(int index) const;

private:
  class Imp;
  std::unique_ptr<Imp> m_imp;

private:
  PermissionsManager();
  ~PermissionsManager();
};

#endif
