

#include "toonz/txshlevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/hook.h"

//-----------------------------------------------------------------------------

TXshLevel::TXshLevel(ClassCode code, std::wstring name)
    : TSmartObject(code)
    , m_name(name)
    , m_type(UNKNOWN_XSHLEVEL)
    , m_hookSet(new HookSet())
    , m_scene(0) {
  updateShortName();
}

//-----------------------------------------------------------------------------

TXshLevel::~TXshLevel() { delete m_hookSet; }

//-----------------------------------------------------------------------------

void TXshLevel::setScene(ToonzScene *scene) { m_scene = scene; }

//-----------------------------------------------------------------------------

void TXshLevel::setName(std::wstring name) {
  m_name = name;
  updateShortName();
}

//-----------------------------------------------------------------------------

void TXshLevel::updateShortName() {
  if (m_name.size() < 5)
    m_shortName = m_name;
  else
    m_shortName = m_name.substr(0, 4) + L"~";
}
