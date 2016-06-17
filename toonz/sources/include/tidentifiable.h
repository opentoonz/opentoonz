#pragma once

#ifndef TIDENTIFIABLE_INCLUDED
#define TIDENTIFIABLE_INCLUDED

#include "tutil.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================

class DVAPI TIdentifiable {
  unsigned long m_id;

public:
  TIdentifiable();
  virtual ~TIdentifiable();
  TIdentifiable(const TIdentifiable &);
  const TIdentifiable &operator=(const TIdentifiable &);

  unsigned long getIdentifier() const { return m_id; }
  void setIdentifier(unsigned long id);
  void setNewIdentifier();

  void storeByIdentifier();
  static TIdentifiable *fetchByIdentifier(unsigned long id);
};

#endif
