

#include "tlevel.h"
#include "tpalette.h"

DEFINE_CLASS_CODE(TLevel, 7)

//-------------------------------------------------

TLevel::TLevel()
    : TSmartObject(m_classCode)
    , m_name("")
    , m_table(new Table())
    , m_palette(0) {}

//-------------------------------------------------

TLevel::~TLevel() {
  if (m_palette) m_palette->release();
  delete m_table;
}

//-------------------------------------------------

std::string TLevel::getName() const { return m_name; }

//-------------------------------------------------

void TLevel::setName(std::string name) { m_name = name; }

//-------------------------------------------------

const TImageP &TLevel::frame(const TFrameId fid) {
  const static TImageP none;
  assert(m_table);
  Iterator it = m_table->find(fid);
  if (it == m_table->end())
    return none;  // (*m_table)[fid];
  else
    return it->second;
}

//-------------------------------------------------

void TLevel::setFrame(const TFrameId &fid, const TImageP &img) {
  assert(m_table);
  if (img) img->setPalette(getPalette());
  (*m_table)[fid] = img;
}

//-------------------------------------------------

/*
// brutto e inefficiente
int TLevel::getIndex(const TFrameId fid)
{
  int index = 0;
  for(Iterator it = m_table->begin(); it != m_table->end(); it++, index++)
    if(it->first == fid) return index;
  return -1;
}

*/

//-------------------------------------------------

TPalette *TLevel::getPalette() { return m_palette; }

//-------------------------------------------------

void TLevel::setPalette(TPalette *palette) {
  if (m_palette == palette) return;
  if (palette) palette->addRef();
  if (m_palette) m_palette->release();
  m_palette = palette;
  for (Iterator it = begin(); it != end(); ++it) {
    TImageP &img = it->second;
    if (img) img->setPalette(m_palette);
  }
}

//-------------------------------------------------
