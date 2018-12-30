#pragma once

#ifndef ONION_SKIN_MASK_INCLUDED
#define ONION_SKIN_MASK_INCLUDED

// TnzCore includes
#include "tcommon.h"
#include "tgeometry.h"

#include <QList>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//====================================================

//    Forward declarations

class TFrameId;
class TXshSimpleLevel;

//====================================================

//***************************************************************************
//    OnionSkinMask  declaration
//***************************************************************************

/*!
  OnionSkinMask is the class encapsulating onion skin data in Toonz.
*/

class DVAPI OnionSkinMask {
public:
  enum ShiftTraceStatus {
    DISABLED,
    EDITING_GHOST,
    ENABLED,
    ENABLED_WITHOUT_GHOST_MOVEMENTS
  };

public:
  OnionSkinMask() : m_enabled(false), m_wholeScene(false) {}

  void clear();

  //! Fills in the output vector with the absolute frames to be onion-skinned.
  void getAll(int currentRow, std::vector<int> &output) const;

  int getMosCount() const {
    return m_mos.size();
  }  //!< Returns the Mobile OS frames count
  int getFosCount() const {
    return m_fos.size();
  }  //!< Returns the Fixed OS frames count

  int getMos(int index) const {
    assert(0 <= index && index < (int)m_mos.size());
    return m_mos[index];
  }

  int getFos(int index) const {
    assert(0 <= index && index < (int)m_fos.size());
    return m_fos[index];
  }

  void setMos(int drow, bool on);  //!< Sets a Mobile OS frame shifted by drow
                                   //! around current xsheet frame
  void setFos(int row,
              bool on);  //!< Sets a Fixed OS frame to the specified xsheet row

  bool isMos(int drow) const;
  bool isFos(int row) const;

  bool getMosRange(int &drow0, int &drow1) const;

  bool isEmpty() const { return m_mos.empty() && m_fos.empty(); }

  bool isEnabled() const { return m_enabled; }
  void enable(bool on) { m_enabled = on; }

  bool isWholeScene() const { return m_wholeScene; }
  void setIsWholeScene(bool wholeScene) { m_wholeScene = wholeScene; }

  /*!
Returns the fade (transparency) value, in the [0.0, 1.0] range, corresponding to
the specified
distance from current frame. In case distance == 0, the returned value is 0.9,
ie \a almost opaque,
since underlying onion-skinned drawings must be visible.
*/
  static double getOnionSkinFade(int distance);

  // Shift & Trace  stuff

  ShiftTraceStatus getShiftTraceStatus() const { return m_shiftTraceStatus; }
  void setShiftTraceStatus(ShiftTraceStatus status) {
    m_shiftTraceStatus = status;
  }

  bool isShiftTraceEnabled() const { return m_shiftTraceStatus != DISABLED; }

  const TAffine getShiftTraceGhostAff(int index) const {
    return m_ghostAff[index];
  }
  void setShiftTraceGhostAff(int index, const TAffine &aff);

  const TPointD getShiftTraceGhostCenter(int index) const {
    return m_ghostCenter[index];
  }
  void setShiftTraceGhostCenter(int index, const TPointD &center);

  const int getShiftTraceGhostFrameOffset(int index) {
    return m_ghostFrame[index];
  }
  void setShiftTraceGhostFrameOffset(int index, int offset) {
    m_ghostFrame[index] = offset;
  }

  const int getGhostFlipKey() {
    return (m_ghostFlipKeys.isEmpty()) ? 0 : m_ghostFlipKeys.last();
  }
  void appendGhostFlipKey(int key) {
    m_ghostFlipKeys.removeAll(key);
    m_ghostFlipKeys.append(key);
  }
  void removeGhostFlipKey(int key) { m_ghostFlipKeys.removeAll(key); }
  void clearGhostFlipKey() { m_ghostFlipKeys.clear(); }

private:
  std::vector<int> m_fos, m_mos;  //!< Fixed and Mobile Onion Skin indices
  bool m_enabled;                 //!< Whether onion skin is enabled
  bool m_wholeScene;              //!< Whether the OS works on the entire scene

  ShiftTraceStatus m_shiftTraceStatus;
  TAffine m_ghostAff[2];
  TPointD m_ghostCenter[2];
  int m_ghostFrame[2];         // relative frame position of the ghosts
  QList<int> m_ghostFlipKeys;  // If F1, F2 or F3 key is pressed, then only
                               // display the corresponding ghost
};

//***************************************************************************
//    OnionSkinMaskModifier  declaration
//***************************************************************************

class DVAPI OnionSkinMaskModifier {
public:
  OnionSkinMaskModifier(OnionSkinMask mask, int currentRow);

  const OnionSkinMask &getMask() const { return m_curMask; }

  void click(int row, bool isFos);
  void drag(int row);
  void release(int row);

private:
  OnionSkinMask m_oldMask, m_curMask;

  int m_firstRow, m_lastRow;
  int m_curRow;

  int m_status;
};

#endif  // ONION_SKIN_MASK_INCLUDED
