#include "navtageditorpopup.h"

#include "../toonz/tapp.h"

#include <QPushButton>
#include <QMainWindow>
#include <QColor>

namespace {
const QIcon getColorChipIcon(QColor color) {
  QPixmap pixmap(12, 12);
  pixmap.fill(color);
  return QIcon(pixmap);
}
}

void NavTagEditorPopup::accept() {
  m_label = QString(m_labelFld->text());

  Dialog::accept();
}

NavTagEditorPopup::NavTagEditorPopup(int frame, QString label, QColor color)
    : Dialog(TApp::instance()->getMainWindow(), true, true, "Edit Tag")
    , m_label(label)
    , m_color(color) {
  bool ret = true;

  setWindowTitle(tr("Edit Tag"));

  m_labelFld = new DVGui::LineEdit(m_label);
  m_labelFld->setMaximumHeight(DVGui::WidgetHeight);
  addWidget(tr("Frame %1 Label:").arg(frame + 1), m_labelFld);

  m_colorCB = new QComboBox(this);
  m_colorCB->addItem(getColorChipIcon(Qt::magenta), tr("Magenta"),
                     TagColors::Magenta);
  m_colorCB->addItem(getColorChipIcon(Qt::red), tr("Red"), TagColors::Red);
  m_colorCB->addItem(getColorChipIcon(Qt::green), tr("Green"),
                     TagColors::Green);
  m_colorCB->addItem(getColorChipIcon(Qt::blue), tr("Blue"), TagColors::Blue);
  m_colorCB->addItem(getColorChipIcon(Qt::yellow), tr("Yellow"),
                     TagColors::Yellow);
  m_colorCB->addItem(getColorChipIcon(Qt::cyan), tr("Cyan"), TagColors::Cyan);
  m_colorCB->addItem(getColorChipIcon(Qt::white), tr("White"),
                     TagColors::White);
  m_colorCB->addItem(getColorChipIcon(Qt::darkMagenta), tr("Dark Magenta"),
                     TagColors::DarkMagenta);
  m_colorCB->addItem(getColorChipIcon(Qt::darkRed), tr("Dark Red"),
                     TagColors::DarkRed);
  m_colorCB->addItem(getColorChipIcon(Qt::darkGreen), tr("Dark Green"),
                     TagColors::DarkGreen);
  m_colorCB->addItem(getColorChipIcon(Qt::darkBlue), tr("Dark Blue"),
                     TagColors::DarkBlue);
  m_colorCB->addItem(getColorChipIcon(Qt::darkYellow), tr("Dark Yellow"),
                     TagColors::DarkYellow);
  m_colorCB->addItem(getColorChipIcon(Qt::darkCyan), tr("Dark Cyan"),
                     TagColors::DarkCyan);
  m_colorCB->addItem(getColorChipIcon(Qt::darkGray), tr("Dark Gray"),
                     TagColors::DarkGray);

  addWidget(tr("Color:"), m_colorCB);

  if (color == Qt::magenta)
    m_colorCB->setCurrentIndex(TagColors::Magenta);
  else if (color == Qt::red)
    m_colorCB->setCurrentIndex(TagColors::Red);
  else if (color == Qt::green)
    m_colorCB->setCurrentIndex(TagColors::Green);
  else if (color == Qt::blue)
    m_colorCB->setCurrentIndex(TagColors::Blue);
  else if (color == Qt::yellow)
    m_colorCB->setCurrentIndex(TagColors::Yellow);
  else if (color == Qt::cyan)
    m_colorCB->setCurrentIndex(TagColors::Cyan);
  else if (color == Qt::white)
    m_colorCB->setCurrentIndex(TagColors::White);
  else if (color == Qt::darkMagenta)
    m_colorCB->setCurrentIndex(TagColors::DarkMagenta);
  else if (color == Qt::darkRed)
    m_colorCB->setCurrentIndex(TagColors::DarkRed);
  else if (color == Qt::darkGreen)
    m_colorCB->setCurrentIndex(TagColors::DarkGreen);
  else if (color == Qt::darkBlue)
    m_colorCB->setCurrentIndex(TagColors::DarkBlue);
  else if (color == Qt::darkYellow)
    m_colorCB->setCurrentIndex(TagColors::DarkYellow);
  else if (color == Qt::darkCyan)
    m_colorCB->setCurrentIndex(TagColors::DarkCyan);
  else if (color == Qt::darkGray)
    m_colorCB->setCurrentIndex(TagColors::DarkGray);

  ret = ret &&
        connect(m_labelFld, SIGNAL(editingFinished()), SLOT(onLabelChanged()));
  ret = ret &&
        connect(m_colorCB, SIGNAL(activated(int)), SLOT(onColorChanged(int)));

  QPushButton *okBtn = new QPushButton(tr("Ok"), this);
  okBtn->setDefault(true);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
  connect(okBtn, SIGNAL(clicked()), this, SLOT(accept()));
  connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(okBtn, cancelBtn);
}

void NavTagEditorPopup::onColorChanged(int index) {
  QColor color;

  switch (index) {
  case TagColors::Magenta:
  default:
    color = Qt::magenta;
    break;
  case TagColors::Red:
    color = Qt::red;
    break;
  case TagColors::Green:
    color = Qt::green;
    break;
  case TagColors::Blue:
    color = Qt::blue;
    break;
  case TagColors::Yellow:
    color = Qt::yellow;
    break;
  case TagColors::Cyan:
    color = Qt::cyan;
    break;
  case TagColors::White:
    color = Qt::white;
    break;
  case TagColors::DarkMagenta:
    color = Qt::darkMagenta;
    break;
  case TagColors::DarkRed:
    color = Qt::darkRed;
    break;
  case TagColors::DarkGreen:
    color = Qt::darkGreen;
    break;
  case TagColors::DarkBlue:
    color = Qt::darkBlue;
    break;
  case TagColors::DarkYellow:
    color = Qt::darkYellow;
    break;
  case TagColors::DarkCyan:
    color = Qt::darkCyan;
    break;
  case TagColors::DarkGray:
    color = Qt::darkGray;
    break;
  }

  m_color = color;
}
