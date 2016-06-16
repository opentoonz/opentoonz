

#include "versioncontrolwidget.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QTimeEdit>
#include <QTextEdit>
#include <QDateTimeEdit>
#include <QRadioButton>

//=============================================================================
// DateChooserWidget
//-----------------------------------------------------------------------------

DateChooserWidget::DateChooserWidget(QWidget *parent)
    : QWidget(parent), m_selectedRadioIndex(0) {
  QVBoxLayout *mainLayout = new QVBoxLayout;
  mainLayout->setMargin(0);
  mainLayout->setAlignment(Qt::AlignTop);

  // Time
  QHBoxLayout *timeLayout = new QHBoxLayout;

  m_timeRadioButton = new QRadioButton;
  m_timeRadioButton->setChecked(true);
  connect(m_timeRadioButton, SIGNAL(clicked()), this,
          SLOT(onRadioButtonClicked()));

  m_timeEdit = new QTimeEdit;
  m_timeEdit->setDisplayFormat("hh:mm");

  timeLayout->addWidget(m_timeRadioButton);
  timeLayout->addWidget(m_timeEdit);
  timeLayout->addWidget(new QLabel(tr("time ago.")));
  timeLayout->addStretch();
  mainLayout->addLayout(timeLayout);

  // Days
  QHBoxLayout *dayLayout = new QHBoxLayout;

  m_dayRadioButton = new QRadioButton;
  connect(m_dayRadioButton, SIGNAL(clicked()), this,
          SLOT(onRadioButtonClicked()));

  m_dayEdit = new QSpinBox;
  m_dayEdit->setRange(1, 99);
  m_dayEdit->setValue(1);
  m_dayEdit->setEnabled(false);

  dayLayout->addWidget(m_dayRadioButton);
  dayLayout->addWidget(m_dayEdit);
  dayLayout->addWidget(new QLabel(tr("days ago.")));
  dayLayout->addStretch();
  mainLayout->addLayout(dayLayout);

  // Weeks
  QHBoxLayout *weekLayout = new QHBoxLayout;

  m_weekRadioButton = new QRadioButton;
  connect(m_weekRadioButton, SIGNAL(clicked()), this,
          SLOT(onRadioButtonClicked()));

  m_weekEdit = new QSpinBox;
  m_weekEdit->setRange(1, 99);
  m_weekEdit->setValue(1);
  m_weekEdit->setEnabled(false);

  weekLayout->addWidget(m_weekRadioButton);
  weekLayout->addWidget(m_weekEdit);
  weekLayout->addWidget(new QLabel(tr("weeks ago.")));
  weekLayout->addStretch();
  mainLayout->addLayout(weekLayout);

  // Custom date
  QHBoxLayout *customDateLayout = new QHBoxLayout;

  m_dateRadioButton = new QRadioButton;
  connect(m_dateRadioButton, SIGNAL(clicked()), this,
          SLOT(onRadioButtonClicked()));

  m_dateTimeEdit = new QDateTimeEdit;
  m_dateTimeEdit->setDisplayFormat("yyyy-MM-dd hh:mm");
  QDate now = QDate::currentDate();
  m_dateTimeEdit->setMaximumDate(now);
  m_dateTimeEdit->setDate(now);
  m_dateTimeEdit->setEnabled(false);
  m_dateTimeEdit->setCalendarPopup(true);

  customDateLayout->addWidget(m_dateRadioButton);
  customDateLayout->addWidget(m_dateTimeEdit);
  customDateLayout->addWidget(new QLabel(tr("( Custom date )")));
  customDateLayout->addStretch();
  mainLayout->addLayout(customDateLayout);

  setLayout(mainLayout);
}

//-----------------------------------------------------------------------------

void DateChooserWidget::disableAllWidgets() {
  m_timeEdit->setEnabled(false);
  m_dayEdit->setEnabled(false);
  m_weekEdit->setEnabled(false);
  m_dateTimeEdit->setEnabled(false);
}

//-----------------------------------------------------------------------------

void DateChooserWidget::onRadioButtonClicked() {
  QObject *obj         = sender();
  QRadioButton *button = dynamic_cast<QRadioButton *>(obj);

  if (!button) return;

  disableAllWidgets();

  if (button == m_timeRadioButton) {
    m_timeEdit->setEnabled(true);
    m_selectedRadioIndex = 0;
  } else if (button == m_dayRadioButton) {
    m_dayEdit->setEnabled(true);
    m_selectedRadioIndex = 1;
  } else if (button == m_weekRadioButton) {
    m_weekEdit->setEnabled(true);
    m_selectedRadioIndex = 2;
  } else if (button == m_dateRadioButton) {
    m_dateTimeEdit->setEnabled(true);
    m_selectedRadioIndex = 3;
  }
}

//-----------------------------------------------------------------------------

QString DateChooserWidget::getRevisionString() const {
  QString timeString;
  QString dateString;

  QDate today = QDate::currentDate();

  if (m_selectedRadioIndex == 0)  // Time
  {
    QTime currentTime = QTime::currentTime();
    QTime t           = m_timeEdit->time();
    int seconds       = t.hour() * 60 * 60 + t.minute() * 60;
    currentTime       = currentTime.addSecs(-seconds);
    timeString        = currentTime.toString("hh:mm");
    dateString        = today.toString("yyyy-MM-dd");
  } else if (m_selectedRadioIndex == 1)  // Days
  {
    timeString = "00:00";
    today      = today.addDays(-(m_dayEdit->value()));
    dateString = today.toString("yyyy-MM-dd");
  } else if (m_selectedRadioIndex == 2)  // Weeks
  {
    timeString = "00:00";
    today      = today.addDays(-(m_weekEdit->value() * 7));
    dateString = today.toString("yyyy-MM-dd");
  } else  // Custom date
  {
    QTime time = m_dateTimeEdit->time();
    timeString = time.toString("hh:mm");

    QDate date = m_dateTimeEdit->date();
    dateString = date.toString("yyyy-MM-dd");
  }

  return "{" + dateString + " " + timeString + "}";
}

//-----------------------------------------------------------------------------

//=============================================================================
// ConflictWidget
//-----------------------------------------------------------------------------

ConflictWidget::ConflictWidget(QWidget *parent)
    : QWidget(parent), m_button1Text(tr("Mine")), m_button2Text(tr("Theirs")) {
  m_mainLayout = new QVBoxLayout;
  m_mainLayout->setMargin(0);
  m_mainLayout->setAlignment(Qt::AlignTop);
  setLayout(m_mainLayout);
}

//-----------------------------------------------------------------------------

void ConflictWidget::setFiles(const QStringList &files) {
  DoubleRadioWidget *radio = 0;
  int fileCount            = files.size();
  for (int i = 0; i < fileCount; i++) {
    radio = new DoubleRadioWidget(m_button1Text, m_button2Text, files.at(i));
    connect(radio, SIGNAL(valueChanged()), this, SLOT(onRadioValueChanged()));
    m_mainLayout->addWidget(radio);
    m_radios.insert(radio, -1);
  }
  m_mainLayout->addStretch();
}

//-----------------------------------------------------------------------------

void ConflictWidget::onRadioValueChanged() {
  DoubleRadioWidget *obj = dynamic_cast<DoubleRadioWidget *>(sender());
  if (obj) m_radios[obj] = obj->getValue();

  if (!m_radios.values().contains(-1)) emit allConflictSetted();
}

//-----------------------------------------------------------------------------

QStringList ConflictWidget::getFilesWithOption(int option) const {
  QStringList files;
  QMap<DoubleRadioWidget *, int>::const_iterator i;
  for (i = m_radios.constBegin(); i != m_radios.constEnd(); ++i) {
    if (i.value() == option) files << i.key()->getText();
  }
  return files;
}

//=============================================================================
// DoubleRadioWidget
//-----------------------------------------------------------------------------

DoubleRadioWidget::DoubleRadioWidget(const QString &button1Text,
                                     const QString &button2Text,
                                     const QString &text, QWidget *parent)
    : QWidget(parent)

{
  QHBoxLayout *mainLayout = new QHBoxLayout;
  mainLayout->setMargin(0);

  m_firstButton = new QRadioButton(button1Text);
  connect(m_firstButton, SIGNAL(clicked()), this, SIGNAL(valueChanged()));
  m_secondButton = new QRadioButton(button2Text);
  connect(m_secondButton, SIGNAL(clicked()), this, SIGNAL(valueChanged()));

  m_label = new QLabel;
  m_label->setFixedWidth(180);
  m_label->setText(text);

  mainLayout->addWidget(m_label);
  mainLayout->addSpacing(20);
  mainLayout->addWidget(m_firstButton);
  mainLayout->addWidget(m_secondButton);

  setLayout(mainLayout);
}

//-----------------------------------------------------------------------------

int DoubleRadioWidget::getValue() const {
  if (m_firstButton->isChecked())
    return 0;
  else if (m_secondButton->isChecked())
    return 1;
  else
    return -1;
}

//-----------------------------------------------------------------------------

QString DoubleRadioWidget::getText() const { return m_label->text(); }
