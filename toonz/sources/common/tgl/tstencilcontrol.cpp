

#include "tstencilcontrol.h"
#include "tgl.h"
#include "tthreadmessage.h"
#include <stack>

#include <QThreadStorage>

//-----------------------------------------------------------------------------------------

namespace {

// singleton
class StencilControlManager {
  QThreadStorage<TStencilControl *> m_storage;

  StencilControlManager() {}

public:
  static StencilControlManager *instance() {
    static StencilControlManager theInstance;
    return &theInstance;
  }

  TStencilControl *getCurrentStencilControl() {
    if (!m_storage.hasLocalData()) {
      m_storage.setLocalData(new TStencilControl);
    }

    return m_storage.localData();
  }

  ~StencilControlManager() {}
};

}  // Local namepace

//-----------------------------------------------------------------------------------------

class TStencilControl::Imp {
public:
  int m_stencilBitCount;
  int m_pushCount;
  int m_currentWriting;  // current stencil bit plane.
  // 0 is the first bit plane ; -1 menas no stencil mask is writing

  int m_virtualState;
// the state of the (eventually virtual) top mask.
// A mask is virtual if overflows stencil buffer
// 0 is closed and disabled, 1 closed and enabled and 2 is opened

#ifdef _DEBUG
  std::stack<bool> fullState;
// state of each mask in the stack (except top mask).
// 'true' means opend; 'false' means close and enabled
// Used only for assert
#endif

  unsigned char m_writingMask;  // bit mask. The i-th bit=1 iff the i-th mask is
                                // opened to write
  unsigned char m_drawOnScreenMask;  // bitmsk.The ith bit=1 iff the ith mask
                                     // WRITE also ON SCREEN WHEN WRITE ON
                                     // STENCIL BIT PLANE
  unsigned char
      m_enabledMask;  // bit mask. The i-th bit=1 iff the i-th mask is enabled
  unsigned char
      m_inOrOutMask;  // bit mask. The i-th bit=1 iff the i-th mask is inside
  unsigned char m_drawOnlyOnceMask;

  Imp();

  void updateOpenGlState();

  void pushMask();
  // make a new stencil plane the current one.
  // if there is no plane available, increase a counter and does not push
  // (virtual masks)
  // So the same number of pop has no effect

  void popMask();
  // assert if the stencil stack contains only 0

  void beginMask(DrawMode drawMode);
  // clear the current stencil plane; start writing to it
  // if drawOnScreen is not 0, it writes also to the color buffer (or stencil
  // plane if another
  // mask is open). If drawOnScreen is 2, it drows only once (by stencil buffer)

  void endMask();
  // end writing to the stencil plane.

  void enableMask(MaskType maskType);
  void disableMask();
  // between enableMask()/disableMask() drawing is filtered by the values of the
  // current
  // stencil plane
  // n.b. enableMask()/disableMask() can be nidified. Between the inner pair
  // writing is enabled
  // according to the AND of all of them
};

//-----------------------------------------------------------------------------------------

TStencilControl::Imp::Imp()
    : m_stencilBitCount(0)
    , m_pushCount(1)
    , m_currentWriting(-1)
    , m_enabledMask(0)
    , m_writingMask(0)
    , m_inOrOutMask(0)
    , m_drawOnScreenMask(0)
    , m_drawOnlyOnceMask(0)
    , m_virtualState(0) {
  glGetIntegerv(GL_STENCIL_BITS, (GLint *)&m_stencilBitCount);

  glStencilMask(0xFFFFFFFF);
  // glClearStencil(0);
  glClear(GL_STENCIL_BUFFER_BIT);
}

//---------------------------------------------------------

TStencilControl *TStencilControl::instance() {
  StencilControlManager *instance = StencilControlManager::instance();
  return instance->getCurrentStencilControl();
}

//---------------------------------------------------------

TStencilControl::TStencilControl() : m_imp(new Imp) {}

//---------------------------------------------------------

TStencilControl::~TStencilControl() {}

//---------------------------------------------------------

void TStencilControl::Imp::updateOpenGlState() {
  if (m_currentWriting >= 0) {  // writing on stencil buffer
    unsigned char currentWritingMask = 1 << m_currentWriting;

    bool drawOnlyOnce = (currentWritingMask & m_drawOnlyOnceMask) != 0;

    if (currentWritingMask & m_drawOnScreenMask) {
      unsigned char lastWritingMask;
      int lastWriting = m_currentWriting - 1;
      for (; lastWriting >= 0; lastWriting--) {
        lastWritingMask = 1 << lastWriting;
        if ((lastWritingMask & m_writingMask) == lastWritingMask) break;
      }
      if (lastWriting < 0) {
        // glColorMask(1,1,1,1);

        if (drawOnlyOnce)
          m_enabledMask |= currentWritingMask;
        else
          m_enabledMask &= ~currentWritingMask;
      } else {
        tglMultColorMask(0, 0, 0, 0);
        // glDrawBuffer(GL_NONE);

        drawOnlyOnce = false;  // essendo solo un'effetto visivo, se sto
                               // scrivendo su una maschera e non a schermo, e'
                               // inutile
        currentWritingMask |= lastWritingMask;
      }
    } else
      tglMultColorMask(0, 0, 0, 0);
    // glDrawBuffer(GL_NONE);

    glStencilMask(currentWritingMask);

    if (drawOnlyOnce) {
      glStencilFunc(GL_EQUAL, m_inOrOutMask, m_enabledMask);
      glStencilOp(GL_KEEP, GL_INVERT, GL_INVERT);
    } else {
      glStencilFunc(GL_EQUAL, currentWritingMask | m_inOrOutMask,
                    m_enabledMask);
      glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
    }

  } else {  // writing on screen buffer
    glStencilMask(0xFFFFFFFF);
    glStencilFunc(GL_EQUAL, m_inOrOutMask, m_enabledMask);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    // glColorMask(1,1,1,1);
  }

  if (!m_enabledMask && m_currentWriting < 0)
    glDisable(GL_STENCIL_TEST);
  else
    glEnable(GL_STENCIL_TEST);
}

//---------------------------------------------------------

void TStencilControl::Imp::pushMask() { m_pushCount++; }

//---------------------------------------------------------

void TStencilControl::Imp::beginMask(DrawMode drawMode) {
  m_currentWriting          = m_pushCount - 1;
  unsigned char currentMask = 1 << m_currentWriting;

  m_writingMask |= currentMask;

  if (drawMode == DRAW_ALSO_ON_SCREEN) {
    m_drawOnScreenMask |= currentMask;
  } else if (drawMode == DRAW_ON_SCREEN_ONLY_ONCE) {
    m_drawOnScreenMask |= currentMask;
    m_drawOnlyOnceMask |= currentMask;
  } else {
    m_drawOnScreenMask &= ~currentMask;
    m_drawOnlyOnceMask &= ~currentMask;
  }

  glEnable(GL_STENCIL_TEST);
  glStencilMask(currentMask);      // enabled to modify only current bitPlane
  glClear(GL_STENCIL_BUFFER_BIT);  // and clear it
  updateOpenGlState();
}

//---------------------------------------------------------

void TStencilControl::Imp::endMask() {
  assert(m_pushCount - 1 == m_currentWriting);

  unsigned char currentMask = 1 << (m_pushCount - 1);

  m_writingMask &= ~currentMask;  // stop writing
  m_enabledMask &= ~currentMask;
  m_drawOnScreenMask &= ~currentMask;
  m_drawOnlyOnceMask &= ~currentMask;

  //----------------------------------------------------------

  m_currentWriting--;
  for (; m_currentWriting >= 0; m_currentWriting--) {
    unsigned char currentWritingMask = 1 << m_currentWriting;
    if ((currentWritingMask & m_writingMask) == currentWritingMask) break;
  }

  updateOpenGlState();
}

//---------------------------------------------------------

void TStencilControl::Imp::enableMask(MaskType maskType) {
  unsigned char currentMask = 1 << (m_pushCount - 1);

  if ((m_enabledMask & currentMask) == 0) glPushAttrib(GL_ALL_ATTRIB_BITS);

  m_enabledMask |= currentMask;

  assert(maskType == SHOW_INSIDE || maskType == SHOW_OUTSIDE);

  if (maskType == SHOW_INSIDE)
    m_inOrOutMask |= currentMask;
  else
    m_inOrOutMask &= ~currentMask;

  updateOpenGlState();
}

//---------------------------------------------------------

void TStencilControl::Imp::disableMask() {
  unsigned char currentMask = 1 << (m_pushCount - 1);

  m_enabledMask &= ~currentMask;
  m_inOrOutMask &= ~currentMask;

  updateOpenGlState();
  glPopAttrib();
}

//---------------------------------------------------------

void TStencilControl::Imp::popMask() {
  --m_pushCount;
  assert(m_pushCount > 0);  // there is at least one mask in the stack
}

//---------------------------------------------------------
//---------------------------------------------------------

// questo forse e' un po' brutto:
// La maschera e' chiusa.
// Se e' abilitata,    open = push+open
// Se e' disabilitata, open =      open

void TStencilControl::beginMask(DrawMode drawMode) {
  glPushAttrib(GL_ALL_ATTRIB_BITS);

  if (m_imp->m_virtualState)  // opened or enabled
  {
#ifdef _DEBUG
    m_imp->fullState.push(m_imp->m_virtualState == 2);
#endif

    m_imp->pushMask();
  }

  m_imp->m_virtualState = 2;  // opened

  if (m_imp->m_pushCount <= m_imp->m_stencilBitCount)
    m_imp->beginMask(drawMode);
}

//---------------------------------------------------------

void TStencilControl::endMask() {
  if (!m_imp->m_virtualState)  // closed and disabled
  {
    m_imp->popMask();

#ifdef _DEBUG
    assert(m_imp->fullState.top());  // the state before last push must be
                                     // opened
    m_imp->fullState.pop();
#endif
  }

  assert(m_imp->m_virtualState != 1);  // yet closed

  m_imp->m_virtualState = 0;

  if (m_imp->m_pushCount <= m_imp->m_stencilBitCount) m_imp->endMask();

  glPopAttrib();
}

//---------------------------------------------------------

void TStencilControl::enableMask(MaskType maskType) {
  assert(m_imp->m_virtualState != 2);  // cannot enable an opened mask

  m_imp->m_virtualState = 1;  // enabled

  if (m_imp->m_pushCount <= m_imp->m_stencilBitCount)
    m_imp->enableMask(maskType);
}

//---------------------------------------------------------

void TStencilControl::disableMask() {
  assert(m_imp->m_virtualState != 2);  // cannot disable an opened mask

  if (!m_imp->m_virtualState)  // closed and disabled
  {
    m_imp->popMask();

#ifdef _DEBUG
    assert(
        !m_imp->fullState.top());  // the state before last push must be enabled
    m_imp->fullState.pop();
#endif
  }

  m_imp->m_virtualState = 0;  // close and disabled

  if (m_imp->m_pushCount <= m_imp->m_stencilBitCount) m_imp->disableMask();
}

//---------------------------------------------------------
