#pragma once

#ifndef TXSHNOTESET_INCLUDED
#define TXSHNOTESET_INCLUDED

#include <QList>
#include <QString>
#include "tgeometry.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
// forward declarations
class TIStream;
class TOStream;

//=============================================================================
// TXshNoteSet

class DVAPI TXshNoteSet {
public:
  struct Note {
    //! Default colors are defined in TSceneProperties
    int m_colorIndex;
    //! Is html test, contains font inforamtion.
    QString m_text;
    int m_row;
    int m_col;
    //! Top left point of note rect in cell.
    TPointD m_pos;

    Note()
        : m_colorIndex(0), m_text(), m_row(0), m_col(0), m_pos(TPointD(5, 5)) {}
    ~Note() {}
  };
  QList<Note> m_notes;

  TXshNoteSet();
  ~TXshNoteSet() {}

  int addNote(Note note);
  void removeNote(int index);

  int getCount() const;

  int getNoteColorIndex(int noteIndex) const;
  void setNoteColorIndex(int noteIndex, int colorIndex);

  //! Return html text, text with font information.
  QString getNoteHtmlText(int noteIndex) const;
  void setNoteHtmlText(int noteIndex, QString text);

  int getNoteRow(int noteIndex) const;
  void setNoteRow(int noteIndex, int row);

  int getNoteCol(int noteIndex) const;
  void setNoteCol(int noteIndex, int col);

  TPointD getNotePos(int noteIndex) const;
  void setNotePos(int noteIndex, TPointD pos);

  void loadData(TIStream &is);
  void saveData(TOStream &os);
};

#endif  // TXSHNOTESET_INCLUDED
