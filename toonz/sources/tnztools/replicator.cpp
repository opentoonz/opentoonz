
#include <tools/replicator.h>

#include <tools/tool.h>

#include <toonz/tapplication.h>
#include <toonz/txsheet.h>
#include <toonz/txsheethandle.h>
#include <toonz/txshlevelhandle.h>
#include <toonz/tframehandle.h>
#include <toonz/tobjecthandle.h>
#include <toonz/dpiscale.h>
#include <toonz/toonzscene.h>
#include <toonz/sceneproperties.h>

#include <tgl.h>
#include <tpixelutils.h>



const int TReplicator::multiplierSoftLimit = 32;
const int TReplicator::multiplierLimit = 256;


//************************************************************************
//    TReplicator implementation
//************************************************************************

TReplicator::TReplicator(TMetaObject &object):
  TAssistantBase(object),
  m_idSkipFirst("skipFirst"),
  m_idSkipLast("skipLast")
{
  addProperty( createSpinProperty(m_idSkipFirst, getSkipFirst(), 0) );
  addProperty( createSpinProperty(m_idSkipLast, getSkipLast(), 0) );
}

//---------------------------------------------------------------------------------------------------

void
TReplicator::updateTranslation() const {
  TAssistantBase::updateTranslation();
  setTranslation(m_idSkipFirst, tr("Skip First Tracks"));
  setTranslation(m_idSkipLast, tr("Skip Last Tracks"));
}

//---------------------------------------------------------------------------------------------------

int
TReplicator::getMultipler() const
  { return 1; }

//---------------------------------------------------------------------------------------------------

void
TReplicator::getModifiers(const TAffine&, TInputModifier::List&) const
  { }

//---------------------------------------------------------------------------------------------------

void
TReplicator::getPoints(const TAffine&, PointList&) const
  { }

//---------------------------------------------------------------------------------------------------

TIntProperty*
TReplicator::createCountProperty(const TStringId &id, int def, int min, int max) {
  if (min < 0) min = 0;
  if (def < min) def = min;
  if (max <= 0) max = multiplierSoftLimit;
  return createSpinProperty(id, def, min, max);
}

//---------------------------------------------------------------------------------------------------

void
TReplicator::transformPoints(const TAffine &aff, PointList &points, int i0, int i1) {
  int cnt = (int)points.size();
  if (i0 < 0) i0 = 0;
  if (i1 > cnt) i1 = cnt;
  if (i0 >= i1) return;
  points.reserve(points.size() + i1 - i0);
  for(int i = i0; i < i1; ++i)
    points.push_back(aff*points[i]);
}

//---------------------------------------------------------------------------------------------------

void
TReplicator::drawReplicatorPoints(const TPointD *points, int count) {
  TPixelD color = (drawFlags & DRAW_ERROR) ? colorError : colorBase;
  color.m *= 0.3;
  TPixelD colorBack = makeContrastColor(color);
  
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  tglEnableBlending();
  tglEnableLineSmooth(true, 1.0 * lineWidthScale);
  
  double pixelSize = sqrt(tglGetPixelSize2());
  TPointD a(5*pixelSize, 0), da(0, 0.5*pixelSize);
  TPointD b(0, 5*pixelSize), db(0.5*pixelSize, 0);
  
  for(int i = 0; i < count; ++i) {
    const TPointD &p = points[i];
    tglColor(colorBack);
    tglDrawSegment(p - a - da, p + a - da);
    tglDrawSegment(p - b - db, p + b - db);
    tglColor(color);
    tglDrawSegment(p - a + da, p + a + da);
    tglDrawSegment(p - b + db, p + b + db);
  }

  glPopAttrib();
}

//---------------------------------------------------------------------------------------------------

int
TReplicator::scanReplicators(
  TTool *tool,
  PointList *inOutPoints,
  TInputModifier::List *outModifiers,
  bool draw,
  bool enabledOnly,
  bool markEnabled,
  bool drawPoints,
  TImage *skipImage )
{
  bool outOfLimit = false;
  long long multiplier = 0;
  int inputPoints = inOutPoints ? (int)inOutPoints->size() : 0;
  
  if (tool)
  if (TToolViewer *viewer = tool->getViewer())
  if (TApplication *application = tool->getApplication())
  if (TXshLevelHandle *levelHandle = application->getCurrentLevel())
  if (TXshLevel *level = levelHandle->getLevel())
  if (TXshSimpleLevel *simpleLevel = level->getSimpleLevel())
  if (TFrameHandle *frameHandle = application->getCurrentFrame())
  if (TXsheetHandle *XsheetHandle = application->getCurrentXsheet())
  if (TXsheet *Xsheet = XsheetHandle->getXsheet())
  {
    ToonzScene *scene = Xsheet->getScene();
    TSceneProperties *props = scene ? scene->getProperties() : nullptr;
    TPointD dpiScale = getCurrentDpiScale(simpleLevel, tool->getCurrentFid());
    int frame = frameHandle->getFrame();
    int count = Xsheet->getColumnCount();
    TAffine worldToTrack;
    if ( tool->getToolType() & TTool::LevelTool
      && !application->getCurrentObject()->isSpline() )
    {
      worldToTrack.a11 /= dpiScale.x;
      worldToTrack.a22 /= dpiScale.y;
    }

    for(int i = 0; i < count; ++i) {
      if (TXshColumn *column = Xsheet->getColumn(i))
      if (column->isCamstandVisible())
      if (column->isPreviewVisible())
      if (TImageP image = Xsheet->getCell(frame, i).getImage(false))
      if (image != skipImage)
      if (image->getType() == TImage::META)
      if (TMetaImage *metaImage = dynamic_cast<TMetaImage*>(image.getPointer()))
      {
        TPixelD origColor = TAssistantBase::colorBase;
        TAffine imageToTrack = worldToTrack * tool->getColumnMatrix(i);
        if (draw) {
          if (props)
            TAssistantBase::colorBase = toPixelD(props->getColorFilterColor( column->getColorFilterId() ));
          TAssistantBase::colorBase.m *= column->getOpacity()/255.0;
          glPushMatrix();
          tglMultMatrix(imageToTrack);
        }

        TMetaImage::Reader reader(*metaImage);
        for(TMetaObjectListCW::iterator i = reader->begin(); i != reader->end(); ++i) {
          if (*i)
          if (const TReplicator *replicator = (*i)->getHandler<TReplicator>())
          if (!enabledOnly || replicator->getEnabled())
          {
            if (!multiplier) multiplier = 1;
            bool enabled = replicator->getEnabled();
            
            if (enabled) {
              int m = replicator->getMultipler();
              if (m <= 0) m = 1;
              if (multiplier*m > multiplierLimit) {
                outOfLimit = true;
              } else {
                multiplier *= m;
                if (inOutPoints)
                  replicator->getPoints(imageToTrack, *inOutPoints);
                if (outModifiers)
                  replicator->getModifiers(imageToTrack, *outModifiers);
              }
            }
            
            if (draw) {
              unsigned int oldFlags = TReplicator::drawFlags;
              if (enabled && outOfLimit) TReplicator::drawFlags |= TReplicator::DRAW_ERROR;
              replicator->draw(viewer, enabled && markEnabled);
              TReplicator::drawFlags = oldFlags;
            }
          }
        } // for each meta object
        
        if (draw) {
          glPopMatrix();
          TAssistantBase::colorBase = origColor;
        }
      }
    } // for each column
  }
  
  
  if (drawPoints && inOutPoints && (int)inOutPoints->size() > inputPoints)
    drawReplicatorPoints(
      inOutPoints->data() + inputPoints,
      (int)inOutPoints->size() - inputPoints );
  
  return (int)multiplier;
}

