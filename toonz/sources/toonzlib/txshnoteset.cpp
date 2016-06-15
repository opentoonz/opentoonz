

#include "toonz/txshnoteset.h"
#include "tstream.h"
#include "texception.h"

#include <QString>

//=============================================================================
// TXshNoteSet

TXshNoteSet::TXshNoteSet() {}

//-----------------------------------------------------------------------------

int TXshNoteSet::addNote(Note note) {
  m_notes.push_back(note);
  return getCount() - 1;
}

//-----------------------------------------------------------------------------

void TXshNoteSet::removeNote(int index) {
  if (m_notes.empty() || index >= (int)m_notes.size()) return;
  m_notes.removeAt(index);
}

//-----------------------------------------------------------------------------

int TXshNoteSet::getCount() const {
  if (m_notes.empty()) return 0;
  return m_notes.size();
}

//-----------------------------------------------------------------------------

int TXshNoteSet::getNoteColorIndex(int noteIndex) const {
  assert(noteIndex < m_notes.size());
  if (noteIndex >= m_notes.size()) return 0;
  return m_notes[noteIndex].m_colorIndex;
}

//-----------------------------------------------------------------------------

void TXshNoteSet::setNoteColorIndex(int noteIndex, int colorIndex) {
  assert(noteIndex < m_notes.size());
  if (noteIndex >= m_notes.size()) return;
  m_notes[noteIndex].m_colorIndex = colorIndex;
}

//-----------------------------------------------------------------------------

QString TXshNoteSet::getNoteHtmlText(int noteIndex) const {
  assert(noteIndex < m_notes.size());
  if (noteIndex >= m_notes.size()) return QString();
  return m_notes[noteIndex].m_text;
}

//-----------------------------------------------------------------------------

void TXshNoteSet::setNoteHtmlText(int noteIndex, QString text) {
  assert(noteIndex < m_notes.size());
  if (noteIndex >= m_notes.size()) return;
  m_notes[noteIndex].m_text = text;
}

//-----------------------------------------------------------------------------

int TXshNoteSet::getNoteRow(int noteIndex) const {
  assert(noteIndex < m_notes.size());
  if (noteIndex >= m_notes.size()) return 0;
  return m_notes[noteIndex].m_row;
}

//-----------------------------------------------------------------------------

void TXshNoteSet::setNoteRow(int noteIndex, int row) {
  assert(noteIndex < m_notes.size());
  if (noteIndex >= m_notes.size()) return;
  m_notes[noteIndex].m_row = row;
}

//-----------------------------------------------------------------------------

int TXshNoteSet::getNoteCol(int noteIndex) const {
  assert(noteIndex < m_notes.size());
  if (noteIndex >= m_notes.size()) return 0;
  return m_notes[noteIndex].m_col;
}

//-----------------------------------------------------------------------------

void TXshNoteSet::setNoteCol(int noteIndex, int col) {
  assert(noteIndex < m_notes.size());
  if (noteIndex >= m_notes.size()) return;
  m_notes[noteIndex].m_col = col;
}

//-----------------------------------------------------------------------------

TPointD TXshNoteSet::getNotePos(int noteIndex) const {
  assert(noteIndex < m_notes.size());
  if (noteIndex >= m_notes.size()) return TPointD(5, 5);
  return m_notes[noteIndex].m_pos;
}

//-----------------------------------------------------------------------------

void TXshNoteSet::setNotePos(int noteIndex, TPointD pos) {
  assert(noteIndex < m_notes.size());
  if (noteIndex >= m_notes.size()) return;
  m_notes[noteIndex].m_pos = pos;
}

//-----------------------------------------------------------------------------

void TXshNoteSet::loadData(TIStream &is) {
  while (!is.eos()) {
    std::string tagName;
    if (is.matchTag(tagName)) {
      if (tagName == "notes") {
        while (!is.eos()) {
          std::string tagName;
          if (is.matchTag(tagName)) {
            if (tagName == "note") {
              Note note;
              is >> note.m_colorIndex;
              std::wstring text;
              is >> text;
              note.m_text = QString::fromStdWString(text);
              is >> note.m_row;
              is >> note.m_col;
              is >> note.m_pos.x;
              is >> note.m_pos.y;
              m_notes.push_back(note);
            }
          } else
            throw TException("expected <note>");
          is.closeChild();
        }
      } else
        throw TException("expected <defaultColor> or <notes>");
      is.closeChild();
    } else
      throw TException("expected tag");
  }
}

//-----------------------------------------------------------------------------

void TXshNoteSet::saveData(TOStream &os) {
  int i;
  os.openChild("notes");
  for (i = 0; i < getCount(); i++) {
    os.openChild("note");
    Note note = m_notes.at(i);
    os << note.m_colorIndex;
    os << note.m_text.toStdWString();
    os << note.m_row;
    os << note.m_col;
    os << note.m_pos.x;
    os << note.m_pos.y;
    os.closeChild();
  }
  os.closeChild();
}
