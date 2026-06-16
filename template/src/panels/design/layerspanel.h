#pragma once
#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QPushButton>
#include <QHBoxLayout>

// Figma-style Layers panel with sample tree
class LayersPanel : public QWidget {
    Q_OBJECT
public:
    explicit LayersPanel(QWidget* parent = nullptr);
private:
    QTreeWidget* m_tree;
    void buildSampleTree();
    void addLayer(QTreeWidgetItem* parent, const QString& name,
                  const QString& type, bool visible = true);
};
