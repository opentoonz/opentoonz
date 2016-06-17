#pragma once

#ifndef TPARAMCHANGE_INCLUDED
#define TPARAMCHANGE_INCLUDED

// TnzCore includes
#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TPARAM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//===========================================================

//    Forward declarations

class TParam;

//===========================================================

//*****************************************************************************************
//    TParamChange  declaration
//*****************************************************************************************

class DVAPI TParamChange {
public:
  TParam *m_param;  //!< (not owned) The parameter being changed

  double m_firstAffectedFrame,
      m_lastAffectedFrame;  //!< First and last frames affected by the change

  bool
      m_keyframeChanged;  //!< Whether a keyframe has been altered by the change
  bool m_dragging;        //!< Whether the change is about a mouse being dragged
  bool m_undoing;  //!< Whether the change is happening within an undo operation

  static double m_minFrame;
  static double m_maxFrame;

public:
  TParamChange(TParam *param, double firstAffectedFrame,
               double lastAffectedFrame, bool keyframeChanged, bool dragging,
               bool undoing);

  virtual ~TParamChange() {}
};

//*****************************************************************************************
//    TParamObserver  definition
//*****************************************************************************************

class DVAPI TParamObserver {
public:
  virtual ~TParamObserver() {}

  virtual void onChange(const TParamChange &) = 0;
};

#endif  // TPARAMCHANGE_INCLUDED
