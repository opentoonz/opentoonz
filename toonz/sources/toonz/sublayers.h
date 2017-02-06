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

  shared_ptr<SubLayer> get(const CellPosition &pos);
  shared_ptr<SubLayer> get(const TXshColumn *column);

  void foldUnfold(const TXshColumn *column);

  vector<int> childrenDimensions();

private:
  Level *findLevel(const CellPosition &pos);
  SubLayer *build(Level *level) const;
};

//! Basically, a tree node.
//! Knows whether it has children (and therefore is collapsible/expandable).
//! If so, has a collapsed/expanded state.
//! Leafs are keyframe controlled functions (known as TParams).
//! This object is only a means of presenting and manipulating underlying data.
//! Base interface.

class SubLayer : public QObject {
  Q_OBJECT

protected:
  vector<shared_ptr<SubLayer>> m_children;
public:

  SubLayer() { }
  virtual ~SubLayer() { }

  virtual bool hasChildren() const { return false; }
  virtual bool isFolded() const { return true; }
  virtual void foldUnfold() { }
  virtual bool hasActivator() const { return false; }
  virtual QString name() const { return ""; }

  int ownDimension() const;
  virtual int childrenDimension() const;

protected:
  vector<shared_ptr<SubLayer>> children() const { return m_children; }

private:
  SubLayer(const SubLayer &from) = delete;
  SubLayer &operator = (const SubLayer &from) = delete;
};

#endif
