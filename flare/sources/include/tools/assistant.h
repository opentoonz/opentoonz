#pragma once

#ifndef ASSISTANT_INCLUDED
#define ASSISTANT_INCLUDED

// TnzTools includes
#include <tools/track.h>

// TnzCore includes
#include <tsmartpointer.h>
#include <tgeometry.h>
#include <tmetaimage.h>
#include <tproperty.h>

// Qt includes
#include <QCoreApplication>

// std includes
#include <vector>
#include <string>
#include <map>


#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif


//==============================================================

//  Forward declarations

class TProperty;
class TPropertyGroup;

class TTool;
class TToolViewer;
class TAssistant;
class TAssistantBase;
class TAssistantPoint;
class TGuideline;

typedef TSmartPointerT<TGuideline> TGuidelineP;
typedef std::vector<TGuidelineP> TGuidelineList;
typedef std::map<TStringId, TAssistantPoint> TAssistantPointMap;
typedef std::vector<const TAssistantPoint*> TAssistantPointOrder;

//===================================================================


//*****************************************************************************************
//    TGuideline definition
//*****************************************************************************************

class DVAPI TGuideline : public TSmartObject {
public:
  const bool enabled;
  const double magnetism;
  const TPixelD color;

  TGuideline(bool enabled, double magnetism, const TPixelD &color):
    enabled(enabled), magnetism(magnetism), color(color) { }

  virtual TTrackPoint transformPoint(const TTrackPoint &point) const
    { return point; }
  
  // this function uses in calcTrackWeight to select best guideline,
  // does not use for drawing
  virtual TPointD nearestPoint(const TPointD &point) const
    { return transformPoint(TTrackPoint(point)).position; }
  
  virtual void draw(bool active, bool enabled) const
    { }
  
  void draw(bool active = false) const
    { draw(active, true); }

  TTrackPoint smoothTransformPoint(const TTrackPoint &point, double magnetism = 1.0) const {
    return enabled
         ? TTrack::interpolationLinear(point, transformPoint(point), magnetism*this->magnetism)
         : point;
  }

  void drawSegment(
    const TPointD &p0,
    const TPointD &p1,
    double pixelSize,
    bool active,
    bool enabled = true) const;

  double calcTrackWeight(const TTrack &track, const TAffine &toScreen, bool &outLongEnough) const;
  static TGuidelineP findBest(const TGuidelineList &guidelines, const TTrack &track, const TAffine &toScreen, bool &outLongEnough);
};


//*****************************************************************************************
//    TAssistantPoint definition
//*****************************************************************************************

class DVAPI TAssistantPoint {
public:
  enum Type {
    Circle,
    CircleFill,
    CircleCross,
    CircleDiagonalCross,
    CircleDots,
    CircleDoubleDots,
  };

  const TStringId name;
  const TPointD defPosition;

  Type type;
  TPointD position;
  double radius;
  bool visible;

  mutable bool selected;

  explicit TAssistantPoint(const TStringId &name, const TPointD &defPosition = TPointD());
};


//*****************************************************************************************
//    TAssistantType definition
//*****************************************************************************************

class DVAPI TAssistantType: public TMetaObjectType {
public:
  TAssistantType(const TStringId &name):
    TMetaObjectType(name) { }
  TMetaObjectHandler* createHandler(TMetaObject &obj) const override;
  virtual TAssistantBase* createAssistant(TMetaObject &obj) const
    { return 0; }
};


//*****************************************************************************************
//    TAssistantTypeT definition
//*****************************************************************************************

template<typename T>
class TAssistantTypeT: public TAssistantType {
public:
  typedef T Type;

  explicit TAssistantTypeT(
    const TStringId &name,
    const TStringId &alias1 = TStringId(),
    const TStringId &alias2 = TStringId(),
    const TStringId &alias3 = TStringId(),
    const TStringId &alias4 = TStringId(),
    const TStringId &alias5 = TStringId()
  ):
    TAssistantType(TStringId(name))
  {
    if (alias1) registerAlias(alias1);
    if (alias2) registerAlias(alias2);
    if (alias3) registerAlias(alias3);
    if (alias4) registerAlias(alias4);
    if (alias5) registerAlias(alias5);
  }

  explicit TAssistantTypeT(
    const std::string &name,
    const std::string &alias1 = std::string(),
    const std::string &alias2 = std::string(),
    const std::string &alias3 = std::string(),
    const std::string &alias4 = std::string(),
    const std::string &alias5 = std::string()
  ):
    TAssistantType(TStringId(name))
  {
    if (!alias1.empty()) registerAlias(TStringId(alias1));
    if (!alias2.empty()) registerAlias(TStringId(alias2));
    if (!alias3.empty()) registerAlias(TStringId(alias3));
    if (!alias4.empty()) registerAlias(TStringId(alias4));
    if (!alias5.empty()) registerAlias(TStringId(alias5));
  }

  TAssistantBase* createAssistant(TMetaObject &obj) const override
    { return new Type(obj); }
  QString getLocalName() const override {
    QString localName = Type::getLocalName();
    return localName.isEmpty() ? QString::fromStdString(name.str()) : localName;
  }
};


//*****************************************************************************************
//    TAssistantBase definition
//*****************************************************************************************

class DVAPI TAssistantBase : public TMetaObjectHandler {
  Q_DECLARE_TR_FUNCTIONS(TAssistantBase)
public:
  enum {
    DRAW_ERROR = 1,
  };
  
  static TPixelD colorBase;
  static TPixelD colorError;
  static TPixelD colorEdit;
  static TPixelD colorSelect;
  static unsigned int drawFlags;
  static const double lineWidthScale;
  
protected:
  const TStringId m_idEnabled;
  const TStringId m_idPoints;
  const TStringId m_idX;
  const TStringId m_idY;
  const TStringId m_idZ;
  const TStringId m_idW;
  const TStringId m_idMagnetism;

  TAssistantPointMap m_points;
  TAssistantPointOrder m_pointsOrder;
  TAssistantPoint* m_basePoint;

  mutable TPropertyGroup m_properties;

public:
  TAssistantBase(TMetaObject &object);

  static QString getLocalName()
    { return QString(); }

  inline const TAssistantPointMap& points() const
    { return m_points; }
  inline const TAssistantPointOrder& pointsOrder() const
    { return m_pointsOrder; }

  inline const TAssistantPoint* findPoint(const TStringId &name) const {
    TAssistantPointMap::const_iterator i = points().find(name);
    return i == points().end() ? 0 : &i->second;
  }

  void fixPoints();
  bool move(const TPointD &position);
  bool movePoint(const TStringId &name, const TPointD &position);
  void setPointSelection(const TStringId &name, bool selected) const;
  void setAllPointsSelection(bool selected) const;

  bool getEnabled() const
    { return data()[m_idEnabled].getBool(); }
  void setEnabled(bool x)
    { if (getEnabled() != x) data()[m_idEnabled].setBool(x); }

  inline void selectPoint(const TStringId &name) const
    { setPointSelection(name, true); }
  inline void deselectPoint(const TStringId &name) const
    { setPointSelection(name, false); }
  inline void selectAll() const
    { setAllPointsSelection(true); }
  inline void deselectAll() const
    { setAllPointsSelection(false); }

  TPropertyGroup& getProperties() const
    { return m_properties; }
  void propertyChanged(const TStringId &name)
    { onPropertyChanged(name); }

  const TAssistantPoint& getBasePoint() const;

protected:
  TAssistantPoint& addPoint(
    const TStringId &name,
    TAssistantPoint::Type type,
    const TPointD &defPosition,
    bool visible,
    double radius );

  TAssistantPoint& addPoint(
    const TStringId &name,
    TAssistantPoint::Type type = TAssistantPoint::Circle,
    const TPointD &defPosition = TPointD(),
    bool visible               = true );

  inline TAssistantPoint& addPoint(
    const std::string &name,
    TAssistantPoint::Type type,
    const TPointD &defPosition,
    bool visible,
    double radius )
      { return addPoint(TStringId(name), type, defPosition, visible, radius); }

  inline TAssistantPoint& addPoint(
    const std::string &name,
    TAssistantPoint::Type type = TAssistantPoint::Circle,
    const TPointD &defPosition = TPointD(),
    bool visible               = true )
      { return addPoint(TStringId(name), type, defPosition, visible); }

  //! usually called when meta-object created
  void onSetDefaults() override;
  //! called when part of variant data changed
  void onDataChanged(const TVariant &value) override;
  //! called when field of root struct of variant data changed
  virtual void onDataFieldChanged(const TStringId &name, const TVariant &value);
  //! load object data from variant
  virtual void onAllDataChanged();
  //! fix positions of all points (as like as all points moved)
  virtual void onFixPoints();
  //! try to move point
  virtual void onMovePoint(TAssistantPoint &point, const TPointD &position);
  //! save object data to variant
  void onFixData() override;
  //! load all properties from variant
  virtual void updateProperties();
  //! load single property from variant
  virtual void updateProperty(const TStringId &name, const TVariant &value);
  //! put value from property to variant
  virtual void onPropertyChanged(const TStringId &name);

  double getDrawingAlpha(bool enabled = true) const;
  double getDrawingGridAlpha() const;

  TIntProperty* createSpinProperty(const TStringId &id, int def, int min, int max, bool hasMax = true);
  inline TIntProperty* createSpinProperty(const TStringId &id, int def, int min)
    { return createSpinProperty(id, def, min, 0, false); }
  
  void addProperty(TProperty *p);
  void setTranslation(const TStringId &name, const QString &localName) const;

public:
  static TPixelD makeContrastColor(const TPixelD &color);
  static void drawSegment(const TPointD &p0, const TPointD &p1, double pixelSize, double alpha0, double alpha1);
  static void drawMark(const TPointD &p, const TPointD &normal, double pixelSize, double alpha);
  static void drawDot(const TPointD &p, const TPixelD &color);
  static void drawDot(const TPointD &p, double alpha);
  static void drawPoint(const TAssistantPoint &point, double pixelSize);
  static void drawIndex(const TPointD &p, int index, bool selected, double pixelSize);

  static inline void drawSegment(const TPointD &p0, const TPointD &p1, double pixelSize, double alpha)
    { drawSegment(p0, p1, pixelSize, alpha, alpha); }
  inline void drawSegment(const TPointD &p0, const TPointD &p1, double pixelSize) const
    { drawSegment(p0, p1, pixelSize, getDrawingAlpha()); }
  inline void drawDot(const TPointD &p) const
    { drawDot(p, getDrawingAlpha()); }

  virtual void updateTranslation() const;
  virtual void draw(TToolViewer *viewer, bool enabled) const;
  void draw(TToolViewer *viewer) const { draw(viewer, true); }
  virtual void drawEdit(TToolViewer *viewer) const;
  virtual void drawEdit(TToolViewer *viewer, int index) const;
};


//*****************************************************************************************
//    TAssistant definition
//*****************************************************************************************

class DVAPI TAssistant : public TAssistantBase {
  Q_DECLARE_TR_FUNCTIONS(TAssistant)
protected:
  const TStringId m_idMagnetism;

  void onSetDefaults() override;
  void onFixData() override;

public:
  TAssistant(TMetaObject &object);

  double getMagnetism() const
    { return data()[m_idMagnetism].getDouble(); }
  void setMagnetism(double x)
    { if (getMagnetism() != x) data()[m_idMagnetism].setDouble(x); }

public:
  void updateTranslation() const override;
  virtual void getGuidelines(
    const TPointD &position,
    const TAffine &toTool,
    const TPixelD &color,
    TGuidelineList &outGuidelines) const;

  // calc W-coefficient and i-bounds for formula: x0 + 1/(i*W + 1)
  static bool calcPerspectiveStep(
    double minStep,
    double minX,
    double maxX,
    double x0,
    double x1,
    double x2,
    double &outW,
    double &outMinI,
    double &outMaxI );
  
  static bool scanAssistants(
    TTool *tool,
    const TPointD *positions,
    int positionsCount,
    TGuidelineList *outGuidelines,
    bool draw,
    bool enabledOnly,
    bool markEnabled,
    bool drawGuidelines,
    TImage *skipImage );
};


//*****************************************************************************************
//    export template implementations for win32
//*****************************************************************************************

#ifdef _WIN32
template class DVAPI TSmartPointerT<TGuideline>;
#endif


#endif
