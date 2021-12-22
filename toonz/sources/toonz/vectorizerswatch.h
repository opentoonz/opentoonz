#pragma once

#ifndef VECTORIZER_SWATCH_H
#define VECTORIZER_SWATCH_H

// TnzCore includes
#include "tpalette.h"

// Toonz includes
#include "tthread.h"
#include "timage.h"

#include "toonz/tcenterlinevectorizer.h"
#include "toonzqt/planeviewer.h"

// Qt includes
#include <QFrame>

#include <memory>

//*****************************************************************************
//    VectorizerSwatchArea declaration
//*****************************************************************************

class VectorizerSwatchArea final : public QFrame {
  Q_OBJECT

public:
  class Swatch;

private:
  Swatch *m_leftSwatch, *m_rightSwatch;

public:
  VectorizerSwatchArea(QWidget *parent = 0);

  void updateView(const TAffine &viewAff);
  void resetView();

  const Swatch *leftSwatch() const { return m_leftSwatch; }
  Swatch *leftSwatch() { return m_leftSwatch; }

  const Swatch *rightSwatch() const { return m_rightSwatch; }
  Swatch *rightSwatch() { return m_rightSwatch; }

protected:
  void connectUpdates();
  void disconnectUpdates();

  void showEvent(QShowEvent *se) override;  // { connectUpdates(); }
  void hideEvent(QHideEvent *he) override;  // { disconnectUpdates(); }

public slots:

  void updateContents();
  void invalidateContents();

  void enablePreview(bool enable);
  void enableDrawCenterlines(bool enable);
};

//*****************************************************************************
//    VectorizerSwatchArea::Swatch declaration
//*****************************************************************************

class VectorizerSwatchArea::Swatch final : public PlaneViewer {
  TImageP m_img;
  VectorizerSwatchArea *m_area;

  bool m_drawVectors;
  bool m_drawInProgress;

public:
  Swatch(VectorizerSwatchArea *area);

  const TImageP &image() const { return m_img; }
  TImageP &image() { return m_img; }

  void setDrawVectors(bool draw) { m_drawVectors = draw; }
  void setDrawInProgress(bool draw) { m_drawInProgress = draw; }

protected:
  bool event(QEvent *e) override;
  void paintGL() override;
  void drawVectors();
  void drawInProgress();
};

//*****************************************************************************
//    VectorizationSwatchTask declaration   -   Private class
//*****************************************************************************

class VectorizationSwatchTask final : public TThread::Runnable {
  Q_OBJECT

  int m_row, m_col;
  TImageP m_image;

  std::unique_ptr<VectorizerConfiguration> m_config;

  TPaletteP m_substitutePalette;

public:
  VectorizationSwatchTask(int row, int col,
                          TPaletteP substitutePalette = TPaletteP());

  void run() override;
  void onStarted(TThread::RunnableP task) override;
  void onFinished(TThread::RunnableP task) override;

signals:

  void myStarted(TThread::RunnableP task);

private:
  // Not copyable
  VectorizationSwatchTask(const VectorizationSwatchTask &);
  VectorizationSwatchTask &operator=(const VectorizationSwatchTask &);
};

#endif  // VECTORIZER_SWATCH_H
