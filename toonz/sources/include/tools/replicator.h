#pragma once

#ifndef REPLICATOR_INCLUDED
#define REPLICATOR_INCLUDED

// TnzTools includes
#include <tools/assistant.h>
#include <tools/inputmanager.h>

#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif


//*****************************************************************************************
//    TReplicator definition
//*****************************************************************************************

class DVAPI TReplicator : public TAssistantBase {
  Q_DECLARE_TR_FUNCTIONS(TReplicator)
public:
  typedef std::vector<TPointD> PointList;
  
  const TStringId m_idSkipFirst;
  const TStringId m_idSkipLast;

  static const int multiplierSoftLimit;
  static const int multiplierLimit;
  
  TReplicator(TMetaObject &object);

  void updateTranslation() const override;

  inline int getSkipFirst() const
    { return std::max(0, (int)data()[m_idSkipFirst].getDouble()); }
  inline int getSkipLast() const
    { return std::max(0, (int)data()[m_idSkipLast].getDouble()); }
  
  inline void setSkipFirst(int x)
    { if (getSkipFirst() != (double)x) data()[m_idSkipFirst].setDouble((double)x); }
  inline void setSkipLast(int x)
    { if (getSkipLast() != (double)x) data()[m_idSkipLast].setDouble((double)x); }
  
  virtual int getMultipler() const;
  virtual void getPoints(const TAffine &toTool, PointList &points) const;
  virtual void getModifiers(const TAffine &toTool, TInputModifier::List &outModifiers) const;
  
  static void transformPoints(const TAffine &aff, PointList &points, int i0, int i1);
  static void drawReplicatorPoints(const TPointD *points, int count);
  
  //! return summary multiplier, or 0 is no replicators found
  static int scanReplicators(
    TTool *tool,
    PointList *inOutPoints,
    TInputModifier::List *outModifiers,
    bool draw,
    bool enabledOnly,
    bool markEnabled,
    bool drawPoints,
    TImage *skipImage );
  
protected:
  TIntProperty* createCountProperty(const TStringId &id, int def = 1, int min = 1, int max = 0);
};


#endif
