#include "sublayers.h"

#include "toonz\txshcell.h"
#include "toonz\txshlevelcolumn.h"
#include "toonz\txshsimplelevel.h"

#include "tstroke.h"
#include "tvectorimage.h"
#include "xsheetviewer.h"

class SubLayers::Imp final {
  typedef TXshSimpleLevel Level;

  XsheetViewer *m_viewer;
  map<Level *, shared_ptr<SubLayer>> m_items;

public:
  Imp(XsheetViewer *viewer);
  ~Imp() { }

  shared_ptr<SubLayer> get(const CellPosition &pos);
  shared_ptr<SubLayer> get(const TXshColumn *column);

private:
  Level *findLevel(const CellPosition &pos);
  SubLayer *build(Level *level) const;
};

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
  void updateStrokeAdded(TStroke *stroke);
  void updateStrokeRemoved(TStroke *stroke);
private:
  TVectorImageP vectorImage() const;
  SubLayer *build(TStroke *stroke) const;
};

class StrokeSubLayer final : public SubLayer {
  TStroke *m_stroke; // weak pointer - don't own

public:
  StrokeSubLayer(TStroke *stroke) : m_stroke(stroke) { }

  virtual bool hasActivator() const override { return true; }
  virtual QString name() const override { return QString::number(m_stroke->getId ()); }
};

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

SubLayers::SubLayers(XsheetViewer *viewer): imp (nullptr) {
  imp = new Imp(viewer);
}

SubLayers::~SubLayers() {
  delete imp;
}

shared_ptr<SubLayer> SubLayers::get(const CellPosition &pos) {
  return imp->get(pos);
}
shared_ptr<SubLayer> SubLayers::get(const TXshColumn *column) {
  return imp->get(column);
}

//-----------------------------------------------------------------------------

SubLayers::Imp::Imp(XsheetViewer *viewer): m_viewer (viewer) {
}

shared_ptr<SubLayer> SubLayers::Imp::get(const TXshColumn *column) {
  return get(CellPosition(m_viewer->getCurrentRow (), column->getIndex ()));
}
shared_ptr<SubLayer> SubLayers::Imp::get(const CellPosition &pos) {
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

SubLayers::Imp::Level *SubLayers::Imp::findLevel(const CellPosition &pos) {
  if (pos.layer() < 0)
    return nullptr;
  TXshColumn *column = m_viewer->getXsheet()->getColumn(pos.layer());
  if (!column)
    return nullptr;
  TXshLevelColumn *levelColumn = column->getLevelColumn();
  if (!levelColumn)
    return nullptr;
  const TXshCell &cell = levelColumn->getCell(pos.frame());
  return cell.getSimpleLevel();
}

SubLayer *SubLayers::Imp::build(Level *level) const {
  return new SimpleLevelSubLayer(level);
}

//-----------------------------------------------------------------------------

SimpleLevelSubLayer::SimpleLevelSubLayer(TXshSimpleLevelP level)
  : m_level(level), m_folded(true) {
  TVectorImageP image = vectorImage();
  
  for (int i = 0; i < image->getStrokeCount(); i++)
    m_children.push_back(shared_ptr<StrokeSubLayer> (new StrokeSubLayer(image->getStroke(i))));

  connect(image.getPointer (), &TVectorImage::strokeAdded, this, &SimpleLevelSubLayer::updateStrokeAdded);
  connect(image.getPointer (), &TVectorImage::strokeRemoved, this, &SimpleLevelSubLayer::updateStrokeRemoved);
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

void SimpleLevelSubLayer::updateStrokeAdded(TStroke *stroke) {
  TVectorImageP image = vectorImage();
  int index = image->getStrokeIndex(stroke);
  
  vector<shared_ptr<SubLayer>>::iterator it;
  for (it = m_children.begin(); index >= 0 && it < m_children.end(); it++, index--);
  m_children.insert(it, shared_ptr<SubLayer> (build (stroke)));
}

void SimpleLevelSubLayer::updateStrokeRemoved(TStroke *stroke) {
}

SubLayer *SimpleLevelSubLayer::build(TStroke *stroke) const {
  return new StrokeSubLayer(stroke);
}

//-----------------------------------------------------------------------------
