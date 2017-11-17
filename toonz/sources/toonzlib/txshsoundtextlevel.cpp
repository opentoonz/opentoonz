

#include "toonz/txshsoundtextlevel.h"
#include "toonz/txshleveltypes.h"
#include "tstream.h"

//-----------------------------------------------------------------------------

DEFINE_CLASS_CODE(TXshSoundTextLevel, 54)

PERSIST_IDENTIFIER(TXshSoundTextLevel, "soundTextLevel")

//=============================================================================

TXshSoundTextLevel::TXshSoundTextLevel(std::wstring name)
    : TXshLevel(m_classCode, name), m_framesText() {}

//-----------------------------------------------------------------------------

TXshSoundTextLevel::~TXshSoundTextLevel() {}

//-----------------------------------------------------------------------------

TXshSoundTextLevel *TXshSoundTextLevel::clone() const {
  TXshSoundTextLevel *sound = new TXshSoundTextLevel(m_name);
  return sound;
}

//-----------------------------------------------------------------------------

void TXshSoundTextLevel::setFrameText(int frameIndex, QString text) {
  while (frameIndex >= m_framesText.size()) {
    m_framesText.append(QString(" "));
  }
  m_framesText.replace(frameIndex, text);
}

//-----------------------------------------------------------------------------

QString TXshSoundTextLevel::getFrameText(int frameIndex) const {
  if (frameIndex >= m_framesText.size()) return QString();
  return m_framesText[frameIndex];
}

//-----------------------------------------------------------------------------

void TXshSoundTextLevel::loadData(TIStream &is) {
  is >> m_name;
  setName(m_name);
  std::string tagName;
  int type = UNKNOWN_XSHLEVEL;
  while (is.matchTag(tagName)) {
    if (tagName == "type") {
      std::string v;
      is >> v;
      if (v == "textSound") type = SND_TXT_XSHLEVEL;
      is.matchEndTag();
    } else if (tagName == "frame") {
      std::wstring text;
      is >> text;
      m_framesText.push_back(QString::fromStdWString(text));
      is.matchEndTag();
    } else
      throw TException("unexpected tag " + tagName);
  }
  setType(type);
}

//-----------------------------------------------------------------------------

void TXshSoundTextLevel::saveData(TOStream &os) {
  os << m_name;
  int i;
  for (i = 0; i < m_framesText.size(); i++) {
    os.openChild("frame");
    os << m_framesText[i];
    os.closeChild();
  }
  os.child("type") << L"textSound";
}
