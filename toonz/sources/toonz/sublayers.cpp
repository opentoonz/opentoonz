#include "sublayers.h"

#include "toonz/screenmapper.h"
#include "toonz/txshcell.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txsheet.h"

#include "tstroke.h"
#include "tvectorimage.h"
#include "../../common/tvectorimage/tvectorimageP.h"
#include "xsheetviewer.h"

// sub layer indicating that there is no sub layer
// special case pattern
class EmptySubLayer final : public SubLayer {
};

// Contains multiple sublayers, each is a TStroke
class SimpleLevelSubLayer final : public SubLayer {
  TXshSimpleLevelP m_level;
  bool m_folded;

public:
  SimpleLevelSubLayer(TXshSimpleLevelP level);
  ~SimpleLevelSubLayer() { }

  virtual bool hasChildren() const override;
  virtual bool isFolded() const override { return m_folded; }
  virtual void foldUnfold() override { m_folded = !m_folded; }
  virtual QString name() const { return QString::fromStdWString (m_level->getName()); }

protected slots:
  void afterStrokeListChanged();
private:
  TVectorImageP vectorImage() const;
  SubLayer *build(TStroke *stroke) const;

  bool hasChild(const TStroke *stroke, vector<shared_ptr<SubLayer>>::const_iterator &child) const;
};

// SubLayer representing a single stroke
class StrokeSubLayer final : public SubLayer {
  TStroke *m_stroke; // weak pointer - don't own

public:
  StrokeSubLayer(TStroke *stroke) : m_stroke(stroke) { }

  virtual bool hasActivator() const override { return true; }
  virtual QString name() const override { return QString::number(m_stroke->getId ()); }

  bool owns(const TStroke *stroke) const { return m_stroke == stroke; }
};

//-----------------------------------------------------------------------------
// SubLayers

shared_ptr<SubLayer> SubLayers::get(const TXshColumn *column) {
  if (!column)
    return shared_ptr<SubLayer>(new EmptySubLayer());
  return get(CellPosition(m_mapper->getCurrentFrame (), column->getIndex ()));
}
shared_ptr<SubLayer> SubLayers::get(const CellPosition &pos) {
  Level *level = findLevel(pos);
  if (!level)
    return shared_ptr<SubLayer>(new EmptySubLayer());

  map<Level *, shared_ptr<SubLayer>>::iterator it = m_items.find(level);
  if (it != m_items.end())
    return it->second;

  shared_ptr<SubLayer> newItem { build(level) };
  m_items.insert(std::make_pair(level, shared_ptr<SubLayer>(newItem)));
  return newItem;
}

SubLayers::Level *SubLayers::findLevel(const CellPosition &pos) {
  if (pos.layer() < 0)
    return nullptr;
  TXshColumn *column = m_mapper->xsheet ()->getColumn(pos.layer());
  if (!column)
    return nullptr;
  TXshLevelColumn *levelColumn = column->getLevelColumn();
  if (!levelColumn)
    return nullptr;
  const TXshCell &cell = levelColumn->getCell(pos.frame());
  return cell.getSimpleLevel();
}

SubLayer *SubLayers::build(Level *level) const {
  return new SimpleLevelSubLayer(level);
}

//-----------------------------------------------------------------------------

vector<int> SubLayers::childrenDimensions() {
  vector<int> result;
  TXsheet *xsheet = m_mapper->xsheet();
  ColumnFan *fan = xsheet->getColumnFan();
  int count = xsheet->getColumnCount();

  for (int i = 0; i < count; i++) {
    const TXshColumn *column = xsheet->getColumn(i);
    shared_ptr<SubLayer> subLayer = get(column);
    result.push_back(subLayer->childrenDimension());
  }
  return result;
}

void SubLayers::foldUnfold(const TXshColumn *column) {
  shared_ptr<SubLayer> subLayer = get(column);
  subLayer->foldUnfold();
  m_mapper->updateColumnFan();
}

//-----------------------------------------------------------------------------
// SubLayer

namespace {
  const int SUBLAYER_SIZE = 20;
}

int SubLayer::ownDimension() const {
  return SUBLAYER_SIZE;
}

int SubLayer::childrenDimension() const {
  if (isFolded())
    return 0;
  int sum = 0;
  for (shared_ptr<SubLayer> child : children())
    sum += child->ownDimension() + child->childrenDimension();
  return sum;
}

//-----------------------------------------------------------------------------

SimpleLevelSubLayer::SimpleLevelSubLayer(TXshSimpleLevelP level)
  : m_level(level), m_folded(true) {
  TVectorImageP image = vectorImage();
  
  for (int i = 0; i < image->getStrokeCount(); i++)
    m_children.push_back(shared_ptr<StrokeSubLayer> (new StrokeSubLayer(image->getStroke(i))));

  connect(image.getPointer (), &TVectorImage::strokeListChanged, this, &SimpleLevelSubLayer::afterStrokeListChanged);
}

bool SimpleLevelSubLayer::hasChildren() const {
  return vectorImage ()->getStrokeCount() != 0;
}

TVectorImageP SimpleLevelSubLayer::vectorImage() const {
  TFrameId fid = { m_level->getFirstFid() };
  TImageP image = m_level->getFrame(fid, false);
  if (!image) return nullptr;
  TVectorImageP vectorImage = { dynamic_cast<TVectorImage *> (image.getPointer()) };
  return vectorImage;
}

void SimpleLevelSubLayer::afterStrokeListChanged() {
  TVectorImageP image = vectorImage();
  vector<shared_ptr<SubLayer>> newList;

  QMutexLocker sl(image->getMutex ());

  for (UINT i = 0; i < image->getStrokeCount(); i++) {
    VIStroke *stroke = image->getVIStroke(i);

    vector<shared_ptr<SubLayer>>::const_iterator child;
    if (hasChild(stroke->m_s, child))
      newList.push_back(*child);
    else
      newList.push_back(shared_ptr<SubLayer> (build(stroke->m_s)));
  }
  m_children = newList;
}

bool SimpleLevelSubLayer::hasChild(const TStroke *stroke, vector<shared_ptr<SubLayer>>::const_iterator &child) const {
  for (child = m_children.begin(); child != m_children.end(); child++) {
    StrokeSubLayer *subLayer = dynamic_cast<StrokeSubLayer *> (child->get());
    if (subLayer->owns(stroke))
      return true;
  }
  return false;
}

SubLayer *SimpleLevelSubLayer::build(TStroke *stroke) const {
  return new StrokeSubLayer(stroke);
}

//-----------------------------------------------------------------------------
