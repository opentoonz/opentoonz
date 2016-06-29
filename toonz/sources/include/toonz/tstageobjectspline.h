#pragma once

#ifndef T_STAGE_OBJECT_SPLINE_INCLUDED
#define T_STAGE_OBJECT_SPLINE_INCLUDED

#include "tsmartpointer.h"
#include "tgeometry.h"
#include "tpersist.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
// forward declarations
class TStroke;
class TStageObject;
class TDoubleParam;

//=============================================================================
//! The TStageObjectSpline class define stage object motion path.
/*!Inherits \b TSmartObject and \b TPersist.
\n The motion path is defined by spline build as \b TStroke getStroke(),
   and node position getDagNodePos(); spline can be set using setStroke()
   while node position can be set using setDagNodePos().
\n It's possible to clone spline using clone().
*/
//=============================================================================

class DVAPI TStageObjectSpline final : public TSmartObject, public TPersist {
  PERSIST_DECLARATION(TStageObjectSpline)
  TStroke *m_stroke;
  DECLARE_CLASS_CODE
  TPointD m_dagNodePos;

  int m_id;
  std::string m_idBase;
  std::string m_name;
  bool m_isOpened;
  std::vector<TDoubleParam *> m_posPathParams;

public:
  TStageObjectSpline();
  ~TStageObjectSpline();

  TStageObjectSpline *clone() const;

  /*!
Return spline stroke.
\sa setStroke()
*/
  const TStroke *getStroke() const;
  /*!
Set spline stroke to \b stroke.
\sa getStroke()
*/
  void setStroke(TStroke *stroke);  //! keeps ownership

  TPointD getDagNodePos() const { return m_dagNodePos; }
  void setDagNodePos(const TPointD &pos) { m_dagNodePos = pos; }

  int getId() const;
  void setId(int id);
  std::string getName() const;
  void setName(const std::string &name) { m_name = name; }

  bool isOpened() const { return m_isOpened; }
  void setIsOpened(bool value) { m_isOpened = value; }

  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;

  std::string getIconId();

  //! add the PosPath param if you want to update keyframes values
  //! when the stroke changes. addParam() calls param->addRef()
  void addParam(TDoubleParam *param);
  void removeParam(TDoubleParam *param);

private:
  // not implemented: can't copy a TStageObjectSpline
  TStageObjectSpline &operator=(const TStageObjectSpline &);
  TStageObjectSpline(const TStageObjectSpline &);

  void updatePosPathKeyframes(TStroke *oldSpline, TStroke *newSpline);
};

#ifdef _WIN32
template class TSmartPointerT<TStageObjectSpline>;
#endif

typedef TSmartPointerT<TStageObjectSpline> TStageObjectSplineP;

#endif
