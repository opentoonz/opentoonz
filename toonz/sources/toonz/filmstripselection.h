#pragma once

#ifndef TFILMSTRIPSELECTION_H
#define TFILMSTRIPSELECTION_H

#include "toonzqt/selection.h"
#include "tfilepath.h"
#include <set>

class TXshSimpleLevel;

//=============================================================================
// TFilmStripSelection
//-----------------------------------------------------------------------------

class TFilmstripSelection final : public TSelection {
public:
  typedef std::pair<TFrameId, TFrameId> InbetweenRange;

private:
  std::set<TFrameId> m_selectedFrames;
  InbetweenRange m_inbetweenRange;

  void updateInbetweenRange();

public:
  TFilmstripSelection();
  ~TFilmstripSelection();

  void enableCommands() override;

  bool isEmpty() const override;

  void selectNone() override;
  void select(const TFrameId &fid, bool selected = true);
  bool isSelected(const TFrameId &fid) const;

  const std::set<TFrameId> &getSelectedFids() const { return m_selectedFrames; }

  InbetweenRange getInbetweenRange() const { return m_inbetweenRange; }
  bool isInInbetweenRange(const TFrameId &fid) const {
    return m_inbetweenRange.first < fid && fid < m_inbetweenRange.second;
  }

  void selectAll();
  void invertSelection();
  void copyFrames();
  void cutFrames();
  void pasteFrames();
  void mergeFrames();
  void pasteInto();
  void deleteFrames();
  void insertEmptyFrames();
  void addFrames();
  void reverseFrames();
  void swingFrames();
  void stepFrames(int step);
  void eachFrames(int step);
  void duplicateFrames();
  void exposeFrames();
  void renumberFrames();
};

#endif  // TFILMSTRIPSELECTION_H
