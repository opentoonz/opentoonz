#pragma once

#ifndef FRAMEHEAD_GADGET_INCLUDED
#define FRAMEHEAD_GADGET_INCLUDED

#include <QEvent>
#include <QObject>

class QPainter;
class FilmstripFrames;
class QEvent;
class QColor;
class QWidget;
class QMenu;

class FrameHeadGadget : public QObject {
  Q_OBJECT

public:
  enum Action {
    None,
    MoveHead,
    ActivateFos,
    DeactivateFos,
    ActivateMos,
    DeactivateMos
  };

protected:
  bool m_dragging;
  int m_action;
  int m_highlightedFosFrame;
  int m_buttonPressCellIndex;

  int m_highlightedMosFrame;

public:
  FrameHeadGadget();
  virtual ~FrameHeadGadget();

  void draw(QPainter &p, const QColor &lightColor, const QColor &darkColor);

  void drawPlayingHead(QPainter &p, const QColor &lightColor,
                       const QColor &darkColor);
  // reimpremented in FilmstripFrameHeadGadget
  virtual void drawOnionSkinSelection(QPainter &p, const QColor &lightColor,
                                      const QColor &darkColor);

  virtual void drawShiftTraceMarker(QPainter &p) {}

  virtual int getY() const = 0;

  virtual int index2y(int index) const = 0;
  virtual int y2index(int y) const     = 0;

  virtual void setCurrentFrame(int index) const = 0;
  virtual int getCurrentFrame() const           = 0;

  bool isFos(int frame) const;
  bool isMos(int frame) const;
  void setFos(int frame, bool on);
  void setMos(int frame, bool on);

  // reimpremented in FilmstripFrameHeadGadget
  bool eventFilter(QObject *obj, QEvent *event) override;
};

class XsheetFrameHeadGadget final : public FrameHeadGadget {
public:
};

class FilmstripFrameHeadGadget final : public FrameHeadGadget {
  FilmstripFrames *m_filmstrip;
  int m_dy;
  int m_highlightedghostFrame;

public:
  FilmstripFrameHeadGadget(FilmstripFrames *filmstrip);

  int getY() const override;
  int index2y(int index) const override;
  int y2index(int y) const override;

  void drawOnionSkinSelection(QPainter &p, const QColor &lightColor,
                              const QColor &darkColor) override;

  void drawShiftTraceMarker(QPainter &p) override;

  void setCurrentFrame(int index) const override;
  int getCurrentFrame() const override;

  bool eventFilter(QObject *obj, QEvent *event) override;
  bool shiftTraceEventFilter(QObject *obj, QEvent *event);
};

#endif
