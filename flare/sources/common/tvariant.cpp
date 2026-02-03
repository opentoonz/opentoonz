
#include <tvariant.h>

#include <sstream>
#include <iostream>
#include <cstdio>


//---------------------------------------------------------

int
TVariantPath::compare(
  const TVariantPath &a, int beginA,
  const TVariantPath &b, int beginB, int count )
{
  assert(beginA >= 0 && beginA <= (int)a.size());
  assert(beginB >= 0 && beginB <= (int)b.size());
  if (count == 0) return 0;
  int countA = std::min(count, (int)a.size() - beginA);
  int countB = std::min(count, (int)b.size() - beginB);
  count = std::min(countA, countB);

  TVariantPath::const_iterator ia = a.begin() + beginA;
  TVariantPath::const_iterator ib = b.begin() + beginB;
  for(int i = 0; i < count; ++i, ++ia, ++ib)
    if ((*ia) < (*ib)) return -1; else
      if ((*ib) < (*ia)) return 1;
  return countA < countB ? -1
       : countB < countA ?  1 : 0;
}

//---------------------------------------------------------

void
TVariant::setParentForChilds() {
  if (m_type == List) {
    for(TVariantList::iterator i = m_list.begin(); i != m_list.end(); ++i)
      i->setParent(*this);
  } else
  if (m_type == Map) {
    for(TVariantMap::iterator i = m_map.begin(); i != m_map.end(); ++i)
      i->second.setParent(*this, i->first);
  }
}

//---------------------------------------------------------

const TVariant&
TVariant::blank() {
  static const TVariant blank;
  return blank;
}

//---------------------------------------------------------

void
TVariant::resize(int size) {
  setType(List);
  int prevSize = (int)m_list.size();
  if (prevSize == size) return;
  m_list.resize(size);
  if (prevSize < size)
    for(TVariantList::iterator i = m_list.begin() + prevSize; i != m_list.end(); ++i)
      i->setParent(*this);
  touch();
}

//---------------------------------------------------------

void
TVariant::insert(int index, const TVariant &v) {
  resize(std::max((int)m_list.size(), index));
  m_list.insert(m_list.begin() + index, v);
  m_list[index].setParent(*this);
  touch();
}

//---------------------------------------------------------

void
TVariant::remove(int index) {
  if (m_type == List && index >= 0 && index < (int)m_list.size())
    { m_list.erase(m_list.begin() + index); touch(); }
}

//---------------------------------------------------------

TVariant&
TVariant::operator[] (int index) {
  setType(List);
  assert(index >= 0);
  int prevSize = (int)m_list.size();
  if (index >= prevSize) {
    m_list.resize(index + 1);
    for(TVariantList::iterator i = m_list.begin() + prevSize; i != m_list.end(); ++i)
      i->setParent(*this);
    touch();
  }
  return m_list[index];
}

//---------------------------------------------------------

TVariant&
TVariant::operator[] (const TStringId &field) {
  setType(Map);
  TVariant &result = m_map[field];
  if (!result.m_parent) {
    result.setParent(*this, field);
    touch();
  }
  return result;
}

//---------------------------------------------------------

bool
TVariant::remove(const TStringId &field) {
  if (m_type == Map && m_map.erase(field))
    { touch(); return true; }
  return false;
}

//---------------------------------------------------------

const TVariant&
TVariant::byPath(const TVariantPath &path, int begin, int end) const {
  if ((int)path.size() <= begin || begin >= end) return *this;
  if (isNone()) return blank();
  return (*this)[path[begin]].byPath(path, begin + 1, end);
}

//---------------------------------------------------------

TVariant&
TVariant::byPath(const TVariantPath &path, int begin, int end) {
  if ((int)path.size() <= begin || begin >= end) return *this;
  return (*this)[path[begin]].byPath(path, begin + 1, end);
}

//---------------------------------------------------------

int
TVariant::getParentPathSize(const TVariant &parent) const {
  int ac = 0;
  for(const TVariant *a = this; a; a = a->parent(), ++ac)
    if (a == &parent) return ac;
  return -1;
}

//---------------------------------------------------------

bool
TVariant::getParentPath(TVariantPath &outPath, const TVariant &parent) const {
  if (!m_parent)
    { outPath.clear(); return false; }
  if (m_parent == this)
    { outPath.clear(); return true; }
  if (m_parent->getParentPath(outPath))
    { outPath.push_back(parentPathEntry()); return true; }
  return false;
}

//---------------------------------------------------------

bool
TVariant::getChildPathEntry(const TVariant &child, TVariantPathEntry &outEntry) const {
  for(const TVariant *a = &child; a->parent(); a = a->parent())
    if (a->parent() == this)
      { outEntry = a->parentPathEntry(); return true; }
  outEntry = TVariantPathEntry();
  return false;
}

//---------------------------------------------------------

bool
TVariant::isChildOf(const TVariant &other) const {
  for(const TVariant *a = this->m_parent; a; a = a->m_parent)
    if (a == &other) return true;
  return false;
}

//---------------------------------------------------------

bool
TVariant::isChildOrEqual(const TVariant &other) const {
  for(const TVariant *a = this; a; a = a->m_parent)
    if (a == &other) return true;
  return false;
}

//---------------------------------------------------------

const TVariant*
TVariant::findCommonParent(const TVariant &other) const {
  if (m_root != other.m_root) return NULL;
  const TVariant *a = this, *b = &other;
  int ac = 0, bc = 0;
  while(a->m_parent) a = a->m_parent, ++ac;
  while(b->m_parent) b = b->m_parent, ++bc;

  a = this, b = &other;
  while(ac > bc) a = a->m_parent, --ac;
  while(bc > ac) b = b->m_parent, --bc;

  while(true) {
    if (a == b) return a;
    if (ac == 0) break;
    --ac, a = a->m_parent, b = b->m_parent;
  }

  return NULL;
}

//---------------------------------------------------------

size_t
TVariant::getMemSize() const {
  size_t s = sizeof(*this);
  for(TVariantList::const_iterator i = m_list.begin(); i != m_list.end(); ++i)
    s += i->getMemSize();
  for(TVariantMap::const_iterator i = m_map.begin(); i != m_map.end(); ++i)
    s += sizeof(*i) - sizeof(*this) + i->second.getMemSize();
  return s;
}

//---------------------------------------------------------

void
TVariant::toStream(std::ostream &stream, bool pretty, int level) const {
  struct Writer {
    const TVariant &data;
    std::ostream &stream;
    bool pretty;
    int level;

    Writer(const TVariant &data, std::ostream &stream, bool pretty, int level):
      data(data), stream(stream), pretty(pretty), level(level) { }

    void writeNewLine()
      { if (pretty) stream << std::endl; }
    void writeSpace()
      { if (pretty) stream << " "; }
    void writeTab(int level)
      { if (pretty) for(int i = 2*level; i; --i) stream << " "; }
    void writeChar(char c)
      { stream.put(c); }
    void writeWord(const char *word)
      { stream << word; }

    void writeString(const std::string &str) {
      writeWord("\"");
      for(const char *c = str.c_str(); *c; ++c) {
        switch (*c) {
        case '\"': writeWord("\\\""); break;
        case '\\': writeWord("\\\\"); break;
        case '\b': writeWord("\\b"); break;
        case '\f': writeWord("\\f"); break;
        case '\n': writeWord("\\n"); break;
        case '\r': writeWord("\\r"); break;
        case '\t': writeWord("\\t"); break;
        default: writeChar(*c); break;
        }
      }
      writeWord("\"");
    }

    void writeDouble(double x) {
      char buf[256];
      snprintf(buf, sizeof(buf), "%.12lg", x);
      stream << buf;
    }

    void writeList(const TVariantList &list) {
      writeWord("[");
      if (!list.empty()) {
        writeNewLine();
        TVariantList::const_iterator i = list.begin();
        while(true) {
          writeTab(level + 1);
          i->toStream(stream, pretty, level + 1);
          if (++i == list.end()) { writeNewLine(); break; }
          writeWord(",");
          writeNewLine();
        }
        writeTab(level);
      } else writeSpace();
      writeWord("]");
    }

    void writeMap(const TVariantMap &map) {
      writeWord("{");
      if (!map.empty()) {
        writeNewLine();
        TVariantMap::const_iterator i = map.begin();
        while(true) {
          writeTab(level + 1);
          writeString(i->first.str());
          writeWord(":");
          writeSpace();
          i->second.toStream(stream, pretty, level + 1);
          if (++i == map.end()) { writeNewLine(); break; }
          writeWord(",");
          writeNewLine();
        }
        writeTab(level);
      } else writeSpace();
      writeWord("}");
    }

    void write() {
      switch(data.getType()) {
      case Bool: writeWord(data.getBool() ? "true" : "false"); break;
      case Double: writeDouble(data.getDouble()); break;
      case String: writeString(data.getString()); break;
      case List: writeList(data.getList()); break;
      case Map: writeMap(data.getMap()); break;
      case None:
      default: writeWord("null"); break;
      }
      if (!stream) throw TException("write to stream failed");
    }
  };

  Writer(*this, stream, pretty, level).write();
}

//---------------------------------------------------------

void
TVariant::fromStream(std::istream &stream, int *currentRow, int *currentCol) {
  struct Reader {
    TVariant &data;
    std::istream &stream;
    int &row;
    int &col;
    Reader(TVariant &data, std::istream &stream, int &row, int &col):
      data(data), stream(stream), row(row), col(col) { }

    void warning(const std::string &msg)
      { std::cerr << "TVariant load:" << row << ":" << col << ": " << msg << std::endl; }
    void error(const std::string &msg)
      { throw TVariantSyntaxException(row, col, msg); }
    void error()
      { error("cannot recognize type of data"); }

    int peek()
      { return stream.peek(); }

    int get() {
      int c = stream.get();
      if (c == '\n') ++row, col = 1; else ++col;
      if (!stream) error("unexpected end of file");
      return c;
    }

    void skipSpaces()
      { while(isspace(peek())) get(); }

    bool readWord(const char *word) {
      if (peek() != *word) return false;
      while(*word)
        if (get() == *word) ++word; else error();
      return true;
    }

    bool readNull() {
      if (readWord("null") || peek() == EOF)
        { data.reset(); return true; }
      return false;
    }

    bool readBool() {
      if (readWord("true")) { data.setBool(true); return true; }
      if (readWord("false")) { data.setBool(false); return true; }
      return false;
    }

    bool isdouble(int c)
      { return isdigit(c) || c == '-' || c == '+' || c == '.'; }

    bool readDouble() {
      if (!isdouble(peek())) return false;
      std::string str; str.reserve(20);
      while(isdouble(peek()) || isalpha(peek())) str.push_back((char)get());
      double d = 0.0;
      try { d = std::stod(str); }
      catch (const std::exception &e) { warning("wrong number: " + str); }
      data.setDouble(d);
      return true;
    }

    void readHexUnicode(std::string &str) {
      // read utf16 code
      // JSON standard requires exact four hex digits
      // but we're kind and allows 0-4 hex digits
      int code = 0;
      for(int i = 0; i < 4; ++i) {
        char c = peek();
        if (c >= '0' && c <= '9')
          code = 16*code + get() - '0';
        else if (c >= 'a' && c <= 'f')
          code = 16*code + get() - 'a';
        else if (c >= 'A' && c <= 'F')
          code = 16*code + get() - 'A';
        else break;
      }
      if (code == 0)
        { warning("\\u token with zero code"); return; }

      // 16 bits of utf16 character be encoded up to three utf8 bytes
      // in the following format:
      //   11000xxx 10xxxxxx 0xxxxxxx
      if (code >= 1 << 13) {     // 11000xxx
        str.push_back((char)(192 | (code >> 13)));
        code &= (1 << 13) - 1;
      }
      if (code >= 1 << 6) {      // 10xxxxxx
        str.push_back((char)(128 | (code >> 6)));
        code &= (1 << 6) - 1;
      }
      str.push_back((char)code); // 0xxxxxxx
    }

    void readString(std::string &str) {
      if (get() != '\"') error("expected quote");
      while(true) {
        int c = get();
        if (c == '\"') break;
        else
        if (c == '\\') {
          switch(int cc = get()) {
          case '\"': str.push_back('\"'); break;
          case '\\': str.push_back('\\'); break;
          case  '/': str.push_back( '/'); break;
          case  'b': str.push_back('\b'); break;
          case  'f': str.push_back('\f'); break;
          case  'n': str.push_back('\n'); break;
          case  'r': str.push_back('\r'); break;
          case  't': str.push_back('\t'); break;
          case  'u': readHexUnicode(str); break;
          default: str.push_back((char)cc); break;
          }
        } else
          str.push_back((char)c);
      }
    }

    bool readString() {
      if (peek() != '\"') return false;
      std::string str;
      readString(str);
      data.setString(str);
      return true;
    }

    bool readList() {
      if (peek() != '[') return false;
      get(); // skip bracket
      data.reset();
      data.setType(List);
      while(true) {
        skipSpaces();
        if (peek() == ']') { get(); break; }
        if (data.size() && get() != ',') error("expected comma or close bracket");
        skipSpaces();
        if (peek() == ']') { get(); break; } // to allow comma at the end
        data[data.size()].fromStream(stream, &row, &col);
      }
      return true;
    }

    bool readMap() {
      if (peek() != '{') return false;
      get(); // skip brace
      data.reset();
      data.setType(Map);
      while(true) {
        skipSpaces();
        if (peek() == '}') { get(); break; }
        if (data.size() && get() != ',') error("expected comma or close brace");
        skipSpaces();
        if (peek() == '}') { get(); break; } // to allow comma at the end
        std::string key;
        readString(key);
        skipSpaces();
        if (get() != ':') error("expected colon");
        if (data.contains(key)) warning("duplicate key: " + key);
        data[key].fromStream(stream, &row, &col);
      }
      return true;
    }

    void read() {
      skipSpaces();
      if ( !readNull()
        && !readBool()
        && !readDouble()
        && !readString()
        && !readList()
        && !readMap() )
          error();
    }
  };

  reset();
  int row = 1, col = 1;
  Reader(
    *this,
    stream,
    (currentRow ? *currentRow : row),
    (currentCol ? *currentCol : col) ).read();
}

//---------------------------------------------------------

std::string
TVariant::toString(bool pretty, int level) const {
  std::stringstream stream(std::ios_base::out);
  toStream(stream, pretty, level);
  return stream.str();
}

//---------------------------------------------------------

void
TVariant::fromString(const std::string &str, int *currentRow, int *currentCol) {
  std::stringstream stream(str, std::ios_base::in);
  fromStream(stream, currentRow, currentCol);
}

//---------------------------------------------------------

