

#ifndef TSTEPPARAM_H
#define TSTEPPARAM_H

//
// OBSOLETO?
//
/*
#include "tdoubleparam.h"
#include "texception.h"
//#include "tundo.h"
//#include "tparamundo.h"
#include <set>

#undef DVAPI 
#undef DVVAR 
#ifdef TPARAM_EXPORTS
  #define DVAPI DV_EXPORT_API
  #define DVVAR DV_EXPORT_VAR
#else
  #define DVAPI DV_IMPORT_API
  #define DVVAR DV_IMPORT_VAR
#endif

class TDoubleStepParam;
#ifdef _WIN32
template class DVAPI TPersistDeclarationT<TDoubleStepParam>;
#endif


class DVAPI TDoubleStepParam : public TDoubleParam {
  PERSIST_DECLARATION(TDoubleStepParam)
public:
  TDoubleStepParam(double v=0.0);
  TDoubleStepParam(const TDoubleParam &src);
  ~TDoubleStepParam();

  TParam *clone() const {return new TDoubleStepParam(*this);}

  double getValue(double frame, bool cropped=true) const ;

};

DEFINE_PARAM_SMARTPOINTER(TDoubleStepParam, double)
*/

#endif
