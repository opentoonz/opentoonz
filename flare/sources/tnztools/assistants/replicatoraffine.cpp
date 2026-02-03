

// TnzTools includes
#include <tools/replicator.h>
#include <tools/modifiers/modifierclone.h>


// TnzCore includes
#include <tgl.h>


//*****************************************************************************************
//    TReplicatorAffine implementation
//*****************************************************************************************

class TReplicatorAffine final : public TReplicator {
  Q_DECLARE_TR_FUNCTIONS(TReplicatorAffine)
public:
  const TStringId m_idFixScale;
  const TStringId m_idFixAspect;
  const TStringId m_idFixAngle;
  const TStringId m_idFixSkew;
  const TStringId m_idCount;
  const TStringId m_idCountInv;
  const TStringId m_idPressure;

protected:
  TAssistantPoint &m_center0;
  TAssistantPoint &m_a0;
  TAssistantPoint &m_b0;
  TAssistantPoint &m_center1;
  TAssistantPoint &m_a1;
  TAssistantPoint &m_b1;

public:
  TReplicatorAffine(TMetaObject &object):
    TReplicator(object),
    m_idFixScale("fixScale"),
    m_idFixAspect("fixAspect"),
    m_idFixAngle("fixAngle"),
    m_idFixSkew("fixSkew"),
    m_idCount("count"),
    m_idCountInv("countInv"),
    m_idPressure("pressure"),
    m_center0( addPoint("center0", TAssistantPoint::CircleCross) ),
    m_a0     ( addPoint("a0",      TAssistantPoint::CircleFill, TPointD(40,  0)) ),
    m_b0     ( addPoint("b0",      TAssistantPoint::Circle,     TPointD( 0, 40)) ),
    m_center1( addPoint("center1", TAssistantPoint::Circle,     TPointD(50,  0)) ),
    m_a1     ( addPoint("a1",      TAssistantPoint::CircleFill, TPointD(90,  0)) ),
    m_b1     ( addPoint("b1",      TAssistantPoint::Circle,     TPointD(50, 40)) )
  {
    addProperty( new TBoolProperty(m_idFixScale.str(),  getFixScale()) );
    addProperty( new TBoolProperty(m_idFixAspect.str(), getFixAspect()) );
    addProperty( new TBoolProperty(m_idFixAngle.str(),  getFixAngle()) );
    addProperty( new TBoolProperty(m_idFixSkew.str(),   getFixSkew()) );
    addProperty( createCountProperty(m_idCount, getCount(), 0) );
    addProperty( createCountProperty(m_idCountInv, getCountInv(), 0) );
    addProperty( new TDoubleProperty(m_idPressure.str(), 0.0, 2.0, getPressure()) );
  }

  
  static QString getLocalName()
    { return tr("Replicator Affine"); }

    
  void updateTranslation() const override {
    TReplicator::updateTranslation();
    setTranslation(m_idFixScale, tr("Fix Scale"));
    setTranslation(m_idFixAspect, tr("Fix Aspect"));
    setTranslation(m_idFixAngle, tr("Fix Angle"));
    setTranslation(m_idFixSkew, tr("Fix Skew"));
    setTranslation(m_idCount, tr("Count"));
    setTranslation(m_idCountInv, tr("Inv. Count"));
    setTranslation(m_idPressure, tr("Pressure"));
  }

  
  inline bool getFixScale() const
    { return data()[m_idFixScale].getBool(); }
  inline bool getFixAspect() const
    { return data()[m_idFixAspect].getBool(); }
  inline bool getFixAngle() const
    { return data()[m_idFixAngle].getBool(); }
  inline bool getFixSkew() const
    { return data()[m_idFixSkew].getBool(); }
  inline int getCount() const
    { return (int)data()[m_idCount].getDouble(); }
  inline int getCountInv() const
    { return (int)data()[m_idCountInv].getDouble(); }
  inline double getPressure() const
    { return data()[m_idPressure].getDouble(); }

protected:
  inline void setCount(int x)
    { if (getCount() != (double)x) data()[m_idCount].setDouble((double)x); }
  inline void setCountInv(int x)
    { if (getCountInv() != (double)x) data()[m_idCountInv].setDouble((double)x); }
  inline void setPressure(double x)
    { if (getPressure() != x) data()[m_idPressure].setDouble(x); }

    
  void onSetDefaults() override {
    setCount(1);
    setPressure(1);
    TReplicator::onSetDefaults();
  }

  
  void onFixData() override {
    TReplicator::onFixData();
    setCount( std::max(0, std::min(multiplierSoftLimit - 1, getCount())) );
    setCountInv( std::max(0, std::min(multiplierSoftLimit - 1, getCountInv())) );
    setPressure( std::max(0.0, std::min(2.0, getPressure())) );
  }


  TAffine getAffine(const TAffine &toTool = TAffine()) const {
    TPointD c, x, y;
    c = toTool*m_center0.position;
    x = toTool*m_a0.position - c;
    y = toTool*m_b0.position - c;
    TAffine t0( x.x, y.x, c.x,
                x.y, y.y, c.y );
    c = toTool*m_center1.position;
    x = toTool*m_a1.position - c;
    y = toTool*m_b1.position - c;
    TAffine t1( x.x, y.x, c.x,
                x.y, y.y, c.y );
    return t1*t0.inv();
  }
  
  
  bool isMirror() const {
    TPointD da0 = m_a0.position - m_center0.position;
    TPointD db0 = m_b0.position - m_center0.position;
    TPointD pa0(-da0.y, da0.x);

    TPointD da1 = m_a1.position - m_center1.position;
    TPointD db1 = m_b1.position - m_center1.position;
    TPointD pa1(-da1.y, da0.x);
    
    return (pa0*db0 < 0) != (pa1*db1 < 0);
  }
  

  void doFixPoints(int mirror) {
    TPointD &c0 = m_center0.position;
    TPointD &a0 = m_a0.position;
    TPointD &b0 = m_b0.position;
    
    TPointD &c1 = m_center1.position;
    TPointD &a1 = m_a1.position;
    TPointD &b1 = m_b1.position;

    if (getFixScale()) {
      double la0 = tdistance(a0, c0);
      double lb0 = tdistance(b0, c0);
      double la1 = tdistance(a1, c1);
      double lb1 = tdistance(b1, c1);
      a1 = la1 > TConsts::epsilon
         ? (a1 - c1)*(la0/la1) + c1
         : a0 - c0 + c1;
      b1 = lb1 > TConsts::epsilon
         ? (b1 - c1)*(lb0/lb1) + c1
         : b0 - c0 + c1;
    } else
    if (getFixAspect()) {
      double la0 = tdistance(a0, c0);
      double lb0 = tdistance(b0, c0);
      double la1 = tdistance(a1, c1);
      double lb1 = tdistance(b1, c1);
      if (la0 > TConsts::epsilon)
        b1 = lb1 > TConsts::epsilon
           ? (b1 - c1)*(lb0/lb1*la1/la0) + c1
           : (b0 - c0)*(la1/la0) + c1;
    }

    if (getFixAngle()) {
      double la0 = tdistance(a0, c0);
      double lb0 = tdistance(b0, c0);
      double la1 = tdistance(a1, c1);
      double lb1 = tdistance(b1, c1);
      if (la0 > TConsts::epsilon)
        a1 = (a0 - c0)*(la1/la0) + c1;
      if (lb0 > TConsts::epsilon)
        b1 = (b0 - c0)*(lb1/lb0) + c1;
    } else
    if (getFixSkew()) {
      TPointD da0 = a0 - c0;
      TPointD pa0(-da0.y, da0.x);
      double x = (b0 - c0)*da0;
      double y = (b0 - c0)*pa0;
        
      TPointD da1 = a1 - c1;
      TPointD db1 = b1 - c1;
      TPointD pa1(-da1.y, da1.x);
      if (mirror) y *= mirror; else      
        if ((pa1*db1 < 0) != (y < 0)) y = -y;
      
      TPointD p = da1*x + pa1*y;
      double l = norm2(p);
      if (l > TConsts::epsilon*TConsts::epsilon)
        b1 = p*sqrt(norm2(db1)/l) + c1;
    }
  }

  
  void onFixPoints() override {
    doFixPoints(0);
  }
  
  
  void onMovePoint(TAssistantPoint &point, const TPointD &position) override {
    TPointD pc0 = m_center0.position;
    TPointD pc1 = m_center1.position;
    int mirror = &point == &m_b1 ? 0 : (isMirror() ? -1 : 1);
    point.position = position;
    if (&point == &m_center0) {
      TPointD d = m_center0.position - pc0;
      m_a0.position += d;
      m_b0.position += d;
      m_a1.position += d;
      m_b1.position += d;
      m_center1.position += d;
    } else
    if (&point == &m_center1) {
      TPointD d = m_center1.position - pc1;
      m_a1.position += d;
      m_b1.position += d;
    }
    doFixPoints(mirror);
  }
  
  
  void onDataFieldChanged(const TStringId &name, const TVariant &value) override {
    TReplicator::onDataFieldChanged(name, value);
    if ( name == m_idFixScale
      || name == m_idFixAspect
      || name == m_idFixAngle
      || name == m_idFixSkew )
        fixPoints();
  }

public:
  int getMultipler() const override
    { return getCount() + 1; }
  
  
  void getPoints(const TAffine &toTool, PointList &points) const override {
    points.reserve(points.size() * getMultipler());
    int pointsCount = (int)points.size();
    int i0 = getSkipFirst();
    int i1 = pointsCount - getSkipLast();

    TAffine aff = getAffine(toTool);
    struct {
      int count;
      TAffine aff;
    } t[] = {
      { getCount(), aff },
      { getCountInv(), aff.inv() } };

    for(int i = 0; i < 2; ++i) {
      TAffine a;
      for(int j = 0; j < t[i].count; ++j) {
        a = t[i].aff * a;
        transformPoints(a, points, i0, i1);
      }
    }
  }
  
  
  void getModifiers(
    const TAffine &toTool,
    TInputModifier::List &outModifiers ) const override
  {
    TAffine aff = getAffine(toTool);
    double pressure = getPressure();
    double pressureInv = fabs(pressure) > TConsts::epsilon ? 1/pressure : 0;
    
    struct {
      int count;
      TAffine aff;
      double pressure;
    } t[] = {
      { getCount(), aff, pressure },
      { getCountInv(), aff.inv(), pressureInv },
    };

    TModifierClone *modifier = new TModifierClone(true, getSkipFirst(), getSkipLast());
    for(int i = 0; i < 2; ++i) {
      TTrackTransform tt;
      for(int j = 0; j < t[i].count; ++j) {
        tt.transform = t[i].aff * tt.transform;
        tt.tiltTransform = TTrackTransform::makeTiltTransform(tt.transform);
        tt.pressureScale *= t[i].pressure;
        modifier->transforms.push_back(tt);
      }
    }
    
    outModifiers.push_back(modifier);
  }

  
  void draw(TToolViewer*, bool enabled) const override {
    double alpha = getDrawingAlpha(enabled);
    double gridAlpha = getDrawingGridAlpha();
    double pixelSize = sqrt(tglGetPixelSize2());

    TPointD c = m_center0.position;
    TPointD a = m_a0.position;
    TPointD b = m_b0.position;
    TAffine aff = getAffine();

    // draw base
    drawSegment(c, a, pixelSize, alpha);
    drawSegment(c, b, pixelSize, alpha);
    
    // draw clones
    TAffine t;
    for(int i = getCount(); i > 0; --i) {
      t = aff * t;
      drawSegment(t*c, t*a, pixelSize, gridAlpha);
      drawSegment(t*c, t*b, pixelSize, gridAlpha);
    }
    
    // draw inverted clones
    t = TAffine();
    aff = aff.inv();
    for(int i = getCountInv(); i > 0; --i) {
      t = aff * t;
      drawSegment(t*c, t*a, pixelSize, gridAlpha);
      drawSegment(t*c, t*b, pixelSize, gridAlpha);
    }
  }
};


//*****************************************************************************************
//    Registration
//*****************************************************************************************

static TAssistantTypeT<TReplicatorAffine> replicatorAffine("replicatorAffine");
