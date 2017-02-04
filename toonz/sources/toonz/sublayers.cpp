#include "sublayers.h"

#include "toonz\txshcell.h"
#include "toonz\txshlevelcolumn.h"
#include "toonz\txshsimplelevel.h"

#include "tvectorimage.h"
#include "xsheetviewer.h"

class SubLayers::Imp {
  typedef TXshSimpleLevel Level;

  XsheetViewer *m_viewer;
  map<Level *, shared_ptr<SubLayer>> m_items;

public:
  Imp(XsheetViewer *viewer);
  ~Imp() { }

  shared_ptr<SubLayer> get(const CellPosition &pos);
  shared_ptr<SubLayer> get(const TXshColumn *column);

private:

  class LocateLevel {
    XsheetViewer *m_viewer;
    CellPosition m_pos;
    bool m_success;
    Level *m_level;
  public:
    LocateLevel(XsheetViewer *viewer, const CellPosition &pos);

    operator bool() const { return m_success; }
    Level *level() const { return m_level; }
  };

  SubLayer *build(Level *level) const;
};

// Contains multiple sublayers, each is a TStroke
class SimpleLevelSubLayer : public SubLayer {
  TXshSimpleLevel *m_level;
  bool m_folded;

public:
  SimpleLevelSubLayer(TXshSimpleLevel *level): m_level (level), m_folded(true) { }

  virtual bool hasChildren() const override;
  virtual bool isFolded() const override { return m_folded; }
  virtual void foldUnfold() override { m_folded = !m_folded; }

private:
  TVectorImageP vectorImage() const;
};

// sub layer indicating that there is no sub layer
// special case pattern
class EmptySubLayer : public SubLayer {
};

//-----------------------------------------------------------------------------

SubLayers::Imp::LocateLevel::LocateLevel(XsheetViewer *viewer, const CellPosition &pos)
  : m_viewer (viewer), m_pos (pos), m_success (false) {
  if (pos.layer() < 0)
    return;
  TXshColumn *column = m_viewer->getXsheet()->getColumn(pos.layer());
  if (!column)
    return;
  TXshLevelColumn *levelColumn = column->getLevelColumn();
  if (!levelColumn)
    return;
  const TXshCell &cell = levelColumn->getCell(pos.frame());
  m_level = cell.getSimpleLevel();
  if (!m_level)
    return;

  m_success = true;
}

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
  LocateLevel locate(m_viewer, pos);
  if (!locate)
    return shared_ptr<SubLayer>(new EmptySubLayer());
  Level *level = locate.level();

  map<Level *, shared_ptr<SubLayer>>::iterator it = m_items.find(level);
  if (it != m_items.end())
    return it->second;

  shared_ptr<SubLayer> newItem { build(level) };
  m_items.insert(std::make_pair(level, shared_ptr<SubLayer>(newItem)));
  return newItem;
}

SubLayer *SubLayers::Imp::build(Level *level) const {
  return new SimpleLevelSubLayer(level);
}

//-----------------------------------------------------------------------------

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

