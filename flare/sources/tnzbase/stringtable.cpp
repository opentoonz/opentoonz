

#include "tw/stringtable.h"
// #include "tw/message.h"
//#include "tfilepath.h"
#include "tfilepath_io.h"
#include "tenv.h"
//#include "texception.h"
#include "tsystem.h"
#include "tstream.h"
#include "tconvert.h"

namespace {

//-------------------------------------------------------------------

class TStringTableImp final : public TStringTable {
public:
  bool m_initialized;

  std::map<std::string, Item> m_table;
  std::pair<std::string, int> m_defaultFontNameAndSize;
  std::string m_defaultMacFontName;

  TStringTableImp();
  ~TStringTableImp();

  void init();

  void load(const TFilePath &);

  void loadCoded(const TFilePath &);
  void saveCoded(const TFilePath &);

  const Item *getItem(std::string name) const override;
  std::pair<std::string, int> getDefaultFontNameAndSize() const override {
    return m_defaultFontNameAndSize;
  }
  std::string getDefaultMacFontName() const override;
};

//-------------------------------------------------------------------

TStringTableImp::TStringTableImp()
    : m_initialized(false)
    , m_defaultFontNameAndSize("", 0)
    , m_defaultMacFontName("") {}

//-------------------------------------------------------------------

TStringTableImp::~TStringTableImp() {}

//-------------------------------------------------------------------

void TStringTableImp::init() {
  if (m_initialized) return;

  m_initialized = true;

  TFilePath plainFp = TEnv::getConfigDir() + "current.txt";

  try {
    load(plainFp);
  } catch (...) {
  }
}

//-------------------------------------------------------------------

std::string TStringTableImp::getDefaultMacFontName() const {
  return m_defaultMacFontName;
}

//-------------------------------------------------------------------

void writeShort(Tofstream &os, int x) {
  os.put(x & 0xff);
  os.put((x >> 8) & 0xff);
}

//-------------------------------------------------------------------

int readShort(Tifstream &is) {
  char hi = 0, lo = 0;
  is.get(lo);
  is.get(hi);
  return (unsigned char)hi << 8 | (unsigned char)lo;
}

//-------------------------------------------------------------------

void writeString(Tofstream &os, std::string s) {
  int len = s.length();
  writeShort(os, len);
  os.write(s.c_str(), len);
  if (len & 0x3) {
    os.write("____", 4 - (len & 0x3));
  }
}

//-------------------------------------------------------------------

std::string readString(Tifstream &is) {
  int len  = readShort(is);
  int len2 = len;
  if (len2 & 0x3) len2 += 4 - (len2 & 0x3);
  char buffer[1204];
  assert(len2 <= (int)(sizeof(buffer)));
  is.read(buffer, len2);
  return std::string(buffer, len);
}

//-------------------------------------------------------------------

void writeStringW(Tofstream &os, std::wstring s) {
  int len = s.length();
  writeShort(os, len);
  os.write(reinterpret_cast<const char *>(s.c_str()), sizeof(wchar_t) * len);
}

//-------------------------------------------------------------------

std::wstring readStringW(Tifstream &is) {
  int len = readShort(is);
  wchar_t buffer[1204];
  assert(len <= (int)(sizeof(buffer)));
  is.read(reinterpret_cast<char *>(buffer), sizeof(wchar_t) * len);
  return std::wstring(buffer, len);
}

//-------------------------------------------------------------------
#ifdef MACOSX
class TMagic  // singleton
{
public:
  std::string m_magic;

private:
  TMagic() : m_magic("stab.001") {}

public:
  static TMagic *instance() {
    static TMagic inst;
    ;
    return &inst;
  }
};

#else
const std::string magic = "stab.001";
#endif

void TStringTableImp::loadCoded(const TFilePath &fp) {
  try {
    Tifstream is(fp);
    char buffer[1024];

#ifdef MACOSX
    is.read(buffer, TMagic::instance()->m_magic.length());
#else
    is.read(buffer, magic.length());
#endif

    m_defaultFontNameAndSize.first  = readString(is);
    m_defaultFontNameAndSize.second = readShort(is);
    int count                       = readShort(is);
    for (int i = 0; i < count; i++) {
      int m = readShort(is);
      assert(1 <= m && m <= 3);
      std::string id = readString(is);
      Item &item     = m_table[id];
      item.m_name    = readStringW(is);
      if (m >= 2) {
        item.m_help            = readStringW(is);
        if (m == 3) item.m_tip = readStringW(is);
      }
    }
    int check = readShort(is);
    assert(check == 12345);
    // if(check != 12345)
    //  throw;
  } catch (...) {
    // TMessage::error("Error reading StringTable file: ", fp);
  }
}

//-------------------------------------------------------------------

/*
void TStringTableImp::saveCoded(const TFilePath &fp)
{
  try {
    Tofstream os(fp);

#ifdef MACOSX
    os.write(TMagic::instance()->m_magic.c_str(),
TMagic::instance()->m_magic.length());
 #else
    os.write(magic.c_str(), magic.length());
 #endif
    writeString(os, m_defaultFontNameAndSize.first);
    writeShort(os, m_defaultFontNameAndSize.second);
    writeShort(os, m_table.size());
    for(std::map<std::string, Item>::iterator it = m_table.begin();
        it != m_table.end(); ++it)
      {
       Item &item = it->second;
       int m = 1;
       if(item.m_tip != L"") m = 3;
       else if(item.m_help != L"") m = 2;
       writeShort(os, m);
       writeString(os, it->first);
       writeStringW(os, item.m_name);
       if(m>=2)
         {
          writeStringW(os, item.m_help);
          if(m==3)
            writeStringW(os, item.m_tip);
         }
      }
    writeShort(os, 12345);
  } catch(...) {
    TMessage::error("Unable to save StringTable file: ", fp);
  }
}
*/
//-------------------------------------------------------------------

void TStringTableImp::load(const TFilePath &fp) {
  if (!TFileStatus(fp).doesExist()) throw TException("file not found");

  TIStream is(fp);
  if (!is) throw TException("can't read string table ");

  std::string tagName;
  if (!is.matchTag(tagName) || tagName != "stringtable")
    throw TException("not a string table file");

  while (!is.matchEndTag()) {
    if (!is.matchTag(tagName)) throw TException("expected tag");
    if (tagName == "item") {
      std::string id, name, help, tip;
      is >> id >> name;
      if (!is.matchEndTag()) {
        is >> help;
        if (!is.matchEndTag()) {
          is >> tip;
          if (!is.matchEndTag()) throw TException("Expected end tag");
        }
      }
      Item &item  = m_table[id];
      item.m_name = ::to_wstring(name);
      item.m_help = ::to_wstring(help);
      item.m_tip  = ::to_wstring(tip);
    } else if (tagName == "defaultFont") {
      std::string fontName;
      int fontSize = 0;
      is >> fontName >> fontSize;
      if (!is.matchEndTag()) throw TException("Expected end tag");
      m_defaultFontNameAndSize = std::make_pair(fontName, fontSize);
    } else if (tagName == "defaultMacFont") {
      std::string macFontName;
      is >> macFontName;
      if (!is.matchEndTag()) throw TException("Expected end tag");
      m_defaultMacFontName = macFontName;
    } else
      throw TException("unexpected tag /" + tagName + "/");
  }
  // m_valid =true;
}

//-------------------------------------------------------------------

const TStringTable::Item *TStringTableImp::getItem(std::string name) const {
  std::map<std::string, Item>::const_iterator it;
  it = m_table.find(name);
  if (it == m_table.end())
    return 0;
  else
    return &(it->second);
}

//-------------------------------------------------------------------

}  // namespace

//-------------------------------------------------------------------

TStringTable::TStringTable() {}

//-------------------------------------------------------------------

TStringTable::~TStringTable() {}

//-------------------------------------------------------------------

std::wstring TStringTable::translate(std::string name) {
  const TStringTable::Item *item = instance()->getItem(name);
  if (item)
    return item->m_name;
  else
    return ::to_wstring(name);
}

//-------------------------------------------------------------------

const TStringTable *TStringTable::instance() {
  // may hurt MacOsX
  static TStringTableImp *instance = 0;
  if (!instance) instance          = new TStringTableImp;

  instance->init();

  return instance;
}
