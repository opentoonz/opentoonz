#include "propertiespanel.h"
#include <QGroupBox>

PropertiesPanel::PropertiesPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* tabs = new QTabWidget(this);
    tabs->setStyleSheet(
        "QTabWidget::pane { border: none; background: #2d2d2d; } "
        "QTabBar::tab { background: #3a3a3a; color: #888; padding: 6px 16px; } "
        "QTabBar::tab:selected { background: #2d2d2d; color: #fff; }");
    tabs->addTab(createDesignTab(), tr("Design"));
    tabs->addTab(createPrototypeTab(), tr("Prototype"));
    layout->addWidget(tabs);
}

QLabel* PropertiesPanel::makeLabel(const QString& text) {
    auto* l = new QLabel(text);
    l->setStyleSheet("color: #888; font-size: 11px; padding: 2px 0;");
    return l;
}

QLineEdit* PropertiesPanel::makeField(const QString& value) {
    auto* f = new QLineEdit(value);
    f->setStyleSheet(
        "QLineEdit { background: #3a3a3a; border: 1px solid #555; color: #ccc; "
        "padding: 3px 6px; border-radius: 2px; } "
        "QLineEdit:focus { border-color: #007acc; }");
    return f;
}

QWidget* PropertiesPanel::createDesignTab() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(4);

    // Position & Size
    auto* posGroup = new QGroupBox(tr("Position & Size"));
    posGroup->setStyleSheet("QGroupBox { color: #aaa; font-weight: bold; "
        "border: 1px solid #444; border-radius: 4px; margin-top: 8px; padding-top: 12px; }");
    auto* posLayout = new QGridLayout(posGroup);
    posLayout->setVerticalSpacing(2);
    posLayout->addWidget(makeLabel("X"), 0, 0); posLayout->addWidget(makeField("0"), 0, 1);
    posLayout->addWidget(makeLabel("Y"), 0, 2); posLayout->addWidget(makeField("0"), 0, 3);
    posLayout->addWidget(makeLabel("W"), 1, 0); posLayout->addWidget(makeField("100"), 1, 1);
    posLayout->addWidget(makeLabel("H"), 1, 2); posLayout->addWidget(makeField("100"), 1, 3);
    layout->addWidget(posGroup);

    // Fill
    auto* fillGroup = new QGroupBox(tr("Fill"));
    fillGroup->setStyleSheet(posGroup->styleSheet());
    auto* fillLayout = new QHBoxLayout(fillGroup);
    auto* colorBtn = new QPushButton("  ");
    colorBtn->setFixedSize(28, 28);
    colorBtn->setStyleSheet("background: #4488ff; border: 1px solid #666; border-radius: 3px;");
    fillLayout->addWidget(colorBtn);
    fillLayout->addWidget(makeField("#4488FF"));
    fillLayout->addStretch();
    layout->addWidget(fillGroup);

    // Stroke
    auto* strokeGroup = new QGroupBox(tr("Stroke"));
    strokeGroup->setStyleSheet(posGroup->styleSheet());
    auto* strokeLayout = new QHBoxLayout(strokeGroup);
    auto* strokeField = new QLineEdit("1");
    strokeField->setFixedWidth(50);
    strokeField->setStyleSheet(makeField("")->styleSheet());
    strokeLayout->addWidget(strokeField);
    strokeLayout->addWidget(makeField("#000000"));
    strokeLayout->addStretch();
    layout->addWidget(strokeGroup);

    // Opacity
    auto* opacityLayout = new QHBoxLayout();
    opacityLayout->addWidget(makeLabel(tr("Opacity")));
    auto* opacitySpin = new QSpinBox();
    opacitySpin->setRange(0, 100);
    opacitySpin->setValue(100);
    opacitySpin->setSuffix("%");
    opacitySpin->setStyleSheet(makeField("")->styleSheet());
    opacityLayout->addWidget(opacitySpin);
    layout->addLayout(opacityLayout);

    layout->addStretch();
    return page;
}

QWidget* PropertiesPanel::createPrototypeTab() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(4);

    auto* group = new QGroupBox(tr("Interaction"));
    group->setStyleSheet("QGroupBox { color: #aaa; font-weight: bold; "
        "border: 1px solid #444; border-radius: 4px; margin-top: 8px; padding-top: 12px; }");
    auto* gLayout = new QGridLayout(group);
    gLayout->setVerticalSpacing(4);

    gLayout->addWidget(makeLabel(tr("On Click")), 0, 0);
    auto* triggerCombo = new QComboBox();
    triggerCombo->addItems({tr("None"), tr("Navigate to"), tr("Open overlay"),
                            tr("Swap overlay"), tr("Close overlay"), tr("Back")});
    triggerCombo->setStyleSheet(makeField("")->styleSheet());
    gLayout->addWidget(triggerCombo, 0, 1);

    gLayout->addWidget(makeLabel(tr("Transition")), 1, 0);
    auto* transCombo = new QComboBox();
    transCombo->addItems({tr("Instant"), tr("Dissolve"), tr("Smart Animate"),
                          tr("Move In"), tr("Push"), tr("Slide In")});
    transCombo->setStyleSheet(makeField("")->styleSheet());
    gLayout->addWidget(transCombo, 1, 1);

    gLayout->addWidget(makeLabel(tr("Duration")), 2, 0);
    auto* durSpin = new QSpinBox();
    durSpin->setRange(0, 10000);
    durSpin->setValue(300);
    durSpin->setSuffix(" ms");
    durSpin->setStyleSheet(makeField("")->styleSheet());
    gLayout->addWidget(durSpin, 2, 1);

    layout->addWidget(group);
    layout->addStretch();
    return page;
}
