#pragma once

#ifndef STROKE_SELECTION_H
#define STROKE_SELECTION_H

#include <memory>

// TnzQt includes
#include "toonzqt/selection.h"

// TnzCore includes
#include "tcommon.h"
#include "tvectorimage.h"

// STD includes
#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================

//    Forward declarations

class TGroupCommand;
class TSceneHandle;

//=========================================================

//****************************************************************************
//    StrokeSelection  declaration
//****************************************************************************

class DVAPI StrokeSelection final : public TSelection {
public:
  typedef std::set<int> IndexesContainer;

public:
  StrokeSelection();
  ~StrokeSelection();

  StrokeSelection(const StrokeSelection &other);
  StrokeSelection &operator=(const StrokeSelection &other);

  TGroupCommand *getGroupCommand() { return m_groupCommand.get(); }

  void setImage(const TVectorImageP &image) { m_vi = image; }
  const TVectorImageP &getImage() const { return m_vi; }

  const IndexesContainer &getSelection() const { return m_indexes; }
  IndexesContainer &getSelection() { return m_indexes; }

  bool isEmpty() const override { return m_indexes.empty(); }
  bool updateSelectionBBox() const { return m_updateSelectionBBox; }
  bool isSelected(int index) const { return (m_indexes.count(index) > 0); }
  void select(int index, bool on);
  void toggle(int index);
  void selectNone() override { m_indexes.clear(); }

  void setSceneHandle(TSceneHandle *tsh) { m_sceneHandle = tsh; }

  bool isEditable();

public:
  // Commands

  void changeColorStyle(int styleIndex);
  void deleteStrokes();
  void copy();
  void cut();
  void paste();

  void removeEndpoints();

  void enableCommands() override;

  void selectAll();

private:
  TVectorImageP m_vi;          //!< Selected vector image.
  IndexesContainer m_indexes;  //!< Selected stroke indexes in m_vi.

  std::unique_ptr<TGroupCommand> m_groupCommand;  //!< Groups commands wrapper.
  TSceneHandle *m_sceneHandle;  //!< Global scene handle. \deprecated  Use
                                //! TApplication instead.

  /*!Set this boolean to true before call tool->notifyImageChanged() when you
want to reset strokes bbox.
N.B. after call tool->notifyImageChanged() set m_updateSelectionBBox to false.*/
  bool m_updateSelectionBBox;
};

#endif  // STROKE_SELECTION_H
