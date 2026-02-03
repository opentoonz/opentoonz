#pragma once

#ifndef FREE_LAYOUT_H
#define FREE_LAYOUT_H

// TnzCore includes
#include "tcommon.h"

// Qt includes
#include <QList>
#include <QLayout>
#include <QWidget>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//************************************************************************************
//    DummyLayout   declaration
//************************************************************************************

/*!
  \brief    The DummyLayout class implements a layout whose items' geometry is
  not
            enforced at all, ignoring parent size bounds or geometry.

  \details  A DummyLayout serves as a simple, inert container of layout items.
            It does not enforce geometry to items added to it, allowing objects
            using this layout to appear as a 'viewport' for free-geometry items.

  \note     As this layout does not effectively manage its objects' geometry,
  these
            must be managed outside.
*/

class DVAPI DummyLayout : public QLayout {
public:
  DummyLayout();
  virtual ~DummyLayout();

  QSize sizeHint() const override;

  QSize minimumSize() const override { return QSize(0, 0); }
  QSize maximumSize() const override {
    return QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
  }

  int count() const override { return m_items.count(); }
  void addItem(QLayoutItem *item) override { m_items.push_back(item); }
  QLayoutItem *itemAt(int index) const override {
    return (index < count()) ? m_items.at(index) : 0;
  }
  QLayoutItem *takeAt(int index) override {
    return (index < count()) ? m_items.takeAt(index) : 0;
  }

  void setGeometry(const QRect &r) override {}
  // Qt::Orientations	expandingDirections () const { return
  // Qt::Orientations(); }

protected:
  QList<QLayoutItem *> m_items;
};

//************************************************************************************
//    FreeLayout   declaration
//************************************************************************************

/*!
  \brief    The FreeLayout class implements a layout whose items can be freely
            positioned, ignoring parent size bounds or geometry.

  \details  A FreeLayout instance can be used to achieve results similar to that
            of a QScrollArea, with a more straightforward API and without
  restriction
            to just one item at a time. Please observe that layouts can be
  inserted too.

            Specifically, FreeLayout is a convenience subclass of DummyLayout
  whose items'
            sizes are enforced to their sizeHint() - in order to spare users
  from having to deal
            with it externally. This complies with the idea that its items are
  comfortly
            free to adapt to their preferred size, as it would be in a
  scrollable environment.
*/

class FreeLayout final : public DummyLayout {
public:
  FreeLayout() : DummyLayout() {}
  ~FreeLayout() {}

  void setGeometry(const QRect &r) override;
};

#endif  // FREE_LAYOUT_H
