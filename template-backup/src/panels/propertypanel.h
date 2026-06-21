#pragma once
#include <QTreeWidget>

class PropertyInspector : public QTreeWidget {
    Q_OBJECT
public:
    explicit PropertyInspector(QWidget* parent = nullptr);
public slots:
    void inspectWidget(QWidget* w);
    void clearInspection();
};
