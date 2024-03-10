

// TnzTools includes
#include <tools/replicator.h>
#include <tools/modifiers/modifierclone.h>
#include <tools/assistants/guidelineline.h>


// TnzCore includes
#include <tgl.h>


//*****************************************************************************************
//    TReplicatorMirror implementation
//*****************************************************************************************

class TReplicatorMirror final : public TReplicator {
  Q_DECLARE_TR_FUNCTIONS(TReplicatorMirror)
public:
  const TStringId m_idDiscreteAngle;
  const TStringId m_idPressure;

protected:
  TAssistantPoint &m_a;
  TAssistantPoint &m_b;

public:
  TReplicatorMirror(TMetaObject &object):
    TReplicator(object),
    m_idDiscreteAngle("discreteAngle"),
    m_idPressure("pressure"),
    m_a( addPoint("a", TAssistantPoint::CircleCross) ),
    m_b( addPoint("b", TAssistantPoint::Circle, TPointD(0, -50)) )
  {
    addProperty( new TBoolProperty(m_idDiscreteAngle.str(),  getDiscreteAngle()) );
    addProperty( new TDoubleProperty(m_idPressure.str(), 0.0, 2.0, getPressure()) );
  }

  
  static QString getLocalName()
    { return tr("Replicator Mirror"); }

    
  void updateTranslation() const override {
    TReplicator::updateTranslation();
    setTranslation(m_idDiscreteAngle, tr("Discrete Angle"));
    setTranslation(m_idPressure, tr("Pressure"));
  }

  
  inline bool getDiscreteAngle() const
    { return data()[m_idDiscreteAngle].getBool(); }
  inline double getPressure() const
    { return data()[m_idPressure].getDouble(); }

protected:
  inline void setPressure(double x)
    { if (getPressure() != x) data()[m_idPressure].setDouble(x); }

    
  void onSetDefaults() override {
    setPressure(1);
    TReplicator::onSetDefaults();
  }

  
  void onFixData() override {
    TReplicator::onFixData();
    setPressure( std::max(0.0, std::min(2.0, getPressure())) );
  }


  TPointD fixB() const {
    TPointD b = m_b.position;
    
    if (getDiscreteAngle()) {
      TPointD d = b - m_a.position;
      double l = norm2(d);
      if (l > TConsts::epsilon*TConsts::epsilon) {
        l = sqrt(l);
        double angle = atan2(d.y, d.x);
        angle = round(angle*4/M_PI)*M_PI/4;
        b.x = cos(angle)*l + m_a.position.x;
        b.y = sin(angle)*l + m_a.position.y;
      }
    }
    
    if (areAlmostEqual(b, m_a.position))
      b = m_a.position + TPointD(1, 0);
    
    return b;
  }
  
  
  TAffine getAffine(const TAffine &toTool = TAffine()) const {
    TPointD c = toTool*m_a.position;
    TPointD x = toTool*fixB() - c;
    TPointD y(-x.y, x.x);
    
    TAffine t0( x.x,  y.x, c.x,
                x.y,  y.y, c.y );
    TAffine t1( x.x, -y.x, c.x,
                x.y, -y.y, c.y );
    return t1*t0.inv();
  }
  

  void onMovePoint(TAssistantPoint &point, const TPointD &position) override {
    if (&point == &m_a)
      m_b.position += position - m_a.position;
    point.position = position;
  }
  

public:
  int getMultipler() const override
    { return 2; }
  
  
  void getPoints(const TAffine &toTool, PointList &points) const override {
    int pointsCount = (int)points.size();
    int i0 = getSkipFirst();
    int i1 = pointsCount - getSkipLast();
    transformPoints(getAffine(toTool), points, i0, i1);
  }
  
  
  void getModifiers(
    const TAffine &toTool,
    TInputModifier::List &outModifiers ) const override
  {
    TModifierClone *modifier = new TModifierClone(true, getSkipFirst(), getSkipLast());
    modifier->transforms.push_back(TTrackTransform(
      getAffine(toTool), getPressure() ));
    outModifiers.push_back(modifier);
  }

  
  void draw(TToolViewer*, bool enabled) const override {
    TPointD p0 = m_a.position;
    TPointD p1 = fixB();
    
    TAffine4 modelview, projection;
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview.a);
    glGetDoublev(GL_PROJECTION_MATRIX, projection.a);

    TAffine matrix = (projection*modelview).get2d();
    TPointD pp0 = matrix*p0;
    TPointD pp1 = matrix*p1;
    const TRectD oneBox(-1.0, -1.0, 1.0, 1.0);
    if (!TGuidelineLine::truncateInfiniteLine(oneBox, pp0, pp1)) return;

    double alpha = getDrawingAlpha(enabled);
    double pixelSize = sqrt(tglGetPixelSize2());
    TAffine matrixInv = matrix.inv();
    drawSegment(matrixInv*pp0, matrixInv*pp1, pixelSize, alpha);
  }
};


//*****************************************************************************************
//    Registration
//*****************************************************************************************

static TAssistantTypeT<TReplicatorMirror> replicatorMirror("replicatorMirror");
