

// TnzTools includes
#include <tools/replicator.h>
#include <tools/modifiers/modifierclone.h>
#include <tools/assistants/guidelineline.h>


// TnzCore includes
#include <tgl.h>


//*****************************************************************************************
//    TReplicatorStar implementation
//*****************************************************************************************

class TReplicatorStar final : public TReplicator {
  Q_DECLARE_TR_FUNCTIONS(TReplicatorStar)
public:
  const TStringId m_idDiscreteAngle;
  const TStringId m_idMirror;
  const TStringId m_idCount;

protected:
  TAssistantPoint &m_center;
  TAssistantPoint &m_a;

public:
  TReplicatorStar(TMetaObject &object):
    TReplicator(object),
    m_idDiscreteAngle("discreteAngle"),
    m_idMirror("mirror"),
    m_idCount("count"),
    m_center( addPoint("center", TAssistantPoint::CircleCross) ),
    m_a     ( addPoint("a",      TAssistantPoint::Circle, TPointD(80, 0)) )
  {
    addProperty( new TBoolProperty(m_idDiscreteAngle.str(), getDiscreteAngle()) );
    addProperty( new TBoolProperty(m_idMirror.str(),        getMirror()) );
    addProperty( createCountProperty(m_idCount, getCount(), 2) );
  }

  
  static QString getLocalName()
    { return tr("Replicator Star"); }

    
  void updateTranslation() const override {
    TReplicator::updateTranslation();
    setTranslation(m_idDiscreteAngle, tr("Discrete Angle"));
    setTranslation(m_idMirror, tr("Mirror"));
    setTranslation(m_idCount, tr("Count"));
  }

  
  inline bool getDiscreteAngle() const
    { return data()[m_idDiscreteAngle].getBool(); }
  inline bool getMirror() const
    { return data()[m_idMirror].getBool(); }
  inline int getCount() const
    { return (int)data()[m_idCount].getDouble(); }

protected:
  inline void setCount(int x)
    { if (getCount() != (double)x) data()[m_idCount].setDouble((double)x); }

    
  void onSetDefaults() override {
    setCount(6);
    TReplicator::onSetDefaults();
  }

  
  void onFixData() override {
    TReplicator::onFixData();
    setCount( std::max(1, std::min(multiplierSoftLimit - 1, getCount())) );
  }


  TPointD fixA() const {
    TPointD a = m_a.position;
    
    if (getDiscreteAngle()) {
      TPointD d = a - m_center.position;
      double l = norm2(d);
      if (l > TConsts::epsilon*TConsts::epsilon) {
        l = sqrt(l);
        int count = getCount();
        if (count > 0) {
          double angle = atan2(d.y, d.x);
          angle = round(angle*2*count/M_PI)*M_PI/(2*count);
          a.x = cos(angle)*l + m_center.position.x;
          a.y = sin(angle)*l + m_center.position.y;
        }
      }
    }
    
    if (areAlmostEqual(a, m_center.position))
      a = m_center.position + TPointD(1, 0);
    
    return a;
  }

  
  void onMovePoint(TAssistantPoint &point, const TPointD &position) override {
    if (&point == &m_center)
      m_a.position += position - m_center.position;
    point.position = position;
  }
  
  
public:
  int getMultipler() const override
    { return getCount() + 1; }
  
  
  void getPoints(const TAffine &toTool, PointList &points) const override {
    points.reserve(points.size() * getMultipler());
    int pointsCount = (int)points.size();
    int i0 = getSkipFirst();
    int i1 = pointsCount - getSkipLast();
    
    int count = getCount();
    bool mirror = getMirror();

    TPointD c = toTool*m_center.position;
    TPointD x = toTool*fixA() - c;
    TPointD y(-x.y, x.x);

    TAffine t1( x.x,  y.x, c.x,
                x.y,  y.y, c.y );
    TAffine t2( x.x, -y.x, c.x,
                x.y, -y.y, c.y );
    
    TAffine t0 = t1.inv();
    TRotation r(360.0/getCount());
    
    for(int i = 0; i < count; ++i) {
      if (i)
        transformPoints(t1*t0, points, i0, i1);
      if (mirror) {
        transformPoints(t2*t0, points, i0, i1);
        t2 *= r;
      }
      t1 *= r;
    }
  }
  
  
  void getModifiers(
    const TAffine &toTool,
    TInputModifier::List &outModifiers ) const override
  {
    int count = getCount();
    bool mirror = getMirror();

    TPointD c = toTool*m_center.position;
    TPointD x = toTool*fixA() - c;
    TPointD y(-x.y, x.x);

    TAffine t1( x.x,  y.x, c.x,
                x.y,  y.y, c.y );
    TAffine t2( x.x, -y.x, c.x,
                x.y, -y.y, c.y );
    
    TAffine t0 = t1.inv();
    TRotation r(360.0/getCount());
                          
    TModifierClone *modifier = new TModifierClone(true, getSkipFirst(), getSkipLast());
    for(int i = 0; i < count; ++i) {
      if (i)
        modifier->transforms.push_back(TTrackTransform(t1*t0));
      if (mirror) {
        modifier->transforms.push_back(TTrackTransform(t2*t0));
        t2 *= r;
      }
      t1 *= r;
    }
    
    outModifiers.push_back(modifier);
  }

  
  void draw(TToolViewer*, bool enabled) const override {
    double alpha = getDrawingAlpha(enabled);
    double gridAlpha = getDrawingGridAlpha();
    double pixelSize = sqrt(tglGetPixelSize2());

    int count = getCount();
    bool mirror = getMirror();

    TPointD c = m_center.position;
    TPointD a = fixA();
    TPointD d = normalize(a - c);
    
    double spacing = 10*pixelSize;
    double l = spacing*count/M_2PI;
    
    TPointD p0 = c + d*l;
    TPointD p1 = p0 + d;
    TRotation r(360.0/count);
    
    TAffine4 modelview, projection;
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview.a);
    glGetDoublev(GL_PROJECTION_MATRIX, projection.a);

    TAffine matrix = (projection*modelview).get2d();
    TAffine matrixInv = matrix.inv();
    const TRectD oneBox(-1.0, -1.0, 1.0, 1.0);
    
    for(int i = 0; i < count; ++i) {
      TPointD pp0 = matrix*p0;
      TPointD pp1 = matrix*p1;
      if (TGuidelineLine::truncateRay(oneBox, pp0, pp1))
        drawSegment(matrixInv*pp0, matrixInv*pp1, pixelSize, i ? gridAlpha : alpha);
      p0 = r*(p0 - c) + c;
      p1 = r*(p1 - c) + c;
    }
    
    TPointD p = TPointD(-d.y, d.x);
    drawSegment( (mirror ? a+p*15 : a), a-p*15, pixelSize, alpha );
    
    drawSegment(c - d*10, c + d*10, pixelSize, alpha);
    drawSegment(c - p*10, c + p*10, pixelSize, alpha);
  }
};


//*****************************************************************************************
//    Registration
//*****************************************************************************************

static TAssistantTypeT<TReplicatorStar> replicatorStar("replicatorStar");
