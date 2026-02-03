

// TnzTools includes
#include <tools/replicator.h>
#include <tools/modifiers/modifierclone.h>


// TnzCore includes
#include <tgl.h>


//*****************************************************************************************
//    TReplicatorGrid implementation
//*****************************************************************************************

class TReplicatorGrid final : public TReplicator {
  Q_DECLARE_TR_FUNCTIONS(TReplicatorGrid)
public:
  const TStringId m_idFixAngle;
  const TStringId m_idFixSkew;
  const TStringId m_idMirrorA;
  const TStringId m_idMirrorB;
  const TStringId m_idCountA;
  const TStringId m_idCountAInv;
  const TStringId m_idCountB;
  const TStringId m_idCountBInv;

protected:
  TAssistantPoint &m_center;
  TAssistantPoint &m_a;
  TAssistantPoint &m_b;

public:
  TReplicatorGrid(TMetaObject &object):
    TReplicator(object),
    m_idFixAngle("fixAngle"),
    m_idFixSkew("fixSkew"),
    m_idMirrorA("mirrorA"),
    m_idMirrorB("mirrorB"),
    m_idCountA("countA"),
    m_idCountAInv("countAInv"),
    m_idCountB("countB"),
    m_idCountBInv("countBInv"),
    m_center( addPoint("center", TAssistantPoint::CircleCross) ),
    m_a     ( addPoint("a",      TAssistantPoint::CircleFill, TPointD(80,   0)) ),
    m_b     ( addPoint("b",      TAssistantPoint::Circle,     TPointD( 0, -80)) )
  {
    addProperty( new TBoolProperty(m_idFixAngle.str(),  getFixAngle()) );
    addProperty( new TBoolProperty(m_idFixSkew.str(),   getFixSkew()) );
    addProperty( new TBoolProperty(m_idMirrorA.str(),   getMirrorA()) );
    addProperty( new TBoolProperty(m_idMirrorB.str(),   getMirrorB()) );
    addProperty( createCountProperty(m_idCountA, getCountA(), 1) );
    addProperty( createCountProperty(m_idCountAInv, getCountAInv(), 0) );
    addProperty( createCountProperty(m_idCountB, getCountB(), 1) );
    addProperty( createCountProperty(m_idCountBInv, getCountBInv(), 0) );
  }

  
  static QString getLocalName()
    { return tr("Replicator Grid"); }

    
  void updateTranslation() const override {
    TReplicator::updateTranslation();
    setTranslation(m_idFixAngle, tr("Fix Angle"));
    setTranslation(m_idFixSkew, tr("Fix Skew"));
    setTranslation(m_idMirrorA, tr("Mirror A"));
    setTranslation(m_idMirrorB, tr("Mirror B"));
    setTranslation(m_idCountA, tr("Count A"));
    setTranslation(m_idCountAInv, tr("Inv. Count A"));
    setTranslation(m_idCountB, tr("Count B"));
    setTranslation(m_idCountBInv, tr("Inv. Count B"));
  }

  
  inline bool getFixAngle() const
    { return data()[m_idFixAngle].getBool(); }
  inline bool getFixSkew() const
    { return data()[m_idFixSkew].getBool(); }
  inline bool getMirrorA() const
    { return data()[m_idMirrorA].getBool(); }
  inline bool getMirrorB() const
    { return data()[m_idMirrorB].getBool(); }
  inline int getCountA() const
    { return (int)data()[m_idCountA].getDouble(); }
  inline int getCountAInv() const
    { return (int)data()[m_idCountAInv].getDouble(); }
  inline int getCountB() const
    { return (int)data()[m_idCountB].getDouble(); }
  inline int getCountBInv() const
    { return (int)data()[m_idCountBInv].getDouble(); }

protected:
  inline void setCountA(int x)
    { if (getCountA() != (double)x) data()[m_idCountA].setDouble((double)x); }
  inline void setCountAInv(int x)
    { if (getCountAInv() != (double)x) data()[m_idCountAInv].setDouble((double)x); }
  inline void setCountB(int x)
    { if (getCountB() != (double)x) data()[m_idCountB].setDouble((double)x); }
  inline void setCountBInv(int x)
    { if (getCountBInv() != (double)x) data()[m_idCountBInv].setDouble((double)x); }

    
  void onSetDefaults() override {
    setCountA(3);
    setCountAInv(2);
    setCountB(2);
    setCountBInv(1);
    TReplicator::onSetDefaults();
  }

  
  void onFixData() override {
    TReplicator::onFixData();
    setCountA( std::max(1, std::min(multiplierSoftLimit, getCountA())) );
    setCountAInv( std::max(0, std::min(multiplierSoftLimit, getCountAInv())) );
    setCountB( std::max(1, std::min(multiplierSoftLimit, getCountB())) );
    setCountBInv( std::max(0, std::min(multiplierSoftLimit, getCountBInv())) );
  }


  void onFixPoints() override {
    TPointD &c = m_center.position;
    TPointD &a = m_a.position;
    TPointD &b = m_b.position;

    if (getFixAngle()) {
      double la = tdistance(a, c);
      double lb = tdistance(b, c);
      a = TPointD(a.x < c.x ? -la : la, 0) + c;
      b = TPointD(0, b.y < c.y ? -lb : lb) + c;
    } else
    if (getFixSkew()) {
      TPointD pa(c.y - a.y, a.x - c.x);
      double l = norm2(pa);
      if (l > TConsts::epsilon*TConsts::epsilon) {
        TPointD db = b - c;
        double k = sqrt(norm2(db)/l);
        if (db*pa < 0) k = -k;
        b = pa*k + c;
      }
    }
  }
  
  
  void onMovePoint(TAssistantPoint &point, const TPointD &position) override {
    TPointD pc = m_center.position;
    TPointD pa = m_a.position;
    point.position = position;
    if (&point == &m_center) {
      TPointD d = m_center.position - pc;
      m_a.position += d;
      m_b.position += d;
    } else {
      fixPoints();
    }
  }
  
  
  void onDataFieldChanged(const TStringId &name, const TVariant &value) override {
    TReplicator::onDataFieldChanged(name, value);
    if ( name == m_idFixAngle
      || name == m_idFixSkew )
        fixPoints();
  }

 
public:
  int getMultipler() const override {
    return (getCountA() + getCountAInv())
         * (getCountB() + getCountBInv())
         * (getMirrorA() ? 2 : 1)
         * (getMirrorB() ? 2 : 1);    
  }
  
  
  void getPoints(const TAffine &toTool, PointList &points) const override {
    points.reserve(points.size() * getMultipler());
    int pointsCount = (int)points.size();
    int i0 = getSkipFirst();
    int i1 = pointsCount - getSkipLast();
    
    TPointD c = toTool*m_center.position;
    TPointD da = toTool*m_a.position - c;
    TPointD db = toTool*m_b.position - c;
    
    bool mirrorA = getMirrorA();
    bool mirrorB = getMirrorB();
    TAffine t, ma, mb, mc;
    if (mirrorA || mirrorB) {
      if (fabs(da*db) > TConsts::epsilon) {
        t  = TAffine(  da.x,  db.x, c.x,
                       da.y,  db.y, c.y ).inv();
        ma = TAffine( -da.x,  db.x, c.x,
                      -da.y,  db.y, c.y )*t;
        mb = TAffine(  da.x, -db.x, c.x,
                       da.y, -db.y, c.y )*t;
        mc = TAffine( -da.x, -db.x, c.x,
                      -da.y, -db.y, c.y )*t;
      } else {
        mirrorA = mirrorB = false;
      }
    }
    
    int a1 = getCountA();
    int b1 = getCountB();
    int a0 = -getCountAInv();
    int b0 = -getCountBInv();
    
    for(int ib = b0; ib < b1; ++ib)
    for(int ia = a0; ia < a1; ++ia) {
      TPointD o = da*ia + db*ib;
      if (ia || ib)
        transformPoints(
          TAffine( 1, 0, o.x,
                   0, 1, o.y ),
          points, i0, i1 );
      if (mirrorA)
        transformPoints(
          TAffine( ma.a11, ma.a12, ma.a13 + o.x,
                   ma.a21, ma.a22, ma.a23 + o.y ),
          points, i0, i1 );
      if (mirrorB)
        transformPoints(
          TAffine( mb.a11, mb.a12, mb.a13 + o.x,
                   mb.a21, mb.a22, mb.a23 + o.y ),
          points, i0, i1 );
      if (mirrorA && mirrorB)
        transformPoints(
          TAffine( mc.a11, mc.a12, mc.a13 + o.x,
                   mc.a21, mc.a22, mc.a23 + o.y ),
          points, i0, i1 );
    }
  }

  
  void getModifiers(
    const TAffine &toTool,
    TInputModifier::List &outModifiers ) const override
  {
    TPointD c = toTool*m_center.position;
    TPointD da = toTool*m_a.position - c;
    TPointD db = toTool*m_b.position - c;
    
    bool mirrorA = getMirrorA();
    bool mirrorB = getMirrorB();
    TAffine t, ma, mb, mc;
    if (mirrorA || mirrorB) {
      if (fabs(da*db) > TConsts::epsilon) {
        t  = TAffine(  da.x,  db.x, c.x,
                       da.y,  db.y, c.y ).inv();
        ma = TAffine( -da.x,  db.x, c.x,
                      -da.y,  db.y, c.y )*t;
        mb = TAffine(  da.x, -db.x, c.x,
                       da.y, -db.y, c.y )*t;
        mc = TAffine( -da.x, -db.x, c.x,
                      -da.y, -db.y, c.y )*t;
      } else {
        mirrorA = mirrorB = false;
      }
    }
    
    int a1 = getCountA();
    int b1 = getCountB();
    int a0 = -getCountAInv();
    int b0 = -getCountBInv();
    
    TModifierClone *modifier = new TModifierClone(true, getSkipFirst(), getSkipLast());
    for(int ib = b0; ib < b1; ++ib)
    for(int ia = a0; ia < a1; ++ia) {
      TPointD o = da*ia + db*ib;
      if (ia || ib)
        modifier->transforms.push_back(TTrackTransform(
          TAffine( 1, 0, o.x,
                   0, 1, o.y ) ));
      if (mirrorA)
        modifier->transforms.push_back(TTrackTransform(
          TAffine( ma.a11, ma.a12, ma.a13 + o.x,
                   ma.a21, ma.a22, ma.a23 + o.y ) ));
      if (mirrorB)
        modifier->transforms.push_back(TTrackTransform(
          TAffine( mb.a11, mb.a12, mb.a13 + o.x,
                   mb.a21, mb.a22, mb.a23 + o.y ) ));
      if (mirrorA && mirrorB)
        modifier->transforms.push_back(TTrackTransform(
          TAffine( mc.a11, mc.a12, mc.a13 + o.x,
                   mc.a21, mc.a22, mc.a23 + o.y ) ));
    }
    outModifiers.push_back(modifier);
  }

  
  void draw(TToolViewer*, bool enabled) const override {
    double alpha = getDrawingAlpha(enabled);
    double gridAlpha = getDrawingGridAlpha();
    double pixelSize = sqrt(tglGetPixelSize2());

    TPointD c = m_center.position;
    TPointD a = m_a.position;
    TPointD b = m_b.position;
    TPointD da = a - c;
    TPointD db = b - c;

    int a1 = getCountA();
    int b1 = getCountB();
    int a0 = -getCountAInv();
    int b0 = -getCountBInv();
    
    bool mirrorA = getMirrorA();
    bool mirrorB = getMirrorB();
    
    // draw base
    drawSegment(c, a, pixelSize, alpha);
    drawSegment(c, b, pixelSize, alpha);
    
    // draw clones
    for(int ib = b0; ib < b1; ++ib)
    for(int ia = a0; ia < a1; ++ia) {
      TPointD o = c + da*ia + db*ib;
      if (ia || ib) {
        drawSegment(o, o + da*0.2, pixelSize, gridAlpha);
        drawSegment(o, o + db*0.2, pixelSize, gridAlpha);
      }
      if (mirrorA && (ib || ia != 1))
        drawSegment(o - da*0.2, o, pixelSize, gridAlpha);
      if (mirrorB && (ia || ib != 1))
        drawSegment(o - db*0.2, o, pixelSize, gridAlpha);
    }
  }
};


//*****************************************************************************************
//    Registration
//*****************************************************************************************

static TAssistantTypeT<TReplicatorGrid> replicatorGrid("replicatorGrid");
