#pragma once
#include <QWidget>
#include <QToolBar>
#include <QAction>
#include <QToolButton>
#include <QHBoxLayout>
#include <QStyle>
#include <QApplication>

// Figma-style top tool bar
class DesignToolbar : public QWidget {
    Q_OBJECT
public:
    explicit DesignToolbar(QWidget* parent = nullptr);
};
