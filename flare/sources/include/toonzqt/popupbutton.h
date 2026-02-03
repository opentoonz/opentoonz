#pragma once

#ifndef POPUP_BUTTON_H
#define POPUP_BUTTON_H

// DVAPI includes
#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// Qt includes
#include <QPushButton>
#include <QString>
#include <QIcon>
#include <QList>

// Forward declarations
class QAction;

//********************************************************************************
//    PopupButton class
//********************************************************************************

/*!
  The PopupButton represents an enum type in a compact, popup-based form.
  A PopupButton instance at rest is a simple push button showing an icon
  which represents the stored enum. As the user clicks on the button, a
  popup appears showing a description and, optionally, an icon of the
  selectable enum values.
*/

class DVAPI PopupButton : public QPushButton {
  Q_OBJECT

  QList<QAction *> m_actions;
  int m_index;

public:
  PopupButton(QWidget *parent = 0);

  int currentIndex() const;
  const QAction *currentItem() const;
  int count() const;

  QAction *addItem(const QString &text);
  QAction *addItem(const QIcon &icon, const QString &text = QString());
  QAction *addSeparator();

  int findText(const QString &text,
               Qt::CaseSensitivity cs = Qt::CaseSensitive) const;

signals:

  void activated(int index);
  void currentIndexChanged(int index);

public slots:

  void setCurrentIndex(int index);

private slots:

  void onIndexChange();
};

#endif  // POPUP_BUTTON_H
