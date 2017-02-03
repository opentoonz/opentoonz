#include "sublayers.h"

// pretends to be a collapsible / expandable sub layer
class FakeSubLayer : public SubLayer {
  bool m_folded;

public:
  FakeSubLayer() : m_folded(true) { }

  virtual bool hasChildren() const override { return true; }
  virtual bool isFolded() const override { return m_folded; }
  virtual void foldUnfold() override { m_folded = !m_folded; }
};

// sub layer indicating that there is no sub layer
// special case pattern
class EmptySubLayer : public SubLayer {
};

//-----------------------------------------------------------------------------

SubLayers::SubLayers(XsheetViewer *viewer): m_viewer (viewer) {
}

SubLayers::~SubLayers() {
}

SubLayer *SubLayers::get(TXshColumn *column) {
  map<TXshColumn *, unique_ptr<SubLayer>>::iterator it = m_items.find(column);
  if (it != m_items.end ())
    return it->second.get();

  SubLayer *newItem = build(column);
  m_items.insert(std::make_pair(column, unique_ptr<SubLayer>(newItem)));
  return newItem;
}

SubLayer *SubLayers::build(TXshColumn *column) const {
  if (!isGood(column))
    return new EmptySubLayer();

  return new FakeSubLayer();
}

bool SubLayers::isGood(TXshColumn *column) const {
  return column->getLevelColumn() != nullptr;
}
