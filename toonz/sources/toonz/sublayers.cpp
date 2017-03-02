#include "sublayers.h"

#include "toonz/screenmapper.h"
#include "toonz/txshcell.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txsheet.h"
#include "toonz/imagemanager.h"
#include "toonz/tstageobjectid.h"
#include "toonz/tstageobject.h"
#include "toonz/pathanimations.h"

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

// SubLayer containing stroke sub layers for a particular Cell (e.g. B2)
class CellSubLayer final : public SubLayer {
  TXshCell m_cellId;
  bool m_subscribed;

public:
  CellSubLayer(SubLayers *subLayers, const TXshCell &cellId);

  virtual bool hasChildren() const override;
  virtual QString name() const;

  TXshCell cell() const { return m_cellId; }
  void subscribeStrokeListChanged();

protected slots:
  void onStrokeListChanged();
private:
  TVectorImageP vectorImage() const;
  SubLayer *build(TStroke *stroke);

  bool hasChild(const TStroke *stroke, vector<shared_ptr<SubLayer>>::const_iterator &child) const;
};

// SubLayer representing a single stroke
class StrokeSubLayer final : public SubLayer {
  CellSubLayer *m_parent;
  TStroke *m_stroke; // weak pointer - don't own

public:
  StrokeSubLayer(SubLayers *subLayers, CellSubLayer *parent, TStroke *stroke);

  virtual bool hasActivator() const override { return true; }
  virtual bool isActivated() const override;
  virtual void toggleActivator() override;

  virtual QString name() const override;

  bool owns(const TStroke *stroke) const { return m_stroke == stroke; }
private:
  StrokeId strokeId() const;
  TXshCell cell() const;
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

shared_ptr<SubLayer> SubLayers::cell(const TXshColumn *column, int frameNumber) {
  if (!column)
    return empty();
  return cell(CellPosition(frameNumber, column->getIndex()));
}

shared_ptr<SubLayer> SubLayers::cell(const CellPosition &pos) {
  optional<TXshCell> cellId = findCell(pos);
  if (!cellId)
    return empty();

  map<TXshCell, shared_ptr<SubLayer>>::iterator it = m_cellSubLayers.find(*cellId);
  if (it != m_cellSubLayers.end())
    return it->second;

  shared_ptr<SubLayer> newItem { build(*cellId) };
  m_cellSubLayers.insert(std::make_pair(*cellId, shared_ptr<SubLayer>(newItem)));
  return newItem;
}

optional<TXshCell> SubLayers::findCell(const CellPosition &pos) const {
  if (pos.layer() < 0)
    return boost::none;
  TXshColumn *column = m_mapper->xsheet()->getColumn(pos.layer());
  if (!column)
    return boost::none;
  TXshLevelColumn *levelColumn = column->getLevelColumn();
  if (!levelColumn)
    return boost::none;
  const TXshCell &cell = levelColumn->getCell(pos.frame());
  return cell;
}

SubLayer *SubLayers::build(const TXshCell &cellId) {
  return new CellSubLayer(this, cellId);
}
shared_ptr<SubLayer> SubLayers::empty() {
  return shared_ptr<SubLayer> (new EmptySubLayer(this));
}

void SubLayers::onFrameUpdated(const TXshCell &cellId) {
  map<TXshCell, shared_ptr<SubLayer>>::iterator it = m_cellSubLayers.find(cellId);
  if (it == m_cellSubLayers.end())
    return;
  CellSubLayer *subLayer = dynamic_cast<CellSubLayer *> (it->second.get());
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

TXsheet *SubLayer::xsheet() const {
  return subLayers()->screenMapper()->xsheet();
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
  TStageObject *columnObject = xsheet()->getStageObject(columnId);

  return QString::fromStdString(columnObject->getName());
}

// substitute with frame sublayer children
vector<shared_ptr<SubLayer>> LayerSubLayer::children() const {
  int frame = subLayers()->screenMapper()->getCurrentFrame();
  shared_ptr<SubLayer> cellSubLayer = subLayers()->cell(m_column, frame);
  return cellSubLayer->children();
}

//-----------------------------------------------------------------------------
// FrameSubLayer

CellSubLayer::CellSubLayer(SubLayers *subLayers, const TXshCell &cellId)
  : SubLayer(subLayers, nullptr), m_cellId(cellId), m_subscribed(false) {
  subscribeStrokeListChanged();
}

QString CellSubLayer::name() const {
  QString levelName = QString::fromStdWString (m_cellId.getSimpleLevel()->getName());
  return levelName + ' ' + QString::number(m_cellId.getFrameId().getNumber());
}

void CellSubLayer::subscribeStrokeListChanged() {
  if (m_subscribed)
    return;

  TVectorImageP image = vectorImage();
  if (!image) return;

  for (int i = 0; i < image->getStrokeCount(); i++)
    m_children.push_back(shared_ptr<StrokeSubLayer>(new StrokeSubLayer(subLayers(), this, image->getStroke(i))));

  connect(image.getPointer(), &TVectorImage::strokeListChanged, this, &CellSubLayer::onStrokeListChanged);
  m_subscribed = true;
}

bool CellSubLayer::hasChildren() const {
  TVectorImageP image = vectorImage();
  return image && image->getStrokeCount() != 0;
}

TVectorImageP CellSubLayer::vectorImage() const {
  TXshSimpleLevel *level = m_cellId.getSimpleLevel();
  if (!level) return nullptr;

  TImageP image = level->getFrame(m_cellId.getFrameId(), false);
  if (!image) return nullptr;

  TVectorImageP vectorImage = { dynamic_cast<TVectorImage *> (image.getPointer()) };
  return vectorImage;
}

void CellSubLayer::onStrokeListChanged() {
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

bool CellSubLayer::hasChild(const TStroke *stroke, vector<shared_ptr<SubLayer>>::const_iterator &child) const {
  for (child = m_children.begin(); child != m_children.end(); child++) {
    StrokeSubLayer *subLayer = dynamic_cast<StrokeSubLayer *> (child->get());
    if (subLayer->owns(stroke))
      return true;
  }
  return false;
}

SubLayer *CellSubLayer::build(TStroke *stroke) {
  return new StrokeSubLayer(subLayers(), this, stroke);
}

//-----------------------------------------------------------------------------
// StrokeSubLayer

StrokeSubLayer::StrokeSubLayer(SubLayers *subLayers, CellSubLayer *parent, TStroke *stroke)
  : SubLayer(subLayers, parent), m_parent(parent), m_stroke(stroke)
{ }

QString StrokeSubLayer::name() const {
  QString prefix = m_stroke->name();
  if (prefix.isEmpty())
    prefix = "Shape";
  return prefix + " " + QString::number(m_stroke->getId());
}

bool StrokeSubLayer::isActivated() const {
  shared_ptr<PathAnimation> animation = xsheet()->pathAnimations()->addStroke(strokeId());
  return animation->isActivated();
}

void StrokeSubLayer::toggleActivator() {
  shared_ptr<PathAnimation> animation = xsheet()->pathAnimations()->addStroke(strokeId());
  animation->toggleActivated();
  if (!animation->isActivated())
    return;
  animation->takeSnapshot(subLayers()->screenMapper()->getCurrentFrame());
}

StrokeId StrokeSubLayer::strokeId() const {
  return StrokeId(xsheet(), cell(), m_stroke);
}
TXshCell StrokeSubLayer::cell() const {
  return dynamic_cast<CellSubLayer *> (m_parent)->cell();
}