#include "layerspanel.h"
#include <QHeaderView>

LayersPanel::LayersPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Tabs: Layers | Assets
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({"Layer", "Type"});
    m_tree->setColumnWidth(0, 150);
    m_tree->setAlternatingRowColors(false);
    m_tree->setRootIsDecorated(true);
    m_tree->setIndentation(16);
    m_tree->setStyleSheet(
        "QTreeWidget { background: #2d2d2d; color: #ccc; border: none; } "
        "QTreeWidget::item { padding: 3px 4px; } "
        "QTreeWidget::item:hover { background: #3a3a3a; } "
        "QTreeWidget::item:selected { background: #007acc; } "
        "QHeaderView::section { background: #3a3a3a; color: #888; border: none; "
        "  padding: 4px; font-size: 11px; }");
    layout->addWidget(m_tree);

    buildSampleTree();

    // Add/Delete layer buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(4, 2, 4, 2);
    auto* addBtn = new QPushButton("+");
    auto* delBtn = new QPushButton("-");
    addBtn->setFixedSize(24, 24);
    delBtn->setFixedSize(24, 24);
    addBtn->setStyleSheet("color: #ccc; background: #3a3a3a; border: 1px solid #555;");
    delBtn->setStyleSheet("color: #ccc; background: #3a3a3a; border: 1px solid #555;");
    btnLayout->addStretch();
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(delBtn);
    layout->addLayout(btnLayout);
}

void LayersPanel::addLayer(QTreeWidgetItem* parent, const QString& name,
                            const QString& type, bool visible) {
    auto* item = new QTreeWidgetItem(parent ? parent
        : reinterpret_cast<QTreeWidgetItem*>(m_tree->invisibleRootItem()));
    item->setText(0, name);
    item->setText(1, type);
    item->setCheckState(0, visible ? Qt::Checked : Qt::Unchecked);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    if (!parent) m_tree->addTopLevelItem(item);
}

void LayersPanel::buildSampleTree() {
    addLayer(nullptr, "Group 1", "Group");
    m_tree->topLevelItem(0)->setExpanded(true);
    addLayer(m_tree->topLevelItem(0), "Rectangle 1", "Rectangle");
    addLayer(m_tree->topLevelItem(0), "Text Label", "Text");
    addLayer(nullptr, "Frame 1", "Frame");
    m_tree->topLevelItem(1)->setExpanded(true);
    addLayer(m_tree->topLevelItem(1), "Ellipse 1", "Ellipse");
    addLayer(m_tree->topLevelItem(1), "Vector 1", "Vector");
    addLayer(nullptr, "Image Layer", "Image");
}
