#pragma once

#ifndef SUBLAYERS_INCLUDED
#define SUBLAYERS_INCLUDED

#include <map>
#include <memory>
#include <vector>

#include <QObject>
#include <QString>

#include "toonz/txshcolumn.h"
#include "cellposition.h"

using std::map;
using std::shared_ptr;
using std::vector;

class TXshSimpleLevel;

class ScreenMapper;
class Orientation;

class SubLayer;

//! Each XsheetViewer has one of these.
//! Keeps track of which layers are expanded / which ones are collapsed.
//! When a layer is expanded, it has a tree of descendants.
//! Each node in the tree is called SubLayer.

class SubLayers final {
  typedef TXshSimpleLevel Level;

  ScreenMapper *m_mapper;
  map<Level *, shared_ptr<SubLayer>> m_items;

public:

  SubLayers(ScreenMapper *mapper): m_mapper(mapper) { }
  ~SubLayers() { }

  ScreenMapper *screenMapper() const { return m_mapper; }

  shared_ptr<SubLayer> get(const CellPosition &pos);
  shared_ptr<SubLayer> get(const TXshColumn *column);

  void foldUnfold(const TXshColumn *column);

  vector<int> childrenDimensions(const Orientation *o);

private:
  Level *findLevel(const CellPosition &pos) const;
  SubLayer *build(Level *level);
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
  //! direct descendants of this node
  vector<shared_ptr<SubLayer>> m_children;
public:

  SubLayer(SubLayers *subLayers, SubLayer *parent);
  virtual ~SubLayer() { }

  virtual bool hasChildren() const { return false; }
  virtual bool isFolded() const { return true; }
  virtual void foldUnfold() { }
  virtual bool hasActivator() const { return false; }
  virtual QString name() const { return ""; }

  SubLayers *subLayers() const { return m_subLayers; }
  int depth() const { return m_depth; }

  int ownDimension(const Orientation *o) const;
  virtual int childrenDimension(const Orientation *o) const;

  //! list subnodes reachable by following expanded nodes
  vector<shared_ptr<SubLayer>> childrenFlatTree() const;

protected:
  vector<shared_ptr<SubLayer>> children() const { return m_children; }

private:
  SubLayer(const SubLayer &from) = delete;
  SubLayer &operator = (const SubLayer &from) = delete;
};

#endif
