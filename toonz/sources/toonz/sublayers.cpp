#include "sublayers.h"

#include "toonz/screenmapper.h"
#include "toonz/txshcell.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txsheet.h"
#include "toonz/imagemanager.h"
#include "toonz/tstageobjectid.h"
#include "toonz/tstageobject.h"

#include "../toonzlib/imagebuilders.h"

#include "tstroke.h"
#include "tvectorimage.h"
#include "../../common/tvectorimage/tvectorimageP.h"
#include "xsheetviewer.h"

// sub layer indicating that there is no sub layer
// special case pattern
class EmptySubLayer final : public SubLayer {
public:
  EmptySubLayer(SubLayers *subLayers) : SubLayer(subLayers, nullptr) { }
};

// Keeps folded state for a column
class LayerSubLayer final : public SubLayer {
  const TXshColumn *m_column;
public:
  LayerSubLayer(SubLayers *subLayers, const TXshColumn *column);

  virtual bool hasChildren() const override;
  virtual QString name() const;

protected:
  virtual vector<shared_ptr<SubLayer>> children() const;
};

// SubLayer containing stroke sub layers for each particular TFrame
class FrameSubLayer final : public SubLayer {
  TFrameId m_frameId;
  bool m_subscribed;

public:
  FrameSubLayer(SubLayers *subLayers, const TFrameId &frameId);

  virtual bool hasChildren() const override;
  virtual QString name() const { return QString::fromStdString(m_frameId.expand()); }

  void subscribeStrokeListChanged();

protected slots:
  void onStrokeListChanged();
private:
  TVectorImageP vectorImage() const;
  TXshSimpleLevel *getLevel() const;
  SubLayer *build(TStroke *stroke);

  bool hasChild(const TStroke *stroke, vector<shared_ptr<SubLayer>>::const_iterator &child) const;
};

// SubLayer representing a single stroke
class StrokeSubLayer final : public SubLayer {
  TStroke *m_stroke; // weak pointer - don't own
  bool m_activated;

public:
  StrokeSubLayer(SubLayers *subLayers, SubLayer *parent, TStroke *stroke);

  virtual bool hasActivator() const override { return true; }
  virtual bool isActivated() const override { return m_activated; }
  virtual void toggleActivator() override { m_activated = !m_activated; }

  virtual QString name() const override;

  bool owns(const TStroke *stroke) const { return m_stroke == stroke; }
};

//-----------------------------------------------------------------------------
// SubLayers

SubLayers::SubLayers(ScreenMapper *mapper) : m_mapper(mapper) {
  connect(ImageManager::instance(), &ImageManager::updatedFrame, this, &SubLayers::onFrameUpdated);
}


shared_ptr<SubLayer> SubLayers::layer(const TXshColumn *column) {
  if (!column)
    return empty();
  map<const TXshColumn *, shared_ptr<SubLayer>>::iterator it = m_layers.find(column);
  if (it != m_layers.end())
    return it->second;
  shared_ptr<SubLayer> newLayer { new LayerSubLayer(this, column) };
  m_layers.insert(std::make_pair(column, shared_ptr<SubLayer>(newLayer)));
  return newLayer;
}

shared_ptr<SubLayer> SubLayers::frame(const TXshColumn *column, int frameNumber) {
  if (!column)
    return empty();
  return frame(CellPosition(frameNumber, column->getIndex()));
}

shared_ptr<SubLayer> SubLayers::frame(const CellPosition &pos) {
  optional<TFrameId> frameId = findFrameId(pos);
  if (!frameId)
    return empty();

  map<TFrameId, shared_ptr<SubLayer>>::iterator it = m_frames.find(*frameId);
  if (it != m_frames.end())
    return it->second;

  shared_ptr<SubLayer> newItem { build(*frameId) };
  m_frames.insert(std::make_pair(*frameId, shared_ptr<SubLayer>(newItem)));
  return newItem;
}

optional<TFrameId> SubLayers::findFrameId(const CellPosition &pos) const {
  if (pos.layer() < 0)
    return boost::none;
  TXshColumn *column = m_mapper->xsheet()->getColumn(pos.layer());
  if (!column)
    return boost::none;
  TXshLevelColumn *levelColumn = column->getLevelColumn();
  if (!levelColumn)
    return boost::none;
  const TXshCell &cell = levelColumn->getCell(pos.frame());
  return cell.getFrameId();
}

SubLayer *SubLayers::build(const TFrameId &frameId) {
  return new FrameSubLayer(this, frameId);
}
shared_ptr<SubLayer> SubLayers::empty() {
  return shared_ptr<SubLayer> (new EmptySubLayer(this));
}

void SubLayers::onFrameUpdated(const TFrameId &frameId) {
  map<TFrameId, shared_ptr<SubLayer>>::iterator it = m_frames.find(frameId);
  if (it == m_frames.end())
    return;
  FrameSubLayer *subLayer = dynamic_cast<FrameSubLayer *> (it->second.get());
  assert(subLayer);
  subLayer->subscribeStrokeListChanged();
}

//-----------------------------------------------------------------------------

vector<int> SubLayers::childrenDimensions(const Orientation *o) {
  vector<int> result;
  TXsheet *xsheet = m_mapper->xsheet();
  ColumnFan *fan = xsheet->getColumnFan();
  int count = xsheet->getColumnCount();

  for (int i = 0; i < count; i++) {
    const TXshColumn *column = xsheet->getColumn(i);
    shared_ptr<SubLayer> root = layer(column);
    result.push_back(root->childrenDimension(o));
  }
  return result;
}

//-----------------------------------------------------------------------------
// SubLayer

SubLayer::SubLayer(SubLayers *subLayers, SubLayer *parent): m_subLayers (subLayers), m_depth (0), m_folded(true) {
  if (parent)
    m_depth = parent->depth() + 1;
  connect(this, &SubLayer::foldToggled, m_subLayers->screenMapper(), &ScreenMapper::updateColumnFan);
}

int SubLayer::ownDimension(const Orientation *o) const {
  return o->dimension(PredefinedDimension::SUBLAYER);
}

int SubLayer::childrenDimension(const Orientation *o) const {
  if (isFolded())
    return 0;
  int sum = 0;
  for (shared_ptr<SubLayer> child : children())
    sum += child->ownDimension(o) + child->childrenDimension(o);
  return sum;
}

vector<shared_ptr<SubLayer>> SubLayer::childrenFlatTree() const {
  vector<shared_ptr<SubLayer>> result;
  if (isFolded())
    return result;
  for (shared_ptr<SubLayer> child : children()) {
    result.push_back(child);
    vector<shared_ptr<SubLayer>> descendants = child->childrenFlatTree();
    result.insert(result.end(), descendants.begin(), descendants.end());
  }
  return result;
}

void SubLayer::foldUnfold() {
  if (hasChildren())
    m_folded = !m_folded;
  else if (m_folded)
    m_folded = false;
  else
    return;

  emit foldToggled();
}

//-----------------------------------------------------------------------------
// LayerSubLayer

LayerSubLayer::LayerSubLayer(SubLayers *subLayers, const TXshColumn *column):
  SubLayer (subLayers, nullptr), m_column (column) {
  assert(m_column);
}

bool LayerSubLayer::hasChildren() const {
  return !m_column->isEmpty();
}

QString LayerSubLayer::name() const {
  int col = m_column->getIndex();
  TStageObjectId columnId = subLayers()->screenMapper()->viewer()->getObjectId(col);
  TStageObject *columnObject = subLayers()->screenMapper()->xsheet()->getStageObject(columnId);

  return QString::fromStdString(columnObject->getName());
}

// substitute with frame sublayer children
vector<shared_ptr<SubLayer>> LayerSubLayer::children() const {
  int frame = subLayers()->screenMapper()->getCurrentFrame();
  shared_ptr<SubLayer> frameSubLayer = subLayers()->frame(m_column, frame);
  return frameSubLayer->children();
}

//-----------------------------------------------------------------------------
// FrameSubLayer

FrameSubLayer::FrameSubLayer(SubLayers *subLayers, const TFrameId &frameId)
  : SubLayer(subLayers, nullptr), m_frameId(frameId), m_subscribed(false) {
  subscribeStrokeListChanged();
}

void FrameSubLayer::subscribeStrokeListChanged() {
  if (m_subscribed)
    return;

  TVectorImageP image = vectorImage();
  if (!image) return;

  for (int i = 0; i < image->getStrokeCount(); i++)
    m_children.push_back(shared_ptr<StrokeSubLayer>(new StrokeSubLayer(subLayers(), this, image->getStroke(i))));

  connect(image.getPointer(), &TVectorImage::strokeListChanged, this, &FrameSubLayer::onStrokeListChanged);
  m_subscribed = true;
}

bool FrameSubLayer::hasChildren() const {
  TVectorImageP image = vectorImage();
  return image && image->getStrokeCount() != 0;
}

TVectorImageP FrameSubLayer::vectorImage() const {
  TXshSimpleLevel *level = getLevel();
  if (!level) return nullptr;

  TImageP image = level->getFrame(m_frameId, false);
  if (!image) return nullptr;

  TVectorImageP vectorImage = { dynamic_cast<TVectorImage *> (image.getPointer()) };
  return vectorImage;
}

// find first matching level
TXshSimpleLevel *FrameSubLayer::getLevel() const {
  TXsheet *xsheet = subLayers()->screenMapper()->xsheet();
  for (int i = 0; i < xsheet->getColumnCount(); i++) {
    TXshColumn *column = xsheet->getColumn(i);
    if (!column) continue;
    TXshLevelColumn *levelColumn = column->getLevelColumn();
    if (!levelColumn) continue;
    
    int f0, f1;
    levelColumn->getRange(f0, f1);
    for (int j = f0; j <= f1; j++) {
      const TXshCell &cell = levelColumn->getCell(j);
      if (cell.getFrameId() == m_frameId)
        return cell.getSimpleLevel();
    }
  }
  return nullptr;
}

void FrameSubLayer::onStrokeListChanged() {
  TVectorImageP image = vectorImage();
  vector<shared_ptr<SubLayer>> newList;

  {
    QMutexLocker sl(image->getMutex());

    for (UINT i = 0; i < image->getStrokeCount(); i++) {
      VIStroke *stroke = image->getVIStroke(i);

      vector<shared_ptr<SubLayer>>::const_iterator child;
      if (hasChild(stroke->m_s, child))
        newList.push_back(*child);
      else
        newList.push_back(shared_ptr<SubLayer>(build(stroke->m_s)));
    }
    std::reverse(newList.begin(), newList.end());
    m_children = newList;
  }
  subLayers()->screenMapper()->updateColumnFan();
}

bool FrameSubLayer::hasChild(const TStroke *stroke, vector<shared_ptr<SubLayer>>::const_iterator &child) const {
  for (child = m_children.begin(); child != m_children.end(); child++) {
    StrokeSubLayer *subLayer = dynamic_cast<StrokeSubLayer *> (child->get());
    if (subLayer->owns(stroke))
      return true;
  }
  return false;
}

SubLayer *FrameSubLayer::build(TStroke *stroke) {
  return new StrokeSubLayer(subLayers(), this, stroke);
}

//-----------------------------------------------------------------------------
// StrokeSubLayer

StrokeSubLayer::StrokeSubLayer(SubLayers *subLayers, SubLayer *parent, TStroke *stroke)
  : SubLayer(subLayers, parent), m_stroke(stroke), m_activated (false)
{ }

QString StrokeSubLayer::name() const {
  QString prefix = m_stroke->name();
  if (prefix.isEmpty())
    prefix = "Shape";
  return prefix + " " + QString::number(m_stroke->getId());
}