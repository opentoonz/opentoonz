#pragma once

#ifndef SUBLAYERS_INCLUDED
#define SUBLAYERS_INCLUDED

#include <map>
#include <memory>
#include <vector>

#include <boost/optional.hpp>

#include <QObject>
#include <QString>

#include "tfilepath.h"
#include "toonz/txshcell.h"
#include "toonz/txshcolumn.h"
#include "cellposition.h"

#include "tstroke.h"

using std::map;
using std::shared_ptr;
using std::vector;

using boost::optional;

class TXshSimpleLevel;

class ScreenMapper;
class Orientation;

class SubLayer;

//! Each XsheetViewer has one of these.
//! Keeps track of which layers are expanded / which ones are collapsed.
//! When a layer is expanded, each frame has a separate tree of subnodes.
//! Each node in the tree is called SubLayer.

class SubLayers final : public QObject {
  Q_OBJECT

  ScreenMapper *m_mapper;
  map<const TXshColumn *, shared_ptr<SubLayer>> m_layers;
  map<TXshCell, shared_ptr<SubLayer>> m_cellSubLayers;

public:
  SubLayers(ScreenMapper *mapper);
  ~SubLayers() {}

  ScreenMapper *screenMapper() const { return m_mapper; }

  shared_ptr<SubLayer> layer(const TXshColumn *column);
  shared_ptr<SubLayer> cell(const CellPosition &pos);
  shared_ptr<SubLayer> cell(const TXshColumn *column, int frame);

  vector<int> childrenDimensions(const Orientation *o);

private:
  optional<TXshCell> findCell(const CellPosition &pos) const;
  SubLayer *build(const TXshCell &cellId);
  shared_ptr<SubLayer> empty();

private slots:
  void onFrameUpdated(const TXshCell &cellId);
};

//! Basically, a tree node.
//! Knows whether it has children (and therefore is collapsible/expandable).
//! If so, has a collapsed/expanded state.
//! Leafs are keyframe controlled functions (known as TParams).
//! This object is only a means of presenting and manipulating underlying data.
//! Base interface.

class SubLayer : public QObject {
  Q_OBJECT

  //! reference to parent container
  SubLayers *m_subLayers;
  //! root has depth 0, its children have depth 1 and so on
  int m_depth;

protected:
  bool m_folded;
  //! direct descendants of this node
  vector<shared_ptr<SubLayer>> m_children;

public:
  SubLayer(SubLayers *subLayers, SubLayer *parent);
  virtual ~SubLayer() {}

  virtual bool hasChildren() const { return false; }
  virtual bool isFolded() const { return m_folded; }
  virtual void foldUnfold();

  virtual bool hasActivator() const { return false; }
  virtual bool isActivated() const { return false; }
  virtual void toggleActivator() {}
  virtual TStroke *getStroke() const { return 0; }

  virtual QString name() const { return ""; }

  SubLayers *subLayers() const { return m_subLayers; }
  TXsheet *xsheet() const;
  int depth() const { return m_depth; }

  int ownDimension(const Orientation *o) const;
  virtual int childrenDimension(const Orientation *o) const;

  virtual vector<shared_ptr<SubLayer>> children() const { return m_children; }

  //! list subnodes reachable by following expanded nodes
  vector<shared_ptr<SubLayer>> childrenFlatTree() const;

signals:
  void foldToggled();

private:
  SubLayer(const SubLayer &from) = delete;
  SubLayer &operator=(const SubLayer &from) = delete;
};

#endif
