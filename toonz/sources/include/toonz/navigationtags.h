#pragma once

#ifndef NAVIGATION_TAGS_INCLUDED
#define NAVIGATION_TAGS_INCLUDED

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include <QString>
#include <QColor>

class TOStream;
class TIStream;

class DVAPI NavigationTags {
public:
  struct Tag {
    int m_frame;
    QString m_label;
    QColor m_color;
    Tag() : m_frame(-1), m_label(), m_color(Qt::magenta) {}
    Tag(int frame) : m_frame(frame), m_label(), m_color(Qt::magenta) {}
    Tag(int frame, QString label)
        : m_frame(frame), m_label(label), m_color(Qt::magenta) {}
    Tag(int frame, QString label, QColor color)
        : m_frame(frame), m_label(label), m_color(color) {}
    ~Tag() {}

    bool operator<(const Tag &otherTag) const {
      return (m_frame < otherTag.m_frame);
    }
  };
  std::vector<Tag> m_tags;

  QColor m_lastTagColorUsed;

  NavigationTags();
  ~NavigationTags() {}

  std::vector<Tag> getTags() { return m_tags; }

  int getCount() const;

  Tag getTag(int frame);
  void addTag(int frame, QString label = "");
  void removeTag(int frame);
  void clearTags();
  bool isTagged(int frame);
  int getPrevTag(int currentFrame);
  int getNextTag(int currentFrame);
  void moveTag(int fromFrame, int toFrame);
  void shiftTags(int startFrame, int shift);

  QString getTagLabel(int frame);
  void setTagLabel(int frame, QString label);

  QColor getTagColor(int frame);
  void setTagColor(int frame, QColor color);

  void saveData(TOStream &os);
  void loadData(TIStream &is);
};

#endif
