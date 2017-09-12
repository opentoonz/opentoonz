

#include "tstream.h"
#include "tpersist.h"
#include "tfilepath_io.h"
#include "tconvert.h"
#include "tsystem.h"
#include "tutil.h"

#if defined(LZ4_STATIC)
#include "lz4frame_static.h"
#else
#include "lz4frame.h"
#endif

#include <sstream>
#include <memory>

using namespace std;

//===============================================================
namespace {

string escape(string v) {
  int i = 0;
  for (;;) {
// Removing escaping of apostrophe from Windows and OSX as it's not needed and causes problems
#ifdef LINUX
    i = v.find_first_of("\\\'\"", i);
#else
    i = v.find_first_of("\\\"", i);
#endif
    if (i == (int)string::npos) break;
    string h = "\\" + v[i];
    v.insert(i, "\\");
    i = i + 2;
  }
  return v;
}

//===============================================================

void writeCompressedFile(TFilePath dst, const std::string &str) {}

//===================================================================

void readCompressedFile(string &str, TFilePath src) {
  TFileStatus status(src);
  if (!status.doesExist()) return;

  size_t in_len = status.getSize();
  char *in      = (char *)malloc(in_len);
  {
    Tifstream is(src);
    is.read((char *)in, in_len);
  }

  LZ4F_decompressionContext_t lz4dctx;

  LZ4F_errorCode_t err =
      LZ4F_createDecompressionContext(&lz4dctx, LZ4F_VERSION);
  if (LZ4F_isError(err)) return;

  size_t in_len_read = 0;

  size_t out_len = 1000000, out_len_written, out_len_moved = 0;
  void *out = (void *)malloc(out_len);

  while (in_len_read < in_len) {
    out_len_written = out_len;

    size_t remaining =
        LZ4F_decompress(lz4dctx, out, &out_len_written, in, &in_len, NULL);
    if (LZ4F_isError(remaining)) break;

    str.resize(out_len_moved + out_len_written);

    memcpy((void *)(str.c_str() + out_len_moved), (void *)out, out_len_written);
    out_len_moved += out_len_written;
  }

  LZ4F_freeDecompressionContext(lz4dctx);

  free(in);
  free(out);
}

namespace {
// TODO: Unify with tcodec.cpp's version
bool lz4decompress(LZ4F_decompressionContext_t lz4dctx, char *out,
                   size_t *out_len_res, const char *in, size_t in_len) {
  size_t out_len = *out_len_res, in_read, out_written;

  *out_len_res = 0;

  while (in_len) {
    out_written = out_len;
    in_read     = in_len;

    size_t res = LZ4F_decompress(lz4dctx, (void *)out, &out_written,
                                 (const void *)in, &in_read, NULL);

    if (LZ4F_isError(res)) return false;

    *out_len_res += out_written;

    out += out_written;
    out_len -= out_written;

    in += in_read;
    in_len -= in_read;
  }

  return true;
}
}  // namespace

//===============================================================

class StreamTag {
public:
  string m_name;
  std::map<std::string, string> m_attributes;
  enum Type { BeginTag, EndTag, BeginEndTag };
  Type m_type;
  StreamTag() : m_type(BeginTag) {}

  operator bool() const { return m_name != ""; }

  void dump() {
    cout << "name = '" << m_name << "'" << endl;
    cout << "type = ";
    switch (m_type) {
    case BeginTag:
      cout << "begin Tag";
      break;
    case EndTag:
      cout << "end Tag";
      break;
    case BeginEndTag:
      cout << "begin/end Tag";
      break;
    default:
      cout << "**bad Tag**";
      break;
    }
    cout << endl;
    std::map<std::string, string>::iterator it;
    for (it = m_attributes.begin(); it != m_attributes.end(); ++it) {
      cout << " '" << it->first << "' = '" << it->second << "'" << endl;
    }
  }
};

//--------------------------------

class TPersistFactory {
  typedef std::map<std::string, TPersistDeclaration *> Table;
  static TPersistFactory *m_factory;
  Table m_table;
  TPersistFactory() {}

public:
  static TPersistFactory *instance() {
    if (!m_factory) m_factory = new TPersistFactory;
    return m_factory;
  }
  void add(string name, TPersistDeclaration *decl) { m_table[name] = decl; }
  TPersist *create(string name) {
    Table::iterator it = m_table.find(name);
    if (it != m_table.end())
      return (it->second)->create();
    else
      return 0;
  }
};

//--------------------------------

TPersistFactory *TPersistFactory::m_factory = 0;

}  // namespace

//--------------------------------

TPersistDeclaration::TPersistDeclaration(const std::string &id) : m_id(id) {
  TPersistFactory::instance()->add(id, this);
}

//===============================================================

TPersist *TPersist::create(const std::string &name) {
  return TPersistFactory::instance()->create(name);
}

//===============================================================

class TOStream::Imp {
public:
  ostream *m_os;
  bool m_chanOwner;
  bool m_compressed;
  ostringstream m_ostringstream;

  vector<std::string> m_tagStack;
  int m_tab;
  bool m_justStarted;
  typedef map<TPersist *, int> PersistTable;
  PersistTable m_table;
  int m_maxId;
  TFilePath m_filepath;

  Imp()
      : m_os(0)
      , m_chanOwner(false)
      , m_tab(0)
      , m_justStarted(true)
      , m_maxId(0)
      , m_compressed(false) {}
};

//---------------------------------------------------------------

TOStream::TOStream(const TFilePath &fp, bool compressed) : m_imp(new Imp) {
  m_imp->m_filepath = fp;

  if (compressed) {
    m_imp->m_os         = &m_imp->m_ostringstream;
    m_imp->m_compressed = true;
    m_imp->m_chanOwner  = false;
  } else {
    std::unique_ptr<Tofstream> os(new Tofstream(fp));
    m_imp->m_os        = os->isOpen() ? os.release() : 0;
    m_imp->m_chanOwner = true;
  }

  m_imp->m_justStarted = true;
}

//---------------------------------------------------------------

TOStream::TOStream(std::shared_ptr<Imp> imp) : m_imp(std::move(imp)) {
  assert(!m_imp->m_tagStack.empty());
  ostream &os = *m_imp->m_os;
  if (!m_imp->m_justStarted) cr();
  os << "<" << m_imp->m_tagStack.back() << ">";
  m_imp->m_tab++;
  cr();
  m_imp->m_justStarted = true;
}

TOStream::TOStream(TOStream &&that) : m_imp(std::move(that.m_imp)) {}

TOStream &TOStream::operator=(TOStream &&that) {
  if (this != &that) {
    this->m_imp = std::move(that.m_imp);
  }
  return *this;
}

//---------------------------------------------------------------

TOStream::~TOStream() {
  if (!m_imp) {
    return;
  }
  try {
    if (!m_imp->m_tagStack.empty()) {
      string tagName = m_imp->m_tagStack.back();
      m_imp->m_tagStack.pop_back();
      assert(tagName != "");
      ostream &os = *m_imp->m_os;
      m_imp->m_tab--;
      if (!m_imp->m_justStarted) cr();
      os << "</" << tagName << ">";
      cr();
      m_imp->m_justStarted = true;
    } else {
      if (m_imp->m_compressed) {
        std::string tmp = m_imp->m_ostringstream.str();
        const void *in  = (const void *)tmp.c_str();

        size_t in_len = strlen((char *)in);

        size_t out_len = LZ4F_compressFrameBound(in_len, NULL);
        void *out      = malloc(out_len);

        out_len = LZ4F_compressFrame(out, out_len, in, in_len, NULL);
        if (!LZ4F_isError(out_len)) {
          Tofstream os(m_imp->m_filepath);
          // TNZC <lunghezza dati decompress> <lunghezza dati compresso> <dati
          // compressi>
          os.write("TABc", 4);
          TINT32 v;
          v = 0x0A0B0C0D;
          os.write((char *)&v, sizeof v);
          v = in_len;
          os.write((char *)&v, sizeof v);
          v = out_len;
          os.write((char *)&v, sizeof v);
          os.write((char *)out, out_len);
        }

        free(out);
      }
      if (m_imp->m_chanOwner) delete m_imp->m_os;
    }
  } catch (...) {
  }
}

//---------------------------------------------------------------

TFilePath TOStream::getFilePath() { return m_imp->m_filepath; }

//---------------------------------------------------------------

TFilePath TOStream::getRepositoryPath() {
  TFilePath fp = m_imp->m_filepath;
  return fp.getParentDir() + (fp.getName() + "_files");
}

//---------------------------------------------------------------

TOStream &TOStream::operator<<(int v) {
  *(m_imp->m_os) << v << " ";
  m_imp->m_justStarted = false;
  return *this;
}

//---------------------------------------------------------------

TOStream &TOStream::operator<<(double v) {
  if (areAlmostEqual(v, 0))  // con valori molto piccoli (es. 1.4e-310) non
                             // riesce a rileggerli!
    v = 0;

  *(m_imp->m_os) << v << " ";
  m_imp->m_justStarted = false;
  return *this;
}

//---------------------------------------------------------------

TOStream &TOStream::operator<<(string v) {
  ostream &os = *(m_imp->m_os);
  int len     = v.length();
  if (len == 0) {
    os << "\"\""
       << " ";
    m_imp->m_justStarted = false;
    return *this;
  }
  int i;
  for (i = 0; i < len; i++)
    if ((!iswalnum(v[i]) && v[i] != '_' && v[i] != '%')
		|| v[i] < 32   // Less than ASCII for SPACE
		|| v[i] > 126  // Greater than ASCII for ~
		) break;
  if (i == len)
    os << v << " ";
  else {
    os << '"' << escape(v) << '"';
  }
  m_imp->m_justStarted = false;
  return *this;
}

//---------------------------------------------------------------

TOStream &TOStream::operator<<(QString _v) {
  string v = _v.toStdString();

  ostream &os = *(m_imp->m_os);
  int len     = v.length();
  if (len == 0) {
    os << "\"\""
       << " ";
    m_imp->m_justStarted = false;
    return *this;
  }
  int i;
  for (i = 0; i < len; i++)
	  if ((!iswalnum(v[i]) && v[i] != '_' && v[i] != '%')
		  || v[i] < 32   // Less than ASCII for SPACE
		  || v[i] > 126  // Greater than ASCII for ~
		  ) break;
  if (i == len)
    os << v << " ";
  else {
    os << '"' << escape(v) << '"';
  }
  m_imp->m_justStarted = false;
  return *this;
}

//---------------------------------------------------------------

TOStream &TOStream::operator<<(std::wstring v) {
  return operator<<(::to_string(v));
  /*
ostream &os = *(m_imp->m_os);
int len = v.length();
if(len==0)
{
os << "\"" << "\"" << " ";
m_imp->m_justStarted = false;
return *this;
}
int i;
for(i=0;i<len;i++)
if(!iswalnum(v[i]) && v[i]!=L'_')
break;
if(i==len)
{
os << v;
os << " ";
}
else
{
os.put('"');
for(i=0;i<len;i++)
 if(iswalnum(v[i]))
   os.put((char)v[i]);
 else if(v[i]=='"')
   os << "\\\"";
 else if(v[i]=='\n')
   os << "\\n";
 else if(iswprint(v[i]))
   os << v[i];
 else
   {os.put('\\'); os << (int)v[i];}
os << "\" ";
}
m_imp->m_justStarted = false;
return *this;
*/
}

//---------------------------------------------------------------

TOStream &TOStream::operator<<(const TFilePath &v) {
  return operator<<(v.getWideString());
}

//---------------------------------------------------------------

TOStream &TOStream::operator<<(const TPixel32 &v) {
  ostream &os = *(m_imp->m_os);
  os << (int)v.r << " " << (int)v.g << " " << (int)v.b << " " << (int)v.m
     << " ";
  m_imp->m_justStarted = false;
  return *this;
}

//---------------------------------------------------------------

TOStream &TOStream::operator<<(const TPixel64 &v) {
  ostream &os = *(m_imp->m_os);
  os << (int)v.r << " " << (int)v.g << " " << (int)v.b << " " << (int)v.m
     << " ";
  m_imp->m_justStarted = false;
  return *this;
}

//---------------------------------------------------------------

void TOStream::cr() {
  *(m_imp->m_os) << endl;
  for (int i           = 0; i < m_imp->m_tab; i++) *(m_imp->m_os) << "  ";
  m_imp->m_justStarted = false;
}

//---------------------------------------------------------------

TOStream::operator bool() const { return (m_imp->m_os && *m_imp->m_os); }

//---------------------------------------------------------------
TOStream TOStream::child(string tagName) {
  assert(tagName != "");
  m_imp->m_tagStack.push_back(tagName);
  return TOStream(m_imp);
}

//---------------------------------------------------------------

void TOStream::openChild(string tagName) {
  assert(tagName != "");
  m_imp->m_tagStack.push_back(tagName);
  if (m_imp->m_justStarted == false) cr();
  *(m_imp->m_os) << "<" << m_imp->m_tagStack.back() << ">";
  m_imp->m_tab++;
  cr();
  m_imp->m_justStarted = true;
}

//---------------------------------------------------------------

void TOStream::openChild(string tagName,
                         const map<std::string, string> &attributes) {
  assert(tagName != "");
  m_imp->m_tagStack.push_back(tagName);
  if (m_imp->m_justStarted == false) cr();
  *(m_imp->m_os) << "<" << m_imp->m_tagStack.back();
  for (std::map<std::string, string>::const_iterator it = attributes.begin();
       it != attributes.end(); ++it) {
    *(m_imp->m_os) << " " << it->first << "=\"" << escape(it->second) << "\"";
  }
  *(m_imp->m_os) << ">";
  m_imp->m_tab++;
  cr();
  m_imp->m_justStarted = true;
}

//---------------------------------------------------------------

void TOStream::closeChild() {
  assert(!m_imp->m_tagStack.empty());
  string tagName = m_imp->m_tagStack.back();
  m_imp->m_tagStack.pop_back();
  assert(tagName != "");
  // ostream &os = *m_imp->m_os; //os non e' usato
  m_imp->m_tab--;
  if (!m_imp->m_justStarted) cr();
  *(m_imp->m_os) << "</" << tagName << ">";
  cr();
  m_imp->m_justStarted = true;
}

//---------------------------------------------------------------

void TOStream::openCloseChild(string tagName,
                              const map<std::string, string> &attributes) {
  assert(tagName != "");
  // m_imp->m_tagStack.push_back(tagName);
  if (m_imp->m_justStarted == false) cr();
  *(m_imp->m_os) << "<" << tagName;
  for (std::map<std::string, string>::const_iterator it = attributes.begin();
       it != attributes.end(); ++it) {
    *(m_imp->m_os) << " " << it->first << "=\"" << escape(it->second) << "\"";
  }
  *(m_imp->m_os) << "/>";
  // m_imp->m_tab++;
  cr();
  m_imp->m_justStarted = true;
}

//---------------------------------------------------------------

TOStream &TOStream::operator<<(TPersist &v) {
  v.saveData(*this);
  return *this;
}

//---------------------------------------------------------------

TOStream &TOStream::operator<<(TPersist *v) {
  Imp::PersistTable::iterator it = m_imp->m_table.find(v);
  if (it != m_imp->m_table.end()) {
    *(m_imp->m_os) << "<" << v->getStreamTag() << " id='" << it->second
                   << "'/>";
    m_imp->m_justStarted = false;
  } else {
    int id            = ++m_imp->m_maxId;
    m_imp->m_table[v] = id;
    *(m_imp->m_os) << "<" << v->getStreamTag() << " id='" << id << "'>";
    m_imp->m_tab++;
    cr();
    v->saveData(*this);
    m_imp->m_tab--;
    cr();
    *(m_imp->m_os) << "</" << v->getStreamTag() << ">";
    cr();
  }
  return *this;
}

//---------------------------------------------------------------

bool TOStream::checkStatus() const {
  if (!m_imp->m_os) return false;

  m_imp->m_os->flush();
  return m_imp->m_os->rdstate() == ios_base::goodbit;
}

//===============================================================
/*!
        This class contains TIStream's attributes.
        It is created by memory allocation in the TIStream's constructor.
*/
class TIStream::Imp {
public:
  istream *m_is;
  bool m_chanOwner;
  int m_line;
  string m_strbuffer;
  bool m_compressed;

  vector<std::string> m_tagStack;

  typedef map<int, TPersist *> PersistTable;
  PersistTable m_table;

  StreamTag m_currentTag;

  TFilePath m_filepath;

  VersionNumber m_versionNumber;

  Imp()
      : m_is(0)
      , m_chanOwner(false)
      , m_line(0)
      , m_compressed(false)
      , m_versionNumber(0, 0) {}

  // update m_line if necessary; returns -e if eof
  int getNextChar();

  inline void skipBlanks();
  bool matchTag();
  inline bool match(char c);
  bool matchIdent(string &ident);
  bool matchValue(string &value);

  void skipCurrentTag();
};

//---------------------------------------------------------------

TFilePath TIStream::getFilePath() { return m_imp->m_filepath; }

//---------------------------------------------------------------

TFilePath TIStream::getRepositoryPath() {
  TFilePath fp = m_imp->m_filepath;
  return fp.getParentDir() + (fp.getName() + "_files");
}

//---------------------------------------------------------------

int TIStream::Imp::getNextChar() {
  char c;
  m_is->get(c);
  if (m_is->eof()) return -1;
  if (c == '\r') m_line++;
  return c;
}

//---------------------------------------------------------------

void TIStream::Imp::skipBlanks() {
  istream &is = *m_is;
  istream::int_type c;
  while (c = is.peek(), (isspace(c) || c == '\r')) getNextChar();
}

//---------------------------------------------------------------

bool TIStream::Imp::match(char c) {
  if (m_is->peek() == c) {
    getNextChar();
    return true;
  } else
    return false;
}

//---------------------------------------------------------------

bool TIStream::Imp::matchIdent(string &ident) {
  istream &is = *m_is;
  if (!isalnum(is.peek())) return false;
  ident = "";
  char c;
  is.get(c);
  ident.append(1, c);
  while (c = is.peek(), isalnum(c) || c == '_' || c == '.' || c == '-') {
    is.get(c);
    ident.append(1, c);
  }
  return true;
}

//---------------------------------------------------------------

bool TIStream::Imp::matchValue(string &str) {
  istream &is = *m_is;
  char quote  = is.peek();
  char c;
  if (!is || (quote != '\'' && quote != '\"')) return false;
  is.get(c);
  str = "";
  for (;;) {
    is.get(c);
    if (!is) throw TException("expected '\"'");
    if (c == quote) break;
    if (c == '\\') {
      is.get(c);
      if (!is) throw TException("unexpected EOF");
      if (c != '\'' && c != '\"' && c != '\\')
        throw TException("bad escape sequence");
    }
    str.append(1, c);
  }
  if (c != quote) throw TException("missing '\"'");
  return true;
}

//---------------------------------------------------------------

bool TIStream::Imp::matchTag() {
  if (m_currentTag) return true;
  StreamTag &tag = m_currentTag;
  tag            = StreamTag();
  skipBlanks();
  if (!match('<')) return false;
  skipBlanks();
  if (match('!')) {
    skipBlanks();
    if (!match('-') || !match('-')) throw TException("expected '<!--' tag");
    istream &is = *m_is;
    char c;
    int status = 1;
    while (status != 0 && is.get(c)) switch (status) {
      case 1:
        if (c == '-') status = 2;
        break;
      case 2:
        if (c == '-')
          status = 3;
        else
          status = 1;
        break;
      case 3:
        if (c == '>')
          status = 0;
        else if (c == '-') {
        } else
          status = 1;
        break;
      }
    return matchTag();
  }
  if (match('/')) {
    tag.m_type = StreamTag::EndTag;
    skipBlanks();
  }

  if (!matchIdent(tag.m_name)) throw TException("expected identifier");
  skipBlanks();
  for (;;) {
    if (match('>')) break;
    if (match('/')) {
      tag.m_type = StreamTag::BeginEndTag;
      skipBlanks();
      if (match('>')) break;
      throw TException("expected '>'");
    }
    string name;
    if (!matchIdent(name)) throw TException("expected identifier");
    skipBlanks();
    if (match('=')) {
      string value;
      skipBlanks();
      if (!matchValue(value)) throw TException("expected value");
      tag.m_attributes[name] = value;
      skipBlanks();
    }
  }
  return true;
}

//---------------------------------------------------------------

void TIStream::Imp::skipCurrentTag() {
  if (m_currentTag.m_type == StreamTag::BeginEndTag) return;
  istream &is = *m_is;
  int level   = 1;
  int c;
  for (;;) {
    if (is.eof()) break;  // unexpected eof
    c = is.peek();
    if (c != '<') {
      getNextChar();
      continue;
    }

    // tag found
    c = getNextChar();
    if (c < 0) break;

    c = getNextChar();
    if (c < 0) break;

    if (c == '/') {
      // end tag
      do
        c = getNextChar();
      while (c >= 0 && c != '>');
      if (c < 0) break;  // unexpected eof
      if (--level <= 0) {
        // m_currentTag.m_type = StreamTag::EndTag;
        m_tagStack.pop_back();
        m_currentTag = StreamTag();
        break;
      }
    } else {
      // tag
      int oldC;
      do {
        oldC = c;
        c    = getNextChar();
      } while (c >= 0 && c != '>');
      if (c < 0) break;  // unexpected eof
      if (oldC != '/') level++;
    }
  }
}

//---------------------------------------------------------------

TIStream::TIStream(const TFilePath &fp) : m_imp(new Imp) {
  m_imp->m_filepath = fp;
  m_imp->m_is       = new Tifstream(fp);

  if (m_imp->m_is->peek() == 'T')  // non comincia con '<' dev'essere compresso
  {
    bool swapForEndianess = false;

    unique_ptr<std::istream> is(m_imp->m_is);
    m_imp->m_is = 0;

    char magicBuffer[4];
    is->read(magicBuffer, 4);
    string magic(magicBuffer, 4);
    size_t in_len, out_len;

    if (magic == "TNZC") {
      // Tab3.0 beta
      is->read((char *)&out_len, sizeof out_len);
      is->read((char *)&in_len, sizeof in_len);
    } else if (magic == "TABc") {
      TINT32 v;
      is->read((char *)&v, sizeof v);
      printf("magic = %08X\n", v);

      if (v == 0x0A0B0C0D)
        swapForEndianess = false;
      else if (v == 0x0D0C0B0A)
        swapForEndianess = true;
      else {
        swapForEndianess = true;
        printf("UH OH!\n");
      }

      is->read((char *)&v, sizeof v);
      out_len = swapForEndianess ? swapTINT32(v) : v;
      is->read((char *)&v, sizeof v);
      in_len = swapForEndianess ? swapTINT32(v) : v;
    } else
      throw TException("Bad magic number");

    if (in_len <= 0 || in_len > 100000000)  // 100M di tnzfile (compresso)
                                            // sembrano proprio esagerati
      throw TException("Corrupted file");

    LZ4F_decompressionContext_t lz4dctx;

    LZ4F_errorCode_t err =
        LZ4F_createDecompressionContext(&lz4dctx, LZ4F_VERSION);
    if (LZ4F_isError(err)) throw TException("Couldn't decompress file");

    char *in = (char *)malloc(in_len);
    is->read((char *)in, in_len);

    m_imp->m_strbuffer.resize(out_len + 1000);  // per prudenza
    char *out = (char *)m_imp->m_strbuffer.c_str();

    size_t check_len = out_len;

    // size_t remaining = LZ4F_decompress(lz4dctx, out, &out_len, in, &in_len,
    // NULL);
    bool ok = lz4decompress(lz4dctx, out, &out_len, in, in_len);
    LZ4F_freeDecompressionContext(lz4dctx);

    free(in);

    if (!ok) throw TException("Couldn't decompress file");

    if (check_len != out_len) throw TException("corrupted file");

    m_imp->m_is = new istringstream(std::string(out, out_len));
  }

  m_imp->m_chanOwner = true;
}

//---------------------------------------------------------------

/*
TIStream::TIStream(istream &is)
         : m_imp(new Imp)
{
  m_imp->m_is = &is;
}
*/

//---------------------------------------------------------------

TIStream::~TIStream() {
  if (m_imp->m_chanOwner) delete m_imp->m_is;
}

//---------------------------------------------------------------

TIStream &TIStream::operator>>(int &v) {
  *(m_imp->m_is) >> v;
  return *this;
}

//---------------------------------------------------------------

TIStream &TIStream::operator>>(double &v) {
  *(m_imp->m_is) >> v;
  return *this;
}
//---------------------------------------------------------------

TIStream &TIStream::operator>>(std::wstring &v) {
  string s;
  operator>>(s);
  v = ::to_wstring(s);
  return *this;
}

//---------------------------------------------------------------

TIStream &TIStream::operator>>(string &v) {
  istream &is = *(m_imp->m_is);
  v           = "";
  m_imp->skipBlanks();
  char c;
  is.get(c);
  if (c == '\"') {
    is.get(c);
    while (is && c != '"') {
      if (c == '\\') {
        is.get(c);
        if (!is) throw TException("unexpected EOF");
        if (c == '"')
          v.append(1, '"');
        else if (c == '\\')
          v.append(1, '\\');
        else if (c == '\'')
          v.append(1, '\'');
        else {
          v.append(1, '\\');
          v.append(1, c);
        }
      } else
        v.append(1, c);
      is.get(c);
    }
  } else {
    v.append(1, c);
    while (c = is.peek(), isalnum(c) || c == '_' || c == '&' || c == '#' ||
                              c == ';' || c == '%') {
      is.get(c);
      v.append(1, c);
    }
  }

  return *this;
}

//---------------------------------------------------------------

TIStream &TIStream::operator>>(QString &v) {
  istream &is = *(m_imp->m_is);
  v           = "";
  m_imp->skipBlanks();
  char c;
  is.get(c);
  if (c == '\"') {
    is.get(c);
    while (is && c != '"') {
      if (c == '\\') {
        is.get(c);
        if (!is) throw TException("unexpected EOF");
        if (c == '"')
          v.append('"');
        else if (c == '\\')
          v.append('\\');
        else if (c == '\'')
          v.append('\'');
        else {
          v.append('\\');
          v.append(c);
        }
      } else
        v.append(c);
      is.get(c);
    }
  } else {
    v.append(c);
    while (c = is.peek(), isalnum(c) || c == '_' || c == '&' || c == '#' ||
                              c == ';' || c == '%') {
      is.get(c);
      v.append(c);
    }
  }

  return *this;
}

//---------------------------------------------------------------

string TIStream::getString() {
  istream &is = *(m_imp->m_is);
  string v    = "";
  m_imp->skipBlanks();
  char c = is.peek();
  while (c != '<') {
    is.get(c);
    c = is.peek();
    if (!is) throw TException("unexpected EOF");
    v.append(1, c);
  }
  return v;
}

//---------------------------------------------------------------

TIStream &TIStream::operator>>(TPixel32 &v) {
  istream &is = *(m_imp->m_is);
  int r, g, b, m;
  is >> r;
  is >> g;
  is >> b;
  is >> m;
  v.r = r;
  v.g = g;
  v.b = b;
  v.m = m;
  return *this;
}
//---------------------------------------------------------------

TIStream &TIStream::operator>>(TPixel64 &v) {
  istream &is = *(m_imp->m_is);
  int r, g, b, m;
  is >> r;
  is >> g;
  is >> b;
  is >> m;
  v.r = r;
  v.g = g;
  v.b = b;
  v.m = m;
  return *this;
}

//---------------------------------------------------------------

TIStream &TIStream::operator>>(TFilePath &v) {
  istream &is = *(m_imp->m_is);
  string s;
  char c;
  m_imp->skipBlanks();
  is.get(c);
  if (c == '"') {
    is.get(c);
    while (is && c != '"') {
      // if(c=='\\')
      //   is.get(c);
      s.append(1, c);
      is.get(c);
    }
  } else {
    // il filepath non e' fra virgolette:
    // puo' contenere solo caratteri alfanumerici, % e _
    s.append(1, c);
    while (is) {
      c = is.peek();
      if (!isalnum(c) && c != '%' && c != '_') break;
      is.get(c);
      s.append(1, c);
    }
  }

  v = TFilePath(s);
  return *this;
}
//---------------------------------------------------------------

TIStream &TIStream::operator>>(TPersist &v) {
  v.loadData(*this);
  return *this;
}

//---------------------------------------------------------------

TIStream &TIStream::operator>>(TPersist *&v) {
  if (!m_imp->matchTag() || m_imp->m_currentTag.m_type == StreamTag::EndTag) {
    throw TException("expected begin tag");
  }
  StreamTag tag       = m_imp->m_currentTag;
  m_imp->m_currentTag = StreamTag();
  string tagName      = tag.m_name;
  std::map<std::string, string>::iterator it;
  int id                               = -1;
  it                                   = tag.m_attributes.find("id");
  if (it != tag.m_attributes.end()) id = atoi(it->second.c_str());
  // cout << "tagname = " << tagName << " id = " << id << endl;

  Imp::PersistTable::iterator pit = m_imp->m_table.find(id);
  if (pit == m_imp->m_table.end()) {
    v = TPersistFactory::instance()->create(tagName);
    if (!v) throw TException("unable to create a persistent '" + tagName + "'");
    m_imp->m_table[id] = v;
    if (tag.m_type != StreamTag::BeginTag)
      throw TException("expected begin tag");
    m_imp->m_tagStack.push_back(tag.m_name);
    v->loadData(*this);
    m_imp->matchTag();
    if (!m_imp->m_currentTag || m_imp->m_currentTag.m_type != StreamTag::EndTag)
      throw TException("expected end tag");
    if (m_imp->m_currentTag.m_name != m_imp->m_tagStack.back())
      throw TException("end tag mismatch");
    m_imp->m_tagStack.pop_back();
    m_imp->m_currentTag = StreamTag();
  } else {
    v = pit->second;
    if (tag.m_type != StreamTag::BeginEndTag)
      throw TException("expected begin/end tag");
  }
  return *this;
}

//---------------------------------------------------------------

bool TIStream::matchEndTag() {
  if (m_imp->m_tagStack.empty()) throw TException("tag stack emtpy");
  if (!m_imp->matchTag()) return false;
  if (m_imp->m_currentTag.m_type != StreamTag::EndTag) return false;
  if (m_imp->m_currentTag.m_name != m_imp->m_tagStack.back())
    throw TException("end tag mismatch");
  m_imp->m_tagStack.pop_back();
  m_imp->m_currentTag = StreamTag();
  return true;
}

//---------------------------------------------------------------

bool TIStream::eos() {
  if (m_imp->matchTag())
    return m_imp->m_currentTag.m_type == StreamTag::EndTag;
  else
    return !(*m_imp->m_is);
}

//---------------------------------------------------------------

bool TIStream::matchTag(string &tagName) {
  if (!m_imp->matchTag()) return false;
  if (m_imp->m_currentTag.m_type == StreamTag::EndTag) return false;
  tagName                    = m_imp->m_currentTag.m_name;
  m_imp->m_currentTag.m_name = "";
  if (m_imp->m_currentTag.m_type != StreamTag::BeginEndTag)
    m_imp->m_tagStack.push_back(tagName);
  return true;
}

//---------------------------------------------------------------

string TIStream::getTagAttribute(string name) const {
  StreamTag &tag = m_imp->m_currentTag;
  std::map<std::string, string>::const_iterator it =
      tag.m_attributes.find(name);
  if (it == tag.m_attributes.end())
    return "";
  else
    return it->second;
}

//---------------------------------------------------------------

bool TIStream::getTagParam(string paramName, string &value) {
  if (m_imp->m_tagStack.empty()) return false;
  StreamTag &tag = m_imp->m_currentTag;
  std::map<std::string, string>::const_iterator it =
      tag.m_attributes.find(paramName);
  if (it == tag.m_attributes.end()) return false;
  value = it->second;
  return true;
}

//---------------------------------------------------------------

bool TIStream::getTagParam(string paramName, int &value) {
  string svalue;
  if (!getTagParam(paramName, svalue)) return false;
  istringstream is(svalue);
  value = 0;
  is >> value;
  return true;
}

//---------------------------------------------------------------

bool TIStream::isBeginEndTag() {
  return m_imp->m_currentTag.m_type == StreamTag::BeginEndTag;
}

//---------------------------------------------------------------

bool TIStream::openChild(string &tagName) {
  if (!m_imp->matchTag()) return false;
  if (m_imp->m_currentTag.m_type != StreamTag::BeginTag) return false;
  tagName                    = m_imp->m_currentTag.m_name;
  m_imp->m_currentTag.m_name = "";
  m_imp->m_tagStack.push_back(tagName);
  return true;
}

//---------------------------------------------------------------

void TIStream::closeChild() {
  if (!matchEndTag()) {
    string tagName;
    if (!m_imp->m_tagStack.empty()) tagName = m_imp->m_tagStack.back();
    if (tagName != "")
      throw TException("Expected \"" + tagName + "\" end tag");
    else
      throw TException("expected EndTag");
  }
}

//---------------------------------------------------------------

bool TIStream::match(char c) const {
  m_imp->skipBlanks();
  if (m_imp->m_is->peek() != c) return false;
  m_imp->m_is->get(c);
  if (c == '\r') m_imp->m_line++;
  return true;
}

//---------------------------------------------------------------

TIStream::operator bool() const { return (m_imp->m_is && *m_imp->m_is); }

//---------------------------------------------------------------

int TIStream::getLine() const { return m_imp->m_line + 1; }

//---------------------------------------------------------------

VersionNumber TIStream::getVersion() const { return m_imp->m_versionNumber; }

//---------------------------------------------------------------

void TIStream::setVersion(const VersionNumber &version) {
  m_imp->m_versionNumber = version;
}

//---------------------------------------------------------------

void TIStream::skipCurrentTag() { m_imp->skipCurrentTag(); }
