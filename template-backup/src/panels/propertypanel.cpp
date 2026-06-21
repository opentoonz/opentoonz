#include "propertypanel.h"
#include <QHeaderView>

PropertyInspector::PropertyInspector(QWidget* parent) : QTreeWidget(parent) {
    setHeaderLabels({"Property", "Value"});
    header()->setStretchLastSection(true);
    setAlternatingRowColors(true);
    setRootIsDecorated(true);
    setStyleSheet("background-color: #252526; color: #cccccc;");

    QTreeWidgetItem* root = new QTreeWidgetItem({"Application", "YZ UI Template"});
    addTopLevelItem(root);
    root->addChild(new QTreeWidgetItem({"Framework", "DockLayout + TPanel + Room"}));
    root->addChild(new QTreeWidgetItem({"Qt Version", QT_VERSION_STR}));
    expandAll();
}

void PropertyInspector::inspectWidget(QWidget* w) {
    clear();
    if (!w) return;
    QTreeWidgetItem* root = new QTreeWidgetItem({"Widget", w->objectName()});
    addTopLevelItem(root);
    root->addChild(new QTreeWidgetItem({"Class", w->metaObject()->className()}));
    root->addChild(new QTreeWidgetItem({"Geometry",
        QString("%1x%2+%3+%4").arg(w->width()).arg(w->height()).arg(w->x()).arg(w->y())}));
    root->addChild(new QTreeWidgetItem({"Visible", w->isVisible() ? "true" : "false"}));
    root->addChild(new QTreeWidgetItem({"Enabled", w->isEnabled() ? "true" : "false"}));
    expandAll();
}

void PropertyInspector::clearInspection() {
    clear();
}
