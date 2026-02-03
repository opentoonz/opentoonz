

// TnzTools includes
#include <tools/replicator.h>
#include <tools/modifiers/modifierjitter.h>


// TnzCore includes
#include <tgl.h>


//*****************************************************************************************
//    TReplicatorJitter implementation
//*****************************************************************************************

class TReplicatorJitter final : public TReplicator {
  Q_DECLARE_TR_FUNCTIONS(TReplicatorJitter)
public:
  const TStringId m_idPeriod;
  const TStringId m_idAmplitude;
  const TStringId m_idKeepFirstPoint;
  const TStringId m_idKeepLastPoint;

protected:
  TAssistantPoint &m_center;

public:
  TReplicatorJitter(TMetaObject &object):
    TReplicator(object),
    m_idPeriod("period"),
    m_idAmplitude("amplitude"),
    m_idKeepFirstPoint("keepFirstPoint"),
    m_idKeepLastPoint("keepLastPoint"),
    m_center( addPoint("center", TAssistantPoint::CircleCross) )
  {
    TDoubleProperty *p;
    
    p = new TDoubleProperty(m_idPeriod.str(), 0.0, 1000, getPeriod());
    p->setNonLinearSlider();
    addProperty(p);

    p = new TDoubleProperty(m_idAmplitude.str(), 0.0, 1000, getAmplitude());
    p->setNonLinearSlider();
    addProperty(p);
    
    addProperty( new TBoolProperty(m_idKeepFirstPoint.str(), getKeepFirstPoint()) );
    addProperty( new TBoolProperty(m_idKeepLastPoint.str(), getKeepLastPoint()) );
  }

  
  static QString getLocalName()
    { return tr("Jitter"); }

    
  void updateTranslation() const override {
    TReplicator::updateTranslation();
    setTranslation(m_idPeriod, tr("Period"));
    setTranslation(m_idAmplitude, tr("Amplitude"));
    setTranslation(m_idKeepFirstPoint, tr("Fix First Point"));
    setTranslation(m_idKeepLastPoint, tr("Fix Last Point"));
  }

  
  inline double getPeriod() const
    { return data()[m_idPeriod].getDouble(); }
  inline double getAmplitude() const
    { return data()[m_idAmplitude].getDouble(); }
  inline bool getKeepFirstPoint() const
    { return data()[m_idKeepFirstPoint].getBool(); }
  inline bool getKeepLastPoint() const
    { return data()[m_idKeepLastPoint].getBool(); }

protected:
  inline void setPeriod(double x)
    { if (getPeriod() != x) data()[m_idPeriod].setDouble(x); }
  inline void setAmplitude(double x)
    { if (getAmplitude() != x) data()[m_idAmplitude].setDouble(x); }

    
  void onSetDefaults() override {
    setPeriod(30);
    setAmplitude(10);
    TReplicator::onSetDefaults();
  }

  
  void onFixData() override {
    TReplicator::onFixData();
    setPeriod( std::max(0.0, std::min(1000.0, getPeriod())) );
    setAmplitude( std::max(0.0, std::min(1000.0, getAmplitude())) );
  }

  
  double getScale(const TAffine &a) const {
    return sqrt( a.a11*a.a11 + a.a12*a.a12
               + a.a21*a.a21 + a.a22*a.a22 )/2;
  }
    
public:
  
  void getPoints(const TAffine &toTool, PointList &points) const override {
    int pointsCount = (int)points.size();
    int i0 = getSkipFirst();
    int i1 = pointsCount - getSkipLast();
    if (i0 < 0) i0 = 0;
    if (i1 > pointsCount) i1 = pointsCount;
    if (i0 >= i1) return;
    
    double scale = getScale(toTool);
    double period = getPeriod()*scale;
    double amplitude = getAmplitude()*scale;
    if (period > TConsts::epsilon && amplitude > TConsts::epsilon) {
      int seedX = 0;
      int seedY = 7722441;
      for(int i = i0; i < i1; ++i) {
        points[i].x += TModifierJitter::func(seedX, points[i].x/period)*amplitude;
        points[i].y += TModifierJitter::func(seedY, points[i].y/period)*amplitude;
        ++seedX, ++seedY;
      }
    }
  }
  
  
  void getModifiers(
    const TAffine &toTool,
    TInputModifier::List &outModifiers ) const override
  {
    double scale = getScale(toTool);
    outModifiers.push_back(new TModifierJitter(
      getPeriod()*scale,
      getAmplitude()*scale,
      getSkipFirst(),
      getSkipLast(),
      getKeepFirstPoint(),
      getKeepLastPoint() ));
  }

  
  void draw(TToolViewer*, bool enabled) const override {
    double alpha = getDrawingAlpha(enabled);
    double pixelSize = sqrt(tglGetPixelSize2());
    
    TPointD c = m_center.position;
    double h = getPeriod()/2;
    double q = h/2;
    double a = getAmplitude();
    
    drawSegment(TPointD(c.x-h, c.y  ), TPointD(c.x-q, c.y+a), pixelSize, alpha);
    drawSegment(TPointD(c.x-q, c.y+a), TPointD(c.x+q, c.y-a), pixelSize, alpha);
    drawSegment(TPointD(c.x+q, c.y-a), TPointD(c.x+h, c.y  ), pixelSize, alpha);
  }
};


//*****************************************************************************************
//    Registration
//*****************************************************************************************

static TAssistantTypeT<TReplicatorJitter> replicatorJitter("replicatorJitter");
