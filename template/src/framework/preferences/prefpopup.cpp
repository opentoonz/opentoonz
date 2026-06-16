#include "prefpopup.h"
#include "prefsystem.h"
#include "checkbox.h"
#include "intfield.h"
#include "doublefield.h"
#include "filefield.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QGroupBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QApplication>

//=============================================================================
// Constructor — mirrors OpenToonz's QListWidget + QStackedWidget pattern
//=============================================================================

PreferencesPopup::PreferencesPopup(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Preferences"));
    setMinimumSize(700, 500);

    loadPreferences();

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto* mainLayout = new QHBoxLayout();
    mainLayout->setSpacing(0);
    outerLayout->addLayout(mainLayout, 1);

    // Left: Category list
    m_categoryList = new QListWidget(this);
    m_categoryList->setFixedWidth(160);
    m_categoryList->setSpacing(2);
    mainLayout->addWidget(m_categoryList);

    // Right: Stacked pages
    m_pagesStack = new QStackedWidget(this);
    mainLayout->addWidget(m_pagesStack, 1);

    // Add pages
    addCategory(tr("General"), createGeneralPage());
    addCategory(tr("Appearance"), createAppearancePage());
    addCategory(tr("Advanced"), createAdvancedPage());

    // Connect
    connect(m_categoryList, &QListWidget::currentRowChanged,
            this, &PreferencesPopup::onCategoryChanged);

    // Select first
    m_categoryList->setCurrentRow(0);

    // OK/Cancel buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* okBtn = new QPushButton(tr("OK"), this);
    auto* cancelBtn = new QPushButton(tr("Cancel"), this);
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    outerLayout->addLayout(btnLayout);

    connect(okBtn, &QPushButton::clicked, this, [this]() { savePreferences(); accept(); });
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

//=============================================================================
// Category management
//=============================================================================

void PreferencesPopup::addCategory(const QString& name, QWidget* page) {
    m_categoryList->addItem(name);
    m_pagesStack->addWidget(page);
}

void PreferencesPopup::onCategoryChanged(int index) {
    m_pagesStack->setCurrentIndex(index);
}

//=============================================================================
// Widget factory methods
//=============================================================================

void PreferencesPopup::insertUI(QGridLayout* layout, int row,
                                 const QString& label, QWidget* widget) {
    layout->addWidget(new QLabel(label), row, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(widget, row, 1);
}

void PreferencesPopup::insertFootNote(QGridLayout* layout, int row) {
    auto* note = new QLabel(tr("* Changes take effect after restart"));
    note->setStyleSheet("color: #888; font-size: 11px;");
    layout->addWidget(note, row, 0, 1, 2);
}

QWidget* PreferencesPopup::createCheckBox(const QString& key, const QString& label,
                                           bool defaultValue) {
    auto* cb = new DVGui::CheckBox(label);
    cb->setChecked(PreferenceSystem::instance()->boolValue(key, defaultValue));
    m_widgetKeyMap[cb] = key;
    connect(cb, &QCheckBox::toggled, this, &PreferencesPopup::onChange);
    return cb;
}

QWidget* PreferencesPopup::createIntField(const QString& key, const QString& /*label*/,
                                           int value, int min, int max) {
    auto* f = new DVGui::IntLineEdit(nullptr,
        PreferenceSystem::instance()->intValue(key, value), min, max);
    f->setFixedWidth(80);
    m_widgetKeyMap[f] = key;
    connect(f, &DVGui::IntLineEdit::valueEditedByHand, this, &PreferencesPopup::onChange);
    return f;
}

QWidget* PreferencesPopup::createDoubleField(const QString& key, const QString& /*label*/,
                                              double value, double min, double max) {
    auto* f = new DVGui::DoubleLineEdit(nullptr,
        PreferenceSystem::instance()->doubleValue(key, value));
    if (min < max) f->setRange(min, max);
    f->setFixedWidth(80);
    m_widgetKeyMap[f] = key;
    connect(f, &DVGui::DoubleLineEdit::valueEditedByHand, this, &PreferencesPopup::onChange);
    return f;
}

QWidget* PreferencesPopup::createComboBox(const QString& key, const QString& /*label*/,
                                           const QList<QPair<QString, QVariant>>& items,
                                           const QVariant& currentValue) {
    auto* combo = new QComboBox();
    for (const auto& item : items)
        combo->addItem(item.first, item.second);
    int idx = combo->findData(currentValue);
    if (idx >= 0) combo->setCurrentIndex(idx);
    m_widgetKeyMap[combo] = key;
    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PreferencesPopup::onChange);
    return combo;
}

QWidget* PreferencesPopup::createFileField(const QString& key, const QString& /*label*/,
                                            const QString& path) {
    auto* f = new DVGui::FileField(nullptr,
        PreferenceSystem::instance()->stringValue(key, path));
    m_widgetKeyMap[f] = key;
    connect(f, &DVGui::FileField::pathChanged, this, &PreferencesPopup::onChange);
    return f;
}

//=============================================================================
// onChange — unified change handler (mirrors OpenToonz pattern)
//=============================================================================

void PreferencesPopup::onChange() {
    QWidget* w = qobject_cast<QWidget*>(sender());
    if (!w || !m_widgetKeyMap.contains(w)) return;

    QString key = m_widgetKeyMap[w];

    if (auto* cb = qobject_cast<QCheckBox*>(w)) {
        PreferenceSystem::instance()->setValue(key, cb->isChecked());
    } else if (auto* il = dynamic_cast<DVGui::IntLineEdit*>(w)) {
        PreferenceSystem::instance()->setValue(key, il->getValue());
    } else if (auto* dl = dynamic_cast<DVGui::DoubleLineEdit*>(w)) {
        PreferenceSystem::instance()->setValue(key, dl->getValue());
    } else if (auto* combo = qobject_cast<QComboBox*>(w)) {
        PreferenceSystem::instance()->setValue(key, combo->currentData());
    } else if (auto* ff = dynamic_cast<DVGui::FileField*>(w)) {
        PreferenceSystem::instance()->setValue(key, ff->getPath());
    }
}

//=============================================================================
// Load / Save
//=============================================================================

void PreferencesPopup::loadPreferences() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                 + "/preferences.xml";
    PreferenceSystem::instance()->load(path);
}

void PreferencesPopup::savePreferences() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                 + "/preferences.xml";
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    PreferenceSystem::instance()->save(path);
}

//=============================================================================
// Page 1: General
//=============================================================================

QWidget* PreferencesPopup::createGeneralPage() {
    auto* page = new QWidget();
    auto* layout = new QGridLayout(page);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setVerticalSpacing(8);

    int row = 0;

    // Startup
    auto* startupGroup = new QGroupBox(tr("Startup"));
    auto* sgLayout = new QGridLayout(startupGroup);
    sgLayout->addWidget(createCheckBox("general.showStartupPopup",
        tr("Show startup popup"), true), 0, 0);
    layout->addWidget(startupGroup, row++, 0, 1, 2);

    // Project
    auto* projGroup = new QGroupBox(tr("Default Paths"));
    auto* pgLayout = new QGridLayout(projGroup);
    insertUI(pgLayout, 0, tr("Project root:"),
        createFileField("general.projectRoot", tr("Project root:"), ""));
    layout->addWidget(projGroup, row++, 0, 1, 2);

    // Undo
    auto* undoGroup = new QGroupBox(tr("Undo"));
    auto* ugLayout = new QGridLayout(undoGroup);
    insertUI(ugLayout, 0, tr("Undo depth:"),
        createIntField("general.undoDepth", "", 50, 1, 500));
    layout->addWidget(undoGroup, row++, 0, 1, 2);

    insertFootNote(layout, row++);
    return page;
}

//=============================================================================
// Page 2: Appearance
//=============================================================================

QWidget* PreferencesPopup::createAppearancePage() {
    auto* page = new QWidget();
    auto* layout = new QGridLayout(page);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setVerticalSpacing(8);

    int row = 0;

    // Theme
    auto* themeGroup = new QGroupBox(tr("Theme"));
    auto* tgLayout = new QGridLayout(themeGroup);
    QList<QPair<QString, QVariant>> themes = {
        {"Dark", "Dark"}, {"Light", "Light"}, {"Default", "Default"},
        {"Blue", "Blue"}, {"Clay", "Clay"}, {"Neutral", "Neutral"},
        {"Synthwave", "Synthwave"}, {"Default-Green", "Default-Green"}
    };
    insertUI(tgLayout, 0, tr("Theme:"),
        createComboBox("appearance.theme", "", themes, "Dark"));
    layout->addWidget(themeGroup, row++, 0, 1, 2);

    // Language
    auto* langGroup = new QGroupBox(tr("Language"));
    auto* lgLayout = new QGridLayout(langGroup);
    QList<QPair<QString, QVariant>> langs = {
        {"English", "English"}, {"中文", "中文"}, {"日本語", "日本語"}
    };
    insertUI(lgLayout, 0, tr("Language:"),
        createComboBox("appearance.language", "", langs, "English"));
    layout->addWidget(langGroup, row++, 0, 1, 2);

    // Font
    auto* fontGroup = new QGroupBox(tr("Font"));
    auto* fgLayout = new QGridLayout(fontGroup);
    insertUI(fgLayout, 0, tr("Font size:"),
        createIntField("appearance.fontSize", "", 10, 8, 24));
    layout->addWidget(fontGroup, row++, 0, 1, 2);

    // Layout
    auto* layoutGroup = new QGroupBox(tr("Layout"));
    auto* lygLayout = new QGridLayout(layoutGroup);
    lygLayout->addWidget(createCheckBox("appearance.showIconsInMenu",
        tr("Show icons in menu"), true), 0, 0);
    lygLayout->addWidget(createCheckBox("appearance.showRoomBindButtons",
        tr("Show room bind buttons"), false), 1, 0);
    layout->addWidget(layoutGroup, row++, 0, 1, 2);

    insertFootNote(layout, row++);
    return page;
}

//=============================================================================
// Page 3: Advanced
//=============================================================================

QWidget* PreferencesPopup::createAdvancedPage() {
    auto* page = new QWidget();
    auto* layout = new QGridLayout(page);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setVerticalSpacing(8);

    int row = 0;

    // Performance
    auto* perfGroup = new QGroupBox(tr("Performance"));
    auto* pgLayout = new QGridLayout(perfGroup);
    insertUI(pgLayout, 0, tr("Timer interval (ms):"),
        createIntField("advanced.timerInterval", "", 16, 1, 1000));
    insertUI(pgLayout, 1, tr("Cache size (MB):"),
        createIntField("advanced.cacheSize", "", 256, 16, 8192));
    layout->addWidget(perfGroup, row++, 0, 1, 2);

    // Debug
    auto* debugGroup = new QGroupBox(tr("Debug"));
    auto* dgLayout = new QGridLayout(debugGroup);
    dgLayout->addWidget(createCheckBox("advanced.enableLogging",
        tr("Enable file logging"), true), 0, 0);
    dgLayout->addWidget(createCheckBox("advanced.enableDebugMenu",
        tr("Enable debug menu"), false), 1, 0);
    layout->addWidget(debugGroup, row++, 0, 1, 2);

    // File Paths
    auto* pathGroup = new QGroupBox(tr("File Paths"));
    auto* fpgLayout = new QGridLayout(pathGroup);
    insertUI(fpgLayout, 0, tr("FFmpeg path:"),
        createFileField("advanced.ffmpegPath", "", ""));
    insertUI(fpgLayout, 1, tr("Log directory:"),
        createFileField("advanced.logDirectory", "", ""));
    layout->addWidget(pathGroup, row++, 0, 1, 2);

    insertFootNote(layout, row++);
    return page;
}
