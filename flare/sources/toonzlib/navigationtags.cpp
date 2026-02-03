#include "toonz/navigationtags.h"

#include "tstream.h"
#include "texception.h"
#include "tenv.h"

#ifndef _WIN32
#include <limits.h>
#endif

TEnv::IntVar NavigationTagLastColorR("NavigationTagLastColorR", 255);
TEnv::IntVar NavigationTagLastColorG("NavigationTagLastColorG", 0);
TEnv::IntVar NavigationTagLastColorB("NavigationTagLastColorB", 255);

NavigationTags::NavigationTags() {
  m_lastTagColorUsed = QColor(NavigationTagLastColorR, NavigationTagLastColorG,
                              NavigationTagLastColorB);
}

//-----------------------------------------------------------------------------

int NavigationTags::getCount() const {
  if (m_tags.empty()) return 0;
  return m_tags.size();
}

//-----------------------------------------------------------------------------

NavigationTags::Tag NavigationTags::getTag(int frame) {
  for (int i = 0; i < m_tags.size(); i++)
    if (m_tags[i].m_frame == frame) return m_tags[i];

  return Tag();
}

//-----------------------------------------------------------------------------

void NavigationTags::addTag(int frame, QString label) {
  if (frame < 0 || isTagged(frame)) return;

  m_tags.push_back(Tag(frame, label, m_lastTagColorUsed));

  std::sort(m_tags.begin(), m_tags.end());
}

//-----------------------------------------------------------------------------

void NavigationTags::removeTag(int frame) {
  if (frame < 0) return;

  Tag tag = getTag(frame);
  if (tag.m_frame == -1) return;

  std::vector<Tag>::iterator it;
  for (it = m_tags.begin(); it != m_tags.end(); it++)
    if (it->m_frame == frame) {
      m_tags.erase(it);
      break;
    }
}

//-----------------------------------------------------------------------------

void NavigationTags::clearTags() { m_tags.clear(); }

//-----------------------------------------------------------------------------

bool NavigationTags::isTagged(int frame) {
  if (frame < 0) return false;

  for (int i = 0; i < m_tags.size(); i++)
    if (m_tags[i].m_frame == frame) return true;

  return false;
}

//-----------------------------------------------------------------------------

void NavigationTags::moveTag(int fromFrame, int toFrame) {
  if (fromFrame < 0 || toFrame < 0 || isTagged(toFrame)) return;

  for (int i = 0; i < m_tags.size(); i++)
    if (m_tags[i].m_frame == fromFrame) {
      m_tags[i].m_frame = toFrame;
      std::sort(m_tags.begin(), m_tags.end());
      break;
    }
}

//-----------------------------------------------------------------------------

// WARNING: When shifting left, shiftTag callers MUST take care of shifting tags
// to frame < 0 or handle possible frame collisions.  This will not do it
// for you!
void NavigationTags::shiftTags(int startFrame, int shift) {
  if (!m_tags.size()) return;

  for (int i = 0; i < m_tags.size(); i++) {
    if (m_tags[i].m_frame < startFrame) continue;
    m_tags[i].m_frame += shift;
  }
}

//-----------------------------------------------------------------------------

int NavigationTags::getPrevTag(int currentFrame) {
  if (currentFrame < 0) return -1;

  int index        = -1;
  int closestFrame = -1;
  for (int i = 0; i < m_tags.size(); i++)
    if (m_tags[i].m_frame < currentFrame && m_tags[i].m_frame > closestFrame) {
      index        = i;
      closestFrame = m_tags[i].m_frame;
    }

  return index >= 0 ? m_tags[index].m_frame : -1;
}

//-----------------------------------------------------------------------------

int NavigationTags::getNextTag(int currentFrame) {
  int index        = -1;
  int closestFrame = INT_MAX;
  for (int i = 0; i < m_tags.size(); i++)
    if (m_tags[i].m_frame > currentFrame && m_tags[i].m_frame < closestFrame) {
      index        = i;
      closestFrame = m_tags[i].m_frame;
    }

  return index >= 0 ? m_tags[index].m_frame : -1;
}

//-----------------------------------------------------------------------------

QString NavigationTags::getTagLabel(int frame) {
  Tag tag = getTag(frame);

  return tag.m_label;
}

//-----------------------------------------------------------------------------

void NavigationTags::setTagLabel(int frame, QString label) {
  if (frame < 0) return;

  for (int i = 0; i < m_tags.size(); i++)
    if (m_tags[i].m_frame == frame) {
      m_tags[i].m_label = label;
      break;
    }
}

//-----------------------------------------------------------------------------

QColor NavigationTags::getTagColor(int frame) {
  Tag tag = getTag(frame);

  return (tag.m_frame == -1) ? m_lastTagColorUsed : tag.m_color;
}

//-----------------------------------------------------------------------------

void NavigationTags::setTagColor(int frame, QColor color) {
  if (frame < 0) return;

  for (int i = 0; i < m_tags.size(); i++)
    if (m_tags[i].m_frame == frame) {
      m_tags[i].m_color = color;
      break;
    }

  m_lastTagColorUsed      = color;
  NavigationTagLastColorR = color.red();
  NavigationTagLastColorG = color.green();
  NavigationTagLastColorB = color.blue();
}

//-----------------------------------------------------------------------------

void NavigationTags::saveData(TOStream &os) {
  int i;
  os.openChild("Tags");
  for (i = 0; i < getCount(); i++) {
    os.openChild("tag");
    Tag tag = m_tags.at(i);
    os << tag.m_frame;
    os << tag.m_label;
    os << tag.m_color.red();
    os << tag.m_color.green();
    os << tag.m_color.blue();
    os.closeChild();
  }
  os.closeChild();
}

//-----------------------------------------------------------------------------

void NavigationTags::loadData(TIStream &is) {
  while (!is.eos()) {
    std::string tagName;
    if (is.matchTag(tagName)) {
      if (tagName == "Tags") {
        while (!is.eos()) {
          std::string tagName;
          if (is.matchTag(tagName)) {
            if (tagName == "tag") {
              Tag tag;
              is >> tag.m_frame;
              std::wstring text;
              is >> text;
              tag.m_label = QString::fromStdWString(text);
              int r, g, b;
              is >> r;
              is >> g;
              is >> b;
              tag.m_color = QColor(r, g, b);
              m_tags.push_back(tag);
            }
          } else
            throw TException("expected <tag>");
          is.closeChild();
        }
      } else
        throw TException("expected <Tags>");
      is.closeChild();
    } else
      throw TException("expected tag");
  }
}
