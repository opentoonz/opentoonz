#pragma once

#ifndef SUBLAYERS_INCLUDED
#define SUBLAYERS_INCLUDED

#include <map>
#include <memory>

#include "toonz/txshcolumn.h"

using std::map;
using std::unique_ptr;

class XsheetViewer;

class SubLayer;

//! Each XsheetViewer has one of these.
//! Keeps track of which layers are expanded / which ones are collapsed.
//! When a layer is expanded, it has a tree of descendants.
//! Each node in the tree is called SubLayer.

class SubLayers {
  XsheetViewer *m_viewer;
  map<TXshColumn *, unique_ptr<SubLayer>> m_items;
public:

  SubLayers(XsheetViewer *viewer);
  ~SubLayers();

  SubLayer *get(TXshColumn *column);

private:
  bool isGood(TXshColumn *column) const;
  SubLayer *build(TXshColumn *column) const;
};

//! Basically, a tree node.
//! Knows whether it has children (and therefore is collapsible/expandable).
//! If so, has a collapsed/expanded state.
//! Leafs are keyframe controlled functions (known as TParams).
//! This object is only a means of presenting and manipulating underlying data.
//! Base interface.

class SubLayer {
public:

  SubLayer() { }
  virtual ~SubLayer() { }

  virtual bool hasChildren() const { return false; }
  virtual bool isFolded() const { return true; }
  virtual void foldUnfold() { }
};

#endif
