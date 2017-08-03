

#include "toonzqt/dvdialog.h"

// TnzQt includes
#include "toonzqt/checkbox.h"
#include "toonzqt/lineedit.h"
#include "toonzqt/fxsettings.h"

// TnzLib includes
#include "toonz/txsheethandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/palettecmd.h"
#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"

// Qt includes
#include <QFrame>
#include <QLayout>
#include <QLabel>
#include <QPushButton>
#include <QApplication>
#include <QStyle>
#include <QButtonGroup>
#include <QPainter>
#include <QRadioButton>
#include <QThread>
#include <QDesktopWidget>
#include <QCheckBox>

// boost includes
#include <boost/algorithm/cxx11/any_of.hpp>

using namespace DVGui;

QString DialogTitle = QObject::tr("OpenToonz 1.1");

//=============================================================================
namespace {
QPixmap getMsgBoxPixmap(MsgType type) {
  int iconSize =
      QApplication::style()->pixelMetric(QStyle::PM_MessageBoxIconSize);
  QIcon msgBoxIcon;

  switch (type) {
  case DVGui::INFORMATION:
    msgBoxIcon =
        QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation);
    break;
  case DVGui::WARNING:
    msgBoxIcon =
        QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
    break;
  case DVGui::CRITICAL:
    msgBoxIcon =
        QApplication::style()->standardIcon(QStyle::SP_MessageBoxCritical);
    break;
  case DVGui::QUESTION:
    msgBoxIcon =
        QApplication::style()->standardIcon(QStyle::SP_MessageBoxQuestion);
    break;
  default:
    break;
  }

  if (!msgBoxIcon.isNull()) return msgBoxIcon.pixmap(iconSize, iconSize);
  return QPixmap();
}

//-----------------------------------------------------------------------------

QString getMsgBoxTitle(MsgType type) {
  QString title = DialogTitle + " - ";

  switch (type) {
  case DVGui::INFORMATION:
    title.append(QObject::tr("Information"));
    break;
  case DVGui::WARNING:
    title.append(QObject::tr("Warning"));
    break;
  case DVGui::CRITICAL:
    title.append(QObject::tr("Critical"));
    break;
  case DVGui::QUESTION:
    title.append(QObject::tr("Question"));
    break;
  default:
    break;
  }
  return title;
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// Separator
//-----------------------------------------------------------------------------

Separator::Separator(QString name, QWidget *parent)
    : QFrame(parent), m_name(name), m_isHorizontal(true) {
  //	if(m_name.isEmpty())
  //		setMinimumSize(1,1);
  //	else
  setMinimumSize(1, 15);
}

//-----------------------------------------------------------------------------

Separator::~Separator() {}

//-----------------------------------------------------------------------------

void Separator::paintEvent(QPaintEvent *) {
  QPainter p(this);
  ParamsPage *page = dynamic_cast<ParamsPage *>(parentWidget());
  if (!page)
    p.setPen(palette().alternateBase().color());
  else
    p.setPen(page->getTextColor());

  QRect contents(contentsRect());

  int textWidth = p.fontMetrics().width(m_name);

  p.drawText(contents.left(), 10, m_name);

  // make the line semi-transparent
  QColor lineColor = palette().alternateBase().color();
  lineColor.setAlpha(128);

  p.setPen(lineColor);
  int h = contents.height();
  if (m_isHorizontal) {
    int y     = contents.center().y();
    int space = (textWidth == 0) ? 0 : 8;
    p.drawLine(textWidth + space, y, contents.width(), y);
  } else {
    double w       = width();
    int space      = (textWidth == 0) ? 0 : 2;
    int x0         = (w > textWidth) ? w * 0.5 : (double)textWidth * 0.5;
    int textHeghit = (space == 0) ? 0 : p.fontMetrics().height();
    p.drawLine(x0, textHeghit + space, x0, contents.bottom());
  }
}

//=============================================================================
/*! \class DVGui::Dialog
                \brief The Dialog class is the base class of dialog windows.

                Inherits \b QDialog.

                A dialog window is a top-level window, defined in \b QDialog,
this class provides
                a collection of functions to manage \b QDialog widget
visualization.
                Dialog window can be modal or modeless, from this it depends
dialog visualization.
                You set your dialog to modal or modeless directly in
constructor, with \b bool
                hasButton, in fact if the dialog has button then is modal too.

                If dialog is \b modal it's composed of two part, two \b QFrame;
the first, \b main
                \b part at top of dialog, contains a vertical box layout \b
QVBoxLayout, the second,
                \b button \b part at bottom, contains an horizontal box layout
\b QHBoxLayout.
                Else if dialog is modeless it is composed only of first part, to
be more precise,
                it is composed of a \b QFrame containing a vertical box layout
\b QVBoxLayout.

                The first part of dialog can be implemented using different
objects. You can manage
                directly vertical box layout, insert widget addWidget(), or
layout addLayout(), set
                border and spacing, using setTopMargin() and setTopSpacing(), or
insert a spece,
                addSpacing().
\n	If you need an horizontal layout you can use an implemented horizontal
box layout;
                the function beginHLayout() initialize the horizontal box
layout, a collection of
                function permit to insert object in this layout; at the end you
must recall
                endVLayout(), this add horizontal layout to first part of
dialog.
                This struct permit you to allign the object you insert.
\n	If you need vertical layout you can use an implemented vertical box
layout composed
                of two column, the function beginVLayout() initialize the
vertical box layout,
                a collection of function permit to insert object in this layout.
                All this functions insert a pair of object in two vertical
layout and permit
                to allign the objects, all pairs you insert are tabulated.
                At the end you must recall endVLayout(), this add vertical
layout to first part of
                dialog.

                The second part of dialog may be implemented if dialog is modal.
                It's possible insert one, two or three widget in its horizontal
box layout using
                addButtonBarWidget(), or clear all widget using
clearButtonBar(). You can also
                set margin and spacing with setButtonBarMargin() and
setButtonBarSpacing() functions.
*/
//-----------------------------------------------------------------------------
QSettings *Dialog::m_settings = 0;

Dialog::Dialog(QWidget *parent, bool hasButton, bool hasFixedSize,
               const QString &name)
    : QDialog(parent, Qt::CustomizeWindowHint | Qt::WindowTitleHint |
                          Qt::WindowCloseButtonHint)
    , m_hasButton(hasButton)
    , m_mainHLayout(0)
    , m_leftVLayout(0)
    , m_rightVLayout(0)
    , m_isMainVLayout(false)
    , m_isMainHLayout(false)
    , m_layoutSpacing(5)
    , m_layoutMargin(0)
    , m_labelWidth(100)
    , m_name() {
  QVBoxLayout *mainLayout = new QVBoxLayout;
  mainLayout->setMargin(0);
  mainLayout->setSpacing(0);
  m_mainFrame = new QFrame(this);
  m_mainFrame->setObjectName("dialogMainFrame");
  m_mainFrame->setMinimumHeight(41);
  m_mainFrame->setFrameStyle(QFrame::StyledPanel);
  m_topLayout = new QVBoxLayout;
  m_topLayout->setMargin(12);
  m_topLayout->setSpacing(m_layoutSpacing);
  m_topLayout->setAlignment(Qt::AlignCenter);
  m_mainFrame->setLayout(m_topLayout);

  mainLayout->addWidget(m_mainFrame);

  if (m_hasButton) {
    // The dialog is modal
    setModal(true);

    m_buttonFrame = new QFrame(this);
    m_buttonFrame->setObjectName("dialogButtonFrame");
    m_buttonFrame->setFrameStyle(QFrame::StyledPanel);
    m_buttonFrame->setFixedHeight(45);

    m_buttonLayout = new QHBoxLayout;
    m_buttonLayout->setMargin(0);
    m_buttonLayout->setSpacing(20);
    m_buttonLayout->setAlignment(Qt::AlignHCenter);

    QVBoxLayout *buttonFrameLayout = new QVBoxLayout;
    buttonFrameLayout->setAlignment(Qt::AlignVCenter);
    buttonFrameLayout->addLayout(m_buttonLayout);
    m_buttonFrame->setLayout(buttonFrameLayout);

    mainLayout->addWidget(m_buttonFrame);
  }

  if (hasFixedSize)
    mainLayout->setSizeConstraint(QLayout::SetFixedSize);
  else
    mainLayout->setSizeConstraint(QLayout::SetMinimumSize);

  setLayout(mainLayout);
// this->setGeometry(30, 30, 300, 300);
#ifdef MACOSX
  setWindowFlags(Qt::Tool);
#endif

  if (!m_settings) {
    TFilePath layoutDir = ToonzFolder::getMyModuleDir();
    TFilePath savePath  = layoutDir + TFilePath("popups.ini");
    m_settings =
        new QSettings(QString::fromStdWString(savePath.getWideString()),
                      QSettings::IniFormat);
  }

  if (name == QString()) return;
  m_name      = name + "DialogGeometry";
  QString geo = m_settings->value(m_name).toString();
  if (geo != QString()) {
    QStringList values = geo.split(" ");
    assert(values.size() == 4);
    // Ensure that the dialog is visible in the screen.
    // The dialog opens with some offset to bottom-right direction
    // if a flag Qt::WindowMaximizeButtonHint is set. (e.g. PencilTestPopup)
    // Therefore, if the dialog is moved to the bottom-end of the screen,
    // it will be got out of the screen on the next launch.
    // The following position adjustment will also prevent such behavior.

    // try and get active screen
    if (parent != NULL) {
      m_currentScreen = QApplication::desktop()->screenNumber(parent);
    }
    QRect screen = QApplication::desktop()->availableGeometry(m_currentScreen);
    int x        = values.at(0).toInt();
    int y        = values.at(1).toInt();

    // make sure that the window is visible on the screen
    // all popups will popup on the active window the first time
    // so popups moved to other monitors will be moved back
    // when restarting OpenToonz.

    // This may be somewhat annoying if a user REALLY wants the popup
    // on another monitor by default, but this is better than
    // a user thinking the program is broken because they didn't notice
    // the popup on another monitor
    if (x > screen.right() - 50) x  = screen.right() - 50;
    if (x < screen.left()) x        = screen.left();
    if (y > screen.bottom() - 90) y = screen.bottom() - 90;
    if (y < screen.top()) y         = screen.top();
    setGeometry(x, y, values.at(2).toInt(), values.at(3).toInt());
    m_settings->setValue(m_name,
                         QString::number(x) + " " + QString::number(y) + " " +
                             QString::number(values.at(2).toInt()) + " " +
                             QString::number(values.at(3).toInt()));
  }
}

//-----------------------------------------------------------------------------

Dialog::~Dialog() {}
//-----------------------------------------------------------------------------

void Dialog::moveEvent(QMoveEvent *e) {
  if (m_name == QString()) return;

  QRect r = geometry();
  m_settings->setValue(m_name, QString::number(r.left()) + " " +
                                   QString::number(r.top()) + " " +
                                   QString::number(r.width()) + " " +
                                   QString::number(r.height()));
}

//---------------------------------------------------------------------------------

void Dialog::resizeEvent(QResizeEvent *e) {
  if (Preferences::instance()->getCurrentLanguage() != "English") {
    QSize t = this->size();
    QLabel *s;
    foreach (s, m_labelList)
      s->setFixedWidth(t.width() * .35);
  }

  if (m_name == QString()) return;

  QRect r = geometry();
  m_settings->setValue(m_name, QString::number(r.left()) + " " +
                                   QString::number(r.top()) + " " +
                                   QString::number(r.width()) + " " +
                                   QString::number(r.height()));
}

//-----------------------------------------------------------------------------

//! By default, QDialogs always reset position/size upon hide events. However,
//! we want to prevent such behaviour on Toonz Dialogs - this method is
//! reimplemented
//! for this purpose.
void Dialog::hideEvent(QHideEvent *event) {
  int x = pos().rx();
  int y = pos().ry();
  // make sure the dialog is actually visible on a screen
  int screenCount = QApplication::desktop()->screenCount();
  int currentScreen;
  for (int i = 0; i < screenCount; i++) {
    if (QApplication::desktop()->screenGeometry(i).contains(pos())) {
      currentScreen = i;
      break;
    } else {
      // if not - put it back on the main window
      currentScreen = m_currentScreen;
    }
  }
  QRect screen = QApplication::desktop()->availableGeometry(currentScreen);

  if (x > screen.right() - 50) x  = screen.right() - 50;
  if (x < screen.left()) x        = screen.left();
  if (y > screen.bottom() - 90) y = screen.bottom() - 90;
  if (y < screen.top()) y         = screen.top();
  move(QPoint(x, y));
  resize(size());
  QRect r = geometry();
  m_settings->setValue(m_name, QString::number(r.left()) + " " +
                                   QString::number(r.top()) + " " +
                                   QString::number(r.width()) + " " +
                                   QString::number(r.height()));
  emit dialogClosed();
}

//-----------------------------------------------------------------------------
/*! Create the new layouts (2 Vertical) for main part of dialog.
*/
void Dialog::beginVLayout() {
  m_isMainVLayout = true;

  m_leftVLayout = new QVBoxLayout;
  m_leftVLayout->setMargin(m_layoutMargin);
  m_leftVLayout->setSpacing(m_layoutSpacing);

  m_rightVLayout = new QVBoxLayout;
  m_rightVLayout->setMargin(m_layoutMargin);
  m_rightVLayout->setSpacing(m_layoutSpacing);
}

//-----------------------------------------------------------------------------
/*! Add to main part of dialog the Vertical Layouts ,insert them in a orizontal
                layout to form two column, set Vertical Layouts to 0 .
*/
void Dialog::endVLayout() {
  if (!m_leftVLayout || !m_rightVLayout) return;
  m_isMainVLayout = false;

  QHBoxLayout *layout = new QHBoxLayout;
  layout->setMargin(m_layoutMargin);
  layout->setSpacing(m_layoutSpacing);
  layout->setSizeConstraint(QLayout::SetFixedSize);

  layout->addLayout(m_leftVLayout);
  layout->setAlignment(m_leftVLayout, Qt::AlignLeft);
  layout->addLayout(m_rightVLayout);
  layout->setAlignment(m_rightVLayout, Qt::AlignLeft);

  addLayout(layout);

  m_leftVLayout  = 0;
  m_rightVLayout = 0;
}

//-----------------------------------------------------------------------------
/*! Create a new Horizontal Layout for main part of dialog.
*/
void Dialog::beginHLayout() {
  m_isMainHLayout = true;
  m_mainHLayout   = new QHBoxLayout;
  m_mainHLayout->setMargin(m_layoutMargin);
  m_mainHLayout->setSpacing(m_layoutSpacing);
}

//-----------------------------------------------------------------------------
/*! Add to main part of dialog the Horizontal Layout  and set it to 0.
*/
void Dialog::endHLayout() {
  m_isMainHLayout = false;
  if (!m_mainHLayout) return;
  addLayout(m_mainHLayout);
  m_mainHLayout = 0;
}

//-----------------------------------------------------------------------------
/*! Add a widget \b widget to main part of dialog. If vertical layout is
                initialized add widget in right column if \b isRight is true,
   otherwise in
                left column. \b isRight by default is true.
*/
void Dialog::addWidget(QWidget *widget, bool isRight) {
  if (m_isMainVLayout) {
    assert(m_leftVLayout && m_rightVLayout);
    QWidget *w = new QWidget();
    int h      = widget->height() + m_layoutSpacing;
    if (isRight) {
      m_leftVLayout->addSpacing(h);
      m_rightVLayout->addWidget(widget);
    } else {
      m_leftVLayout->addWidget(widget, 1, Qt::AlignRight);
      m_rightVLayout->addSpacing(h);
    }
    return;
  } else if (m_isMainHLayout) {
    assert(m_mainHLayout);
    m_mainHLayout->addWidget(widget);
    return;
  }
  m_topLayout->addWidget(widget);
}

//-----------------------------------------------------------------------------
/*! Add a pair [widget,widget] to main part of dialog.
\n	If vertical and horizontal layout are not initialized create an
horizontal
                box layout containing the two widgets and recall \b addLayout().
                If vertical layout is current add the pair to vertical layout of
main part,
                \b firstW on first column and \b secondW on second column.
                Else if horizontal layout is current create an horizontal box
layout containing
                \b firstW and \b secondW and add it to horizontal layout.
*/
void Dialog::addWidgets(QWidget *firstW, QWidget *secondW) {
  if (m_isMainVLayout) {
    assert(m_leftVLayout && m_rightVLayout);
    m_leftVLayout->addWidget(firstW);
    m_rightVLayout->addWidget(secondW);
    return;
  }
  QHBoxLayout *pairLayout = new QHBoxLayout;
  pairLayout->setMargin(m_layoutMargin);
  pairLayout->setSpacing(m_layoutSpacing);
  pairLayout->addWidget(firstW);
  pairLayout->addWidget(secondW);

  if (m_isMainHLayout) {
    assert(m_mainHLayout);
    m_mainHLayout->addLayout(pairLayout);
    return;
  }
  addLayout(pairLayout);
}

//-----------------------------------------------------------------------------
/*! Add a pair [label,widget] to main part of dialog, label is created from
                \b QString \b nameLabel.
\n	Recall \b addWidgets(QWdiget* firstW, QWdiget* secondW).
*/
void Dialog::addWidget(QString labelName, QWidget *widget) {
  QLabel *label = new QLabel(labelName);
  m_labelList.push_back(label);
  label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  label->setFixedSize(m_labelWidth, widget->height());
  addWidgets(label, widget);
}

//-----------------------------------------------------------------------------
/*! Add a layout \b layout to main part of dialog. If vertical layout is
                initialized add widget in right column if \b isRight is true,
   otherwise in
                left column. \b isRight by default is true.
*/
void Dialog::addLayout(QLayout *layout, bool isRight) {
  if (m_isMainVLayout) {
    assert(m_leftVLayout && m_rightVLayout);
    int h = layout->itemAt(0)->widget()->height() + m_layoutSpacing;
    if (isRight) {
      m_leftVLayout->addSpacing(h);
      m_rightVLayout->addLayout(layout);
    } else {
      m_leftVLayout->addLayout(layout);
      m_rightVLayout->addSpacing(h);
    }
    return;
  } else if (m_isMainHLayout) {
    assert(m_mainHLayout);
    m_mainHLayout->addLayout(layout);
    return;
  }
  m_topLayout->addLayout(layout);
}

//-----------------------------------------------------------------------------
/*! Add a pair [widget,layout] to main part of dialog.
\n	If vertical and horizontal layout are not initialized create an
horizontal
                box layout containing \b widget and \b layout and recall \b
addLayout().
                If vertical layout is current add the pair [label,layout] to
vertical layout
                of main part, label on first column and layout on second column.
                Else if horizontal layout is current create an horizontal box
layout containing
                \b widget and \b layout and add it to horizontal layout.
*/
void Dialog::addWidgetLayout(QWidget *widget, QLayout *layout) {
  layout->setMargin(m_layoutMargin);
  layout->setSpacing(m_layoutSpacing);

  if (m_isMainVLayout) {
    assert(m_leftVLayout && m_rightVLayout);
    m_leftVLayout->addWidget(widget);
    m_rightVLayout->addLayout(layout);
    return;
  }

  QHBoxLayout *pairLayout = new QHBoxLayout;
  pairLayout->setMargin(m_layoutMargin);
  pairLayout->setSpacing(m_layoutSpacing);
  pairLayout->addWidget(widget);
  pairLayout->addLayout(layout);

  if (m_isMainHLayout) {
    assert(m_mainHLayout);
    m_mainHLayout->addLayout(pairLayout);
    return;
  }

  addLayout(pairLayout);
}

//-----------------------------------------------------------------------------
/*! Add a pair [label,layout] to main part of dialog, label is created from
                \b QString \b nameLabel.
\n	Recall \b addWidgetLayout(QWdiget* widget, QWdiget* layout).
*/
void Dialog::addLayout(QString labelName, QLayout *layout) {
  QLabel *label = new QLabel(labelName);
  m_labelList.push_back(label);
  label->setFixedWidth(m_labelWidth);
  label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  addWidgetLayout(label, layout);
}

//-----------------------------------------------------------------------------
/*! Add a pair [layout,layout] to main part of dialog.
\n	If vertical and horizontal layout are not initialized create an
horizontal
                box layout containing the two layouts and recall \b addLayout().
                If vertical layout is current add the pair [layout,layout] to
vertical layout
                of main part, \b firstL on first column and \b secondL on second
column.
                Else if horizontal layout is current create an horizontal box
layout containing
                \b firstL and \b secondL and add it to horizontal layout.
*/
void Dialog::addLayouts(QLayout *firstL, QLayout *secondL) {
  firstL->setMargin(m_layoutMargin);
  firstL->setSpacing(m_layoutSpacing);
  secondL->setMargin(m_layoutMargin);
  secondL->setSpacing(m_layoutSpacing);

  if (m_isMainVLayout) {
    assert(m_leftVLayout && m_rightVLayout);
    m_leftVLayout->addLayout(firstL);
    m_rightVLayout->addLayout(secondL);
    return;
  }

  QHBoxLayout *pairLayout = new QHBoxLayout;
  pairLayout->setMargin(m_layoutMargin);
  pairLayout->setSpacing(m_layoutSpacing);
  pairLayout->addLayout(firstL);
  pairLayout->addLayout(secondL);

  if (m_isMainHLayout) {
    assert(m_mainHLayout);
    m_mainHLayout->addLayout(pairLayout);
    return;
  }

  addLayout(pairLayout);
}

//-----------------------------------------------------------------------------
/*! Add spacing \b spacing to main part of dialog.
*/
void Dialog::addSpacing(int spacing) {
  if (m_isMainVLayout) {
    assert(m_leftVLayout && m_rightVLayout);
    m_leftVLayout->addSpacing(spacing);
    m_rightVLayout->addSpacing(spacing);
    return;
  } else if (m_isMainHLayout) {
    assert(m_mainHLayout);
    m_mainHLayout->addSpacing(spacing);
    return;
  }
  m_topLayout->addSpacing(spacing);
}

//-----------------------------------------------------------------------------
/*! Add a separator \b Separator to main part of dialog.
                If vertical layout is initialized add an horizontal separator.
                If horizontal layout is initialized add a vertical separator.
*/
void Dialog::addSeparator(QString name) {
  Separator *separator = new Separator(name);
  if (m_isMainVLayout) {
    assert(m_leftVLayout && m_rightVLayout);
    endVLayout();
    addWidget(separator);
    beginVLayout();
    return;
  } else if (m_isMainHLayout) {
    assert(m_mainHLayout);
    separator->setOrientation(false);
    m_mainHLayout->addWidget(separator);
    return;
  }
  addWidget(separator);
}

//-----------------------------------------------------------------------------
/*! Set the alignement of the main layout
*/
void Dialog::setAlignment(Qt::Alignment alignment) {
  m_mainFrame->layout()->setAlignment(alignment);
}

//-----------------------------------------------------------------------------
/*! Set to \b spacing spacing of main part of dialog.
*/
void Dialog::setTopSpacing(int spacing) {
  m_layoutSpacing = spacing;
  m_topLayout->setSpacing(spacing);
}
//-----------------------------------------------------------------------------

void Dialog::setLabelWidth(int labelWidth) { m_labelWidth = labelWidth; }

//-----------------------------------------------------------------------------
/*! Set to \b spacing spacing of all layout inserted in main part of dialog,
                horizontal layout and vertical layout.
*/
void Dialog::setLayoutInsertedSpacing(int spacing) {
  m_layoutSpacing = spacing;
}

//-----------------------------------------------------------------------------
/*! Return the spacing of all layout inserted in main part of dialog,
                horizontal layout and vertical layout.
*/
int Dialog::getLayoutInsertedSpacing() { return m_layoutSpacing; }

//-----------------------------------------------------------------------------
/*! Set to \b margin margin of main part of dialog.
*/
void Dialog::setTopMargin(int margin) { m_topLayout->setMargin(margin); }

//-----------------------------------------------------------------------------
/*! Set to \b margin margin of button part of dialog.
*/
void Dialog::setButtonBarMargin(int margin) {
  m_buttonLayout->setMargin(margin);
}

//-----------------------------------------------------------------------------
/*! Set to \b spacing spacing of button part of dialog.
*/
void Dialog::setButtonBarSpacing(int spacing) {
  m_buttonLayout->setSpacing(spacing);
}

//-----------------------------------------------------------------------------
/*! Add a widget to the button part of dialog.
*/
void Dialog::addButtonBarWidget(QWidget *widget) {
  widget->setMinimumSize(65, 25);
  assert(m_hasButton);
  if (m_hasButton) {
    m_buttonLayout->addWidget(widget);
    m_buttonBarWidgets.push_back(widget);
  }
}

//-----------------------------------------------------------------------------
/*! Remove all widget from the button part of dialog.
*/
void Dialog::clearButtonBar() {
  for (int i = 0; i < (int)m_buttonBarWidgets.size(); i++) {
    m_buttonLayout->removeWidget(m_buttonBarWidgets[i]);
  }
  m_buttonBarWidgets.clear();
}

//-----------------------------------------------------------------------------
/*! Add two widget to the button part of dialog.
*/
void Dialog::addButtonBarWidget(QWidget *first, QWidget *second) {
  first->setMinimumSize(65, 25);
  second->setMinimumSize(65, 25);
  assert(m_hasButton);
  if (m_hasButton) {
    m_buttonLayout->addWidget(first);
    m_buttonLayout->addWidget(second);
  }
}

//-----------------------------------------------------------------------------
/*! Add three widget to the button part of dialog.
*/
void Dialog::addButtonBarWidget(QWidget *first, QWidget *second,
                                QWidget *third) {
  first->setMinimumSize(65, 25);
  second->setMinimumSize(65, 25);
  third->setMinimumSize(65, 25);
  assert(m_hasButton);
  if (m_hasButton) {
    m_buttonLayout->addWidget(first);
    m_buttonLayout->addWidget(second);
    m_buttonLayout->addWidget(third);
  }
}

//-----------------------------------------------------------------------------
/*! Add four widget to the button part of dialog.
*/
void Dialog::addButtonBarWidget(QWidget *first, QWidget *second, QWidget *third,
                                QWidget *fourth) {
  first->setMinimumSize(65, 25);
  second->setMinimumSize(65, 25);
  third->setMinimumSize(65, 25);
  assert(m_hasButton);
  if (m_hasButton) {
    m_buttonLayout->addWidget(first);
    m_buttonLayout->addWidget(second);
    m_buttonLayout->addWidget(third);
    m_buttonLayout->addWidget(fourth);
  }
}

//=============================================================================

MessageAndCheckboxDialog::MessageAndCheckboxDialog(QWidget *parent,
                                                   bool hasButton,
                                                   bool hasFixedSize,
                                                   const QString &name)
    : Dialog(parent, hasButton, hasFixedSize, name) {}

//=============================================================================

void MessageAndCheckboxDialog::onButtonPressed(int id) { done(id); }

//=============================================================================

void MessageAndCheckboxDialog::onCheckboxChanged(int checked) {
  m_checked = checked;
}

//=============================================================================

RadioButtonDialog::RadioButtonDialog(const QString &labelText,
                                     const QList<QString> &radioButtonList,
                                     QWidget *parent, Qt::WindowFlags f)
    : Dialog(parent, true, true), m_result(1) {
  setWindowTitle(tr("OpenToonz"));

  setMinimumSize(20, 20);

  beginVLayout();

  QLabel *label = new QLabel(labelText);
  label->setAlignment(Qt::AlignLeft);
  label->setFixedHeight(2 * WidgetHeight);
  addWidget(label);

  QButtonGroup *buttonGroup = new QButtonGroup(this);
  int i;
  for (i = 0; i < radioButtonList.count(); i++) {
    QRadioButton *radioButton = new QRadioButton(radioButtonList.at(i));
    if (i == m_result - 1) radioButton->setChecked(true);
    radioButton->setFixedHeight(WidgetHeight);
    buttonGroup->addButton(radioButton);
    buttonGroup->setId(radioButton, i);
    addWidget(radioButton);
  }

  bool ret = connect(buttonGroup, SIGNAL(buttonClicked(int)),
                     SLOT(onButtonClicked(int)));

  endVLayout();

  QPushButton *applyButton = new QPushButton(QObject::tr("Apply"));
  ret = ret && connect(applyButton, SIGNAL(pressed()), this, SLOT(onApply()));
  QPushButton *cancelButton = new QPushButton(QObject::tr("Cancel"));
  ret = ret && connect(cancelButton, SIGNAL(pressed()), this, SLOT(onCancel()));

  addButtonBarWidget(applyButton, cancelButton);

  assert(ret);
}

//-----------------------------------------------------------------------------

void RadioButtonDialog::onButtonClicked(int id) {
  // Add "1" because "0" is cancel button.
  m_result = id + 1;
}

//-----------------------------------------------------------------------------

void RadioButtonDialog::onCancel() { done(0); }

//-----------------------------------------------------------------------------

void RadioButtonDialog::onApply() { done(m_result); }

//=============================================================================

int DVGui::RadioButtonMsgBox(MsgType type, const QString &labelText,
                             const QList<QString> &radioButtonList,
                             QWidget *parent) {
  RadioButtonDialog *dialog =
      new RadioButtonDialog(labelText, radioButtonList, parent);
  QString msgBoxTitle = getMsgBoxTitle(DVGui::WARNING);
  dialog->setWindowTitle(msgBoxTitle);
  return dialog->exec();
}

//=============================================================================

ProgressDialog::ProgressDialog(const QString &labelText,
                               const QString &cancelButtonText, int minimum,
                               int maximum, QWidget *parent, Qt::WindowFlags f)
    : Dialog(parent, true, true), m_isCanceled(false) {
  setWindowTitle(tr("OpenToonz"));

  setMinimumSize(20, 20);

  beginVLayout();

  m_label = new QLabel(this);
  m_label->setText(labelText);
  addWidget(m_label);

  m_progressBar = new QProgressBar(this);
  m_progressBar->setRange(minimum, maximum);
  m_progressBar->setMinimumWidth(250);
  addWidget(m_progressBar);

  endVLayout();

  if (!cancelButtonText.isEmpty())
    setCancelButton(new QPushButton(cancelButtonText));
}

//-----------------------------------------------------------------------------

void ProgressDialog::setLabelText(const QString &text) {
  m_label->setText(text);
}

//-----------------------------------------------------------------------------

void ProgressDialog::setCancelButton(QPushButton *cancelButton) {
  m_cancelButton = cancelButton;
  bool ret = connect(cancelButton, SIGNAL(pressed()), this, SLOT(onCancel()));
  ret =
      ret && connect(cancelButton, SIGNAL(pressed()), this, SIGNAL(canceled()));
  assert(ret);
  addButtonBarWidget(m_cancelButton);
}

//-----------------------------------------------------------------------------

int ProgressDialog::maximum() { return m_progressBar->maximum(); }
//-----------------------------------------------------------------------------

void ProgressDialog::setMaximum(int maximum) {
  m_progressBar->setMaximum(maximum);
}

//-----------------------------------------------------------------------------

int ProgressDialog::minimum() { return m_progressBar->minimum(); }
//-----------------------------------------------------------------------------

void ProgressDialog::setMinimum(int minimum) {
  m_progressBar->setMinimum(minimum);
}

//-----------------------------------------------------------------------------

int ProgressDialog::value() { return m_progressBar->value(); }

//-----------------------------------------------------------------------------

void ProgressDialog::setValue(int progress) {
  m_progressBar->setValue(progress);
  if (isModal()) qApp->processEvents();
}

//-----------------------------------------------------------------------------

void ProgressDialog::reset() { m_progressBar->reset(); }

//-----------------------------------------------------------------------------

bool ProgressDialog::wasCanceled() const { return m_isCanceled; }

//-----------------------------------------------------------------------------

void ProgressDialog::onCancel() {
  m_isCanceled = true;
  reset();
  hide();
}

//=============================================================================

int DVGui::MsgBox(MsgType type, const QString &text,
                  const std::vector<QString> &buttons, int defaultButtonIndex,
                  QWidget *parent) {
  Dialog dialog(parent, true);
  dialog.setWindowFlags(dialog.windowFlags() | Qt::WindowStaysOnTopHint);
  dialog.setAlignment(Qt::AlignLeft);
  QString msgBoxTitle = getMsgBoxTitle(type);

  dialog.setWindowTitle(msgBoxTitle);

  QLabel *mainTextLabel = new QLabel(text, &dialog);
  QPixmap iconPixmap    = getMsgBoxPixmap(type);
  if (!iconPixmap.isNull()) {
    QLabel *iconLabel = new QLabel(&dialog);
    iconLabel->setPixmap(iconPixmap);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addWidget(iconLabel);
    mainLayout->addSpacing(16);
    mainLayout->addWidget(mainTextLabel);
    dialog.addLayout(mainLayout);
  } else
    dialog.addWidget(mainTextLabel);

  // ButtonGroup: is used only to retrieve the clicked button
  QButtonGroup *buttonGroup = new QButtonGroup(&dialog);

  for (int i = 0; i < (int)buttons.size(); i++) {
    QPushButton *button = new QPushButton(buttons[i], &dialog);
    if (defaultButtonIndex == i)
      button->setDefault(true);
    else
      button->setDefault(false);
    dialog.addButtonBarWidget(button);

    buttonGroup->addButton(button, i + 1);
  }

  QObject::connect(buttonGroup, SIGNAL(buttonPressed(int)), &dialog,
                   SLOT(done(int)));

  dialog.raise();

  return dialog.exec();
}

//-----------------------------------------------------------------------------

void DVGui::MsgBoxInPopup(MsgType type, const QString &text) {
  // this function must be called by the main thread only
  // (only the main thread should access directly the GUI)
  // (note: working thread can and should call MsgBox(type,text) instead; see
  // tmsgcore.h)

  Q_ASSERT(QApplication::instance()->thread() == QThread::currentThread());

  // a working thread can trigger a call to this function (by the main thread)
  // also when a popup is already open
  // therefore we need a messageQueue
  // note: no mutex are needed because only the main thread call this function
  static QList<QPair<MsgType, QString>> messageQueue;
  static bool popupIsOpen = false;

  messageQueue.append(qMakePair(type, text));
  if (popupIsOpen) return;
  popupIsOpen = true;

  Dialog dialog(0, true);

  dialog.setWindowFlags(dialog.windowFlags() | Qt::WindowStaysOnTopHint);
  dialog.setAlignment(Qt::AlignLeft);
  QLabel *mainTextLabel = new QLabel("", &dialog);
  mainTextLabel->setMinimumWidth(400);
  QLabel *iconLabel = new QLabel(&dialog);

  QHBoxLayout *mainLayout = new QHBoxLayout;
  mainLayout->addWidget(iconLabel);
  mainLayout->addStretch();
  mainLayout->addWidget(mainTextLabel);
  mainLayout->addStretch();
  dialog.addLayout(mainLayout);

  // ButtonGroup: is used only to retrieve the clicked button
  QButtonGroup *buttonGroup = new QButtonGroup(&dialog);
  QPushButton *button       = new QPushButton(QPushButton::tr("OK"), &dialog);
  button->setDefault(true);
  dialog.addButtonBarWidget(button);
  buttonGroup->addButton(button, 1);
  QObject::connect(buttonGroup, SIGNAL(buttonPressed(int)), &dialog,
                   SLOT(done(int)));

  while (!messageQueue.empty()) {
    MsgType type1 = messageQueue.first().first;
    QString text1 = messageQueue.first().second;
    messageQueue.pop_front();

    mainTextLabel->setText(text1);

    QString msgBoxTitle = getMsgBoxTitle(type1);
    dialog.setWindowTitle(msgBoxTitle);

    QPixmap iconPixmap = getMsgBoxPixmap(type1);
    if (!iconPixmap.isNull()) {
      iconLabel->setPixmap(iconPixmap);
      iconLabel->setVisible(true);
    } else {
      iconLabel->setVisible(false);
    }

    dialog.raise();
    dialog.exec();

  }  // loop: open the next dialog in the queue
  popupIsOpen = false;
}

//-----------------------------------------------------------------------------

int DVGui::MsgBox(const QString &text, const QString &button1Text,
                  const QString &button2Text, const QString &button3Text,
                  int defaultButtonIndex, QWidget *parent) {
  Dialog dialog(parent, true);
  dialog.setWindowFlags(dialog.windowFlags() | Qt::WindowStaysOnTopHint);
  dialog.setAlignment(Qt::AlignLeft);
  QString msgBoxTitle = getMsgBoxTitle(QUESTION);
  dialog.setWindowTitle(msgBoxTitle);

  QLabel *mainTextLabel = new QLabel(text, &dialog);
  QPixmap iconPixmap    = getMsgBoxPixmap(QUESTION);
  if (!iconPixmap.isNull()) {
    QLabel *iconLabel = new QLabel(&dialog);
    iconLabel->setPixmap(iconPixmap);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addWidget(iconLabel);
    mainLayout->addSpacing(16);
    mainLayout->addWidget(mainTextLabel);
    dialog.addLayout(mainLayout);
  } else
    dialog.addWidget(mainTextLabel);

  // ButtonGroup: is used only to retrieve the clicked button
  QButtonGroup *buttonGroup = new QButtonGroup(&dialog);

  QPushButton *button1 = new QPushButton(button1Text, &dialog);
  button1->setDefault(false);
  if (defaultButtonIndex == 0) button1->setDefault(true);
  dialog.addButtonBarWidget(button1);
  buttonGroup->addButton(button1, 1);

  QPushButton *button2 = new QPushButton(button2Text, &dialog);
  button2->setDefault(false);
  if (defaultButtonIndex == 1) button2->setDefault(true);
  dialog.addButtonBarWidget(button2);
  buttonGroup->addButton(button2, 2);

  QPushButton *button3 = new QPushButton(button3Text, &dialog);
  button3->setDefault(false);
  if (defaultButtonIndex == 2) button3->setDefault(true);
  dialog.addButtonBarWidget(button3);
  buttonGroup->addButton(button3, 3);

  QObject::connect(buttonGroup, SIGNAL(buttonPressed(int)), &dialog,
                   SLOT(done(int)));
  dialog.raise();
  return dialog.exec();
}

//-----------------------------------------------------------------------------

int DVGui::MsgBox(const QString &text, const QString &button1Text,
                  const QString &button2Text, const QString &button3Text,
                  const QString &button4Text, int defaultButtonIndex,
                  QWidget *parent) {
  Dialog dialog(parent, true);
  dialog.setWindowFlags(dialog.windowFlags() | Qt::WindowStaysOnTopHint);
  dialog.setAlignment(Qt::AlignLeft);
  QString msgBoxTitle = getMsgBoxTitle(QUESTION);
  dialog.setWindowTitle(msgBoxTitle);

  QLabel *mainTextLabel = new QLabel(text, &dialog);
  QPixmap iconPixmap    = getMsgBoxPixmap(QUESTION);
  if (!iconPixmap.isNull()) {
    QLabel *iconLabel = new QLabel(&dialog);
    iconLabel->setPixmap(iconPixmap);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addWidget(iconLabel);
    mainLayout->addSpacing(16);
    mainLayout->addWidget(mainTextLabel);
    dialog.addLayout(mainLayout);
  } else
    dialog.addWidget(mainTextLabel);

  // ButtonGroup: is used only to retrieve the clicked button
  QButtonGroup *buttonGroup = new QButtonGroup(&dialog);

  QPushButton *button1 = new QPushButton(button1Text, &dialog);
  button1->setDefault(false);
  if (defaultButtonIndex == 0) button1->setDefault(true);
  dialog.addButtonBarWidget(button1);
  buttonGroup->addButton(button1, 1);

  QPushButton *button2 = new QPushButton(button2Text, &dialog);
  button2->setDefault(false);
  if (defaultButtonIndex == 1) button2->setDefault(true);
  dialog.addButtonBarWidget(button2);
  buttonGroup->addButton(button2, 2);

  QPushButton *button3 = new QPushButton(button3Text, &dialog);
  button3->setDefault(false);
  if (defaultButtonIndex == 2) button3->setDefault(true);
  dialog.addButtonBarWidget(button3);
  buttonGroup->addButton(button3, 3);

  QPushButton *button4 = new QPushButton(button4Text, &dialog);
  button4->setDefault(false);
  if (defaultButtonIndex == 3) button4->setDefault(true);
  dialog.addButtonBarWidget(button4);
  buttonGroup->addButton(button4, 4);

  QObject::connect(buttonGroup, SIGNAL(buttonPressed(int)), &dialog,
                   SLOT(done(int)));
  dialog.raise();
  return dialog.exec();
}

//-----------------------------------------------------------------------------

int DVGui::MsgBox(const QString &text, const QString &button1,
                  const QString &button2, int defaultButtonIndex,
                  QWidget *parent) {
  Dialog dialog(parent, true);
  dialog.setWindowFlags(dialog.windowFlags() | Qt::WindowStaysOnTopHint);
  std::vector<QString> buttons;
  buttons.push_back(button1);
  buttons.push_back(button2);
  return DVGui::MsgBox(DVGui::QUESTION, text, buttons, defaultButtonIndex,
                       parent);
}

//-----------------------------------------------------------------------------

Dialog *DVGui::createMsgBox(MsgType type, const QString &text,
                            const QStringList &buttons, int defaultButtonIndex,
                            QWidget *parent) {
  Dialog *dialog = new Dialog(parent, true);
  dialog->setWindowFlags(dialog->windowFlags() | Qt::WindowStaysOnTopHint);
  dialog->setAlignment(Qt::AlignLeft);
  QString msgBoxTitle = getMsgBoxTitle(type);

  dialog->setWindowTitle(msgBoxTitle);

  QLabel *mainTextLabel = new QLabel(text, dialog);
  mainTextLabel->setObjectName("Label");
  QPixmap iconPixmap = getMsgBoxPixmap(type);
  if (!iconPixmap.isNull()) {
    QLabel *iconLabel = new QLabel(dialog);
    iconLabel->setPixmap(iconPixmap);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addWidget(iconLabel);
    mainLayout->addSpacing(16);
    mainLayout->addWidget(mainTextLabel);
    dialog->addLayout(mainLayout);
  } else
    dialog->addWidget(mainTextLabel);

  // ButtonGroup: is used only to retrieve the clicked button
  QButtonGroup *buttonGroup = new QButtonGroup(dialog);

  for (int i = 0; i < (int)buttons.size(); i++) {
    QPushButton *button = new QPushButton(buttons[i], dialog);
    if (defaultButtonIndex == i)
      button->setDefault(true);
    else
      button->setDefault(false);
    dialog->addButtonBarWidget(button);

    buttonGroup->addButton(button, i + 1);
  }

  QObject::connect(buttonGroup, SIGNAL(buttonPressed(int)), dialog,
                   SLOT(done(int)));

  return dialog;
}

//-----------------------------------------------------------------------------

MessageAndCheckboxDialog *DVGui::createMsgandCheckbox(
    MsgType type, const QString &text, const QString &checkBoxText,
    const QStringList &buttons, int defaultButtonIndex, QWidget *parent) {
  MessageAndCheckboxDialog *dialog = new MessageAndCheckboxDialog(parent, true);
  dialog->setWindowFlags(dialog->windowFlags() | Qt::WindowStaysOnTopHint);
  dialog->setAlignment(Qt::AlignLeft);
  QString msgBoxTitle = getMsgBoxTitle(type);

  dialog->setWindowTitle(msgBoxTitle);

  QLabel *mainTextLabel = new QLabel(text, dialog);
  mainTextLabel->setObjectName("Label");
  QPixmap iconPixmap = getMsgBoxPixmap(type);
  if (!iconPixmap.isNull()) {
    QLabel *iconLabel = new QLabel(dialog);
    iconLabel->setPixmap(iconPixmap);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addWidget(iconLabel);
    mainLayout->addSpacing(16);
    mainLayout->addWidget(mainTextLabel);
    dialog->addLayout(mainLayout);
  } else
    dialog->addWidget(mainTextLabel);

  // ButtonGroup: is used only to retrieve the clicked button
  QButtonGroup *buttonGroup = new QButtonGroup(dialog);

  for (int i = 0; i < (int)buttons.size(); i++) {
    QPushButton *button = new QPushButton(buttons[i], dialog);
    if (defaultButtonIndex == i)
      button->setDefault(true);
    else
      button->setDefault(false);
    dialog->addButtonBarWidget(button);

    buttonGroup->addButton(button, i + 1);
  }

  QCheckBox *dialogCheckBox   = new QCheckBox(dialog);
  QHBoxLayout *checkBoxLayout = new QHBoxLayout;
  QLabel *checkBoxLabel       = new QLabel(checkBoxText, dialog);
  checkBoxLayout->addWidget(dialogCheckBox);
  checkBoxLayout->addWidget(checkBoxLabel);
  checkBoxLayout->addStretch(0);

  dialog->addLayout(checkBoxLayout);

  QObject::connect(dialogCheckBox, SIGNAL(stateChanged(int)), dialog,
                   SLOT(onCheckboxChanged(int)));
  QObject::connect(buttonGroup, SIGNAL(buttonPressed(int)), dialog,
                   SLOT(onButtonPressed(int)));

  return dialog;
}

//-----------------------------------------------------------------------------

QString DVGui::getText(const QString &title, const QString &labelText,
                       const QString &text, bool *ok) {
  Dialog dialog(0, true);

  dialog.setWindowTitle(title);
  dialog.setWindowFlags(Qt::WindowStaysOnTopHint | Qt::WindowTitleHint |
                        Qt::CustomizeWindowHint);

  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  dialog.addLayout(layout);

  QLabel *label = new QLabel(labelText, &dialog);
  layout->addWidget(label);

  LineEdit *nameFld = new LineEdit(text, &dialog);
  layout->addWidget(nameFld);

  QPushButton *okBtn = new QPushButton(dialog.tr("OK"), &dialog);
  okBtn->setDefault(true);
  QPushButton *cancelBtn = new QPushButton(dialog.tr("Cancel"), &dialog);
  QObject::connect(okBtn, SIGNAL(clicked()), &dialog, SLOT(accept()));
  QObject::connect(cancelBtn, SIGNAL(clicked()), &dialog, SLOT(reject()));

  dialog.addButtonBarWidget(okBtn, cancelBtn);

  int ret     = dialog.exec();
  if (ok) *ok = (ret == QDialog::Accepted);

  return nameFld->text();
}

//-----------------------------------------------------------------------------

namespace {
bool isStyleIdInPalette(int styleId, const TPalette *palette) {
  if (palette->getStyleCount() == 0) return false;
  int i;
  for (i = 0; i < palette->getPageCount(); i++) {
    const TPalette::Page *page = palette->getPage(i);
    if (!page) return false;  // La pagina dovrebbe esserci sempre
    int j;
    for (j = 0; j < page->getStyleCount(); j++)
      if (page->getStyleId(j) == styleId) return true;
  }
  return false;
}
}

//-----------------------------------------------------------------------------

int DVGui::eraseStylesInDemand(TPalette *palette,
                               const TXsheetHandle *xsheetHandle,
                               TPalette *newPalette) {
  // Verifico se gli stili della paletta sono usati : eraseStylesInDemand()
  std::vector<int> styleIds;
  int h;
  for (h = 0; h < palette->getPageCount(); h++) {
    TPalette::Page *page = palette->getPage(h);
    if (!page) continue;  // La pagina dovrebbe esserci sempre
    int k;
    for (k = 0; k < page->getStyleCount(); k++) {
      int styleId = page->getStyleId(k);
      bool isStyleIdInNewPalette =
          (!newPalette) ? false : isStyleIdInPalette(styleId, newPalette);
      if (styleId > 0 && !isStyleIdInNewPalette) styleIds.push_back(styleId);
    }
  }

  return eraseStylesInDemand(palette, styleIds, xsheetHandle);
}

//-----------------------------------------------------------------------------

int DVGui::eraseStylesInDemand(TPalette *palette, std::vector<int> styleIds,
                               const TXsheetHandle *xsheetHandle) {
  struct locals {
    static bool isRasterLevel(const TXshSimpleLevel *level) {
      return level->getType() == TZP_XSHLEVEL ||
             level->getType() == OVL_XSHLEVEL;
    }
  };  // locals

  // Search xsheet levels attached to the palette
  std::set<TXshSimpleLevel *> levels;
  int row, column;
  findPaletteLevels(levels, row, column, palette, xsheetHandle->getXsheet());

  bool someStyleIsUsed =
      !levels.empty() ||
      styleIds.empty();  // I guess this is wrong... but I'm not touching it
  if (someStyleIsUsed) someStyleIsUsed = areStylesUsed(levels, styleIds);

  if (!someStyleIsUsed) return 1;

  // At least a style is selected and present in some level - ask user if and
  // how styles
  // should be deleted

  QString question = QObject::tr(
                         "Styles you are going to delete are used to paint "
                         "lines and areas in the animation level.\n") +
                     QObject::tr("How do you want to proceed?");

  int ret = DVGui::MsgBox(question, QObject::tr("Delete Styles Only"),
                          QObject::tr("Delete Styles, Lines and Areas"),
                          QObject::tr("Cancel"), 0);
  if (ret != 2) return (ret == 0 || ret == 3) ? 0 : 1;

  // Inform the user that case 2 will not produce an undo if a raster-based
  // level is detected
  if (boost::algorithm::any_of(levels, locals::isRasterLevel)) {
    std::vector<QString> buttons(2);
    buttons[0] = QObject::tr("Ok"), buttons[1] = QObject::tr("Cancel");

    if (DVGui::MsgBox(DVGui::WARNING,
                      QObject::tr("Deletion of Lines and Areas from "
                                  "raster-based levels is not undoable.\n"
                                  "Are you sure?"),
                      buttons) != 1)
      return 0;
  }

  QApplication::setOverrideCursor(Qt::WaitCursor);
  PaletteCmd::eraseStyles(levels, styleIds);
  QApplication::restoreOverrideCursor();

  return (assert(ret == 2), ret);  // return 2 ?     :D
}

//-----------------------------------------------------------------------------

void DVGui::setDialogTitle(const QString &dialogTitle) {
  DialogTitle = dialogTitle;
}

//-----------------------------------------------------------------------------
