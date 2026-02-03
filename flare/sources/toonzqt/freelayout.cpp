

#include "toonzqt/freelayout.h"

#include <memory>

//************************************************************************************
//    DummyLayout   implementation
//************************************************************************************

DummyLayout::DummyLayout() { setSizeConstraint(QLayout::SetNoConstraint); }

//---------------------------------------------------------------------------

DummyLayout::~DummyLayout() {
  std::for_each(m_items.begin(), m_items.end(), std::default_delete<QLayoutItem>());
}

//---------------------------------------------------------------------------

QSize DummyLayout::sizeHint() const {
  QRect geom, result;

  QList<QLayoutItem *>::const_iterator it, iEnd = m_items.end();
  for (it = m_items.begin(); it != iEnd; ++it) {
    QLayoutItem *item = *it;

    geom = item->geometry();
    geom.setSize(item->sizeHint());

    result |= geom;
  }

  return result.size();
}

//************************************************************************************
//    FreeLayout   implementation
//************************************************************************************

void FreeLayout::setGeometry(const QRect &r) {
  QList<QLayoutItem *>::const_iterator it, iEnd = m_items.end();
  for (it = m_items.begin(); it != iEnd; ++it) {
    QLayoutItem *item = *it;

    const QRect &geom     = item->geometry();
    const QSize &sizeHint = item->sizeHint();

    if (geom.size() != sizeHint)
      item->setGeometry(QRect(geom.topLeft(), sizeHint));
  }
}
