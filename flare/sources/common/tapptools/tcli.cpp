

#include "tcli.h"
#include "tconvert.h"
#include <ctype.h>

#include "tversion.h"
using namespace TVER;

using namespace std;
using namespace TCli;

//=========================================================
//
// Release
//
//---------------------------------------------------------
namespace {

//---------------------------------------------------------

void printToonzRelease(ostream &out) {
  TVER::ToonzVersion tver;
  out << tver.getAppVersionInfo("") << endl;
}

//---------------------------------------------------------

void printLibRelease(ostream &out) {
  TVER::ToonzVersion tver;
  out << tver.getAppVersionInfo("") << " - " __DATE__ << endl;
}
//---------------------------------------------------------

}  // namespace

//=========================================================
//
// local UsageError
//
//---------------------------------------------------------

//=========================================================
//
// local UsageElements
//
//---------------------------------------------------------

class SpecialUsageElement final : public UsageElement {
public:
  SpecialUsageElement(std::string name) : UsageElement(name, " "){};
  void dumpValue(ostream &) const override{};
  void resetValue() override{};
};

//---------------------------------------------------------

static SpecialUsageElement bra("[");
static SpecialUsageElement ket("]");
static Switcher help("-help", "Print this help page");
static Switcher release("-release", "Print the current Toonz version");
static Switcher version("-version", "Print the current Toonz version");
static Switcher libRelease("-librelease", "");
// hidden: print the lib version

//=========================================================
//
// helper function
//
//---------------------------------------------------------
namespace {

//---------------------------------------------------------

void fetchElement(int index, int &argc, char *argv[]) {
  if (index >= argc) throw UsageError("missing argument");
  for (int i = index; i < argc - 1; i++) argv[i] = argv[i + 1];
  argc--;
}

//---------------------------------------------------------

void fetchElement(int &dst, int index, int &argc, char *argv[]) {
  string s = argv[index];
  if (isInt(s))
    dst = std::stoi(s);
  else
    throw UsageError("expected int");
  fetchElement(index, argc, argv);
}

//---------------------------------------------------------

void fetchElement(string &dst, int index, int &argc, char *argv[]) {
  char *s = argv[index];
  fetchElement(index, argc, argv);
  if (*s == '-') throw UsageError("expected argument");
  dst = string(s);
}

//---------------------------------------------------------

}  // namespace

//=========================================================
//
// UsageElement
//
//---------------------------------------------------------

UsageElement::UsageElement(string name, string help)
    : m_name(name), m_help(help), m_selected(false) {}

//---------------------------------------------------------

void UsageElement::print(ostream &out) const { out << m_name; }

//---------------------------------------------------------

void UsageElement::printHelpLine(ostream &out) const {
  out << "  " << m_name << endl;
  out << "       " << m_help << endl;
}

//=========================================================
//
// Qualifier
//
//---------------------------------------------------------

void Qualifier::print(ostream &out) const {
  if (isSwitcher())
    out << m_name;
  else
    out << "[ " << m_name << " ]";
}

//---------------------------------------------------------

void SimpleQualifier::fetch(int index, int &argc, char *argv[]) {
  fetchElement(index, argc, argv);
}

//---------------------------------------------------------

void SimpleQualifier::dumpValue(ostream &out) const {
  out << m_name << " = " << (isSelected() ? "on" : "off") << endl;
}

//---------------------------------------------------------

void SimpleQualifier::resetValue() { m_selected = false; }

//=========================================================
//
// Argument & MultiArgument
//
//---------------------------------------------------------

void Argument::fetch(int index, int &argc, char *argv[]) {
  if (index >= argc) throw UsageError("missing argument");
  if (!assign(argv[index]))
    throw UsageError(string("bad argument type :") + argv[index]);
  for (int i = index; i < argc - 1; i++) argv[i] = argv[i + 1];
  argc--;
}

//---------------------------------------------------------

void MultiArgument::fetch(int index, int &argc, char *argv[]) {
  if (index >= argc) throw UsageError("missing argument(s)");
  allocate(argc - index);
  for (m_index = 0; m_index < m_count; m_index++)
    if (!assign(argv[index + m_index]))
      throw UsageError(string("bad argument type :") + argv[index + m_index]);
  argc -= m_count;
}

//=========================================================
//
// UsageLine<T>
//
//---------------------------------------------------------

UsageLine::UsageLine() : m_count(0) {}

//---------------------------------------------------------

UsageLine::~UsageLine() {}

//---------------------------------------------------------

UsageLine::UsageLine(const UsageLine &src) {
  m_count = src.m_count;
  m_elements.reset(new UsageElementPtr[m_count]);
  ::memcpy(m_elements.get(), src.m_elements.get(),
           m_count * sizeof(m_elements[0]));
}

//---------------------------------------------------------

UsageLine &UsageLine::operator=(const UsageLine &src) {
  if (src.m_elements != m_elements) {
    m_elements.reset(new UsageElementPtr[src.m_count]);
    ::memcpy(m_elements.get(), src.m_elements.get(),
             src.m_count * sizeof(m_elements[0]));
  }
  m_count = src.m_count;
  return *this;
}

//---------------------------------------------------------

UsageLine::UsageLine(const UsageLine &src, UsageElement &elem) {
  m_count = src.m_count;
  m_elements.reset(new UsageElementPtr[m_count + 1]);
  ::memcpy(m_elements.get(), src.m_elements.get(),
           m_count * sizeof(m_elements[0]));
  m_elements[m_count++] = &elem;
}

//---------------------------------------------------------

UsageLine::UsageLine(int count) {
  m_count = count;
  m_elements.reset(new UsageElementPtr[m_count]);
  ::memset(m_elements.get(), 0, m_count * sizeof(m_elements[0]));
}

//---------------------------------------------------------

UsageLine::UsageLine(UsageElement &elem) {
  m_count = 1;
  m_elements.reset(new UsageElementPtr[m_count]);
  m_elements[0] = &elem;
}

//---------------------------------------------------------

UsageLine::UsageLine(UsageElement &a, UsageElement &b) {
  m_count = 2;
  m_elements.reset(new UsageElementPtr[m_count]);
  m_elements[0] = &a;
  m_elements[1] = &b;
}

//---------------------------------------------------------

UsageLine TCli::UsageLine::operator+(UsageElement &elem) {
  return UsageLine(*this, elem);
}

//---------------------------------------------------------

UsageLine TCli::operator+(UsageElement &a, UsageElement &b) {
  return UsageLine(a, b);
}

//---------------------------------------------------------

UsageLine TCli::operator+(const UsageLine &a, const Optional &b) {
  UsageLine ul(a.getCount() + b.getCount());
  int i;
  for (i = 0; i < a.getCount(); i++) ul[i] = a[i];
  for (int j = 0; j < b.getCount(); j++) ul[i++] = b[j];
  return ul;
}

//=========================================================

Optional::Optional(const UsageLine &ul) : UsageLine(ul.getCount() + 2) {
  m_elements[0]           = &bra;
  m_elements[m_count - 1] = &ket;
  for (int i = 0; i < ul.getCount(); i++) m_elements[i + 1]= ul[i];
}

//=========================================================
//
// UsageImp
//
//---------------------------------------------------------

class TCli::UsageImp {
  string m_progName;
  vector<UsageLine> m_usageLines;
  std::map<std::string, Qualifier *> m_qtable;
  vector<Qualifier *> m_qlist;
  vector<Argument *> m_args;
  typedef std::map<std::string, Qualifier *>::iterator qiterator;
  typedef std::map<std::string, Qualifier *>::const_iterator const_qiterator;

  UsageLine *m_selectedUsageLine;

public:
  UsageImp(string progName);
  ~UsageImp(){};
  void add(const UsageLine &);
  void addStandardUsages();

  void registerQualifier(Qualifier *q);
  void registerQualifier(string name, Qualifier *q);

  void registerArgument(Argument *arg);

  void printUsageLine(ostream &out, const UsageLine &ul) const;
  void printUsageLines(ostream &out) const;

  void print(ostream &out) const;

  void resetValues();
  void clear();
  void parse(int argc, char *argv[]);
  void fetchArguments(UsageLine &ul, int a, int b, int &argc, char *argv[]);
  bool matchSwitcher(const UsageLine &ul) const;
  bool hasSwitcher(const UsageLine &ul) const;
  bool matchArgCount(const UsageLine &ul, int a, int b, int argc) const;

  void dumpValues(ostream &out) const;

  void getArgCountRange(const UsageLine &ul, int a, int b, int &min,
                        int &max) const;

  static const int InfiniteArgCount;
};

//---------------------------------------------------------

const int UsageImp::InfiniteArgCount = 2048;

//---------------------------------------------------------

UsageImp::UsageImp(string progName)
    : m_progName(progName), m_selectedUsageLine(0) {
  addStandardUsages();
}

//---------------------------------------------------------

void UsageImp::addStandardUsages() {
  add(help);
  add(release);
  add(version);
  add(libRelease);
}

//---------------------------------------------------------

void UsageImp::clear() {
  m_usageLines.clear();
  m_qtable.clear();
  m_qlist.clear();
  m_args.clear();
  m_selectedUsageLine = 0;
  addStandardUsages();
}

//---------------------------------------------------------

void UsageImp::registerArgument(Argument *arg) {
  unsigned int i;
  for (i = 0; i < m_args.size() && m_args[i] != arg; i++) {
  }
  if (i == m_args.size()) m_args.push_back(arg);
}

//---------------------------------------------------------

void UsageImp::registerQualifier(string name, Qualifier *q) {
  m_qtable[name] = q;
  unsigned int i;
  for (i = 0; i < m_qlist.size() && m_qlist[i] != q; i++) {
  }
  if (i == m_qlist.size()) m_qlist.push_back(q);
}

//---------------------------------------------------------

void UsageImp::registerQualifier(Qualifier *q) {
  string str     = q->getName();
  const char *s0 = str.c_str();
  while (*s0 == ' ') s0++;
  const char *s = s0;
  for (;;) {
    if (*s != '-') {
      assert(!"Qualifier name must start with '-'");
    }
    do
      s++;
    while (isalnum(*s));
    if (s == s0 + 1) {
      assert(!"Empty qualifier name");
    }
    std::string name(s0, s - s0);
    registerQualifier(name, q);
    while (*s == ' ') s++;
    // ignoro gli eventuali parametri
    while (isalnum(*s)) {
      while (isalnum(*s)) s++;
      while (*s == ' ') s++;
    }
    if (*s != '|') break;
    s++;
    while (*s == ' ') s++;
    s0 = s;
  }
  if (*s != '\0') {
    assert(!"Unexpected character");
  }
}

//---------------------------------------------------------

void UsageImp::add(const UsageLine &ul) {
  m_usageLines.push_back(ul);
  for (int i = 0; i < ul.getCount(); i++) {
    Qualifier *q = dynamic_cast<Qualifier *>(ul[i]);
    if (q) registerQualifier(q);
    Argument *a = dynamic_cast<Argument *>(ul[i]);
    if (a) registerArgument(a);
  }
}

//---------------------------------------------------------

void UsageImp::printUsageLine(ostream &out, const UsageLine &ul) const {
  out << m_progName;
  for (int j = 0; j < ul.getCount(); j++)
    if (!ul[j]->isHidden()) {
      out << " ";
      ul[j]->print(out);
    }
  out << endl;
}

//---------------------------------------------------------

void UsageImp::printUsageLines(std::ostream &out) const {
  bool first = true;
  for (unsigned int i = 0; i < m_usageLines.size(); i++) {
    const UsageLine &ul = m_usageLines[i];
    int j;
    for (j = 0; j < ul.getCount(); j++)
      if (!ul[j]->isHidden()) break;
    if (j == ul.getCount()) continue;
    out << (first ? "usage: " : "       ");
    printUsageLine(out, ul);
    first = false;
  }
  out << endl;
}

//---------------------------------------------------------

void UsageImp::print(std::ostream &out) const {
  printUsageLines(out);
  out << endl;
  unsigned int i;
  for (i = 0; i < m_qlist.size(); i++)
    if (!m_qlist[i]->isHidden()) m_qlist[i]->printHelpLine(out);
  for (i = 0; i < m_args.size(); i++) m_args[i]->printHelpLine(out);
  out << endl;
}

//---------------------------------------------------------

void UsageImp::parse(int argc, char *argv[]) {
  resetValues();
  if (argc == 0 || argv[0] == 0) throw UsageError("missing program name");
  m_progName = string(argv[0]);
  unsigned int i;
  for (i = 1; i < (unsigned int)argc;)
    if (argv[i][0] == '-') {
      string qname(argv[i]);
      qiterator qit = m_qtable.find(qname);
      if (qit == m_qtable.end()) {
        string msg = "unknown qualifier '" + qname + "'";
        throw UsageError(msg);
      }
      Qualifier *qualifier = qit->second;
      qualifier->fetch(i, argc, argv);
      qualifier->select();
    } else
      i++;

  vector<UsageLine *> usages;

  for (i = 0; i < m_usageLines.size(); i++)
    if (hasSwitcher(m_usageLines[i]) && matchSwitcher(m_usageLines[i]))
      usages.push_back(&m_usageLines[i]);
  if (usages.empty())
    for (i = 0; i < m_usageLines.size(); i++)
      if (!hasSwitcher(m_usageLines[i])) usages.push_back(&m_usageLines[i]);
  if (usages.size() == 0) throw UsageError("unrecognized syntax");
  if (usages.size() > 1) {
    int min = InfiniteArgCount;
    int max = 0;
    std::vector<UsageLine *>::iterator it;
    for (it = usages.begin(); it != usages.end();) {
      UsageLine &ul = **it;
      int lmin, lmax;
      getArgCountRange(ul, 0, ul.getCount() - 1, lmin, lmax);
      min = std::min(min, lmin);
      max = std::max(max, lmax);
      if (matchArgCount(ul, 0, ul.getCount() - 1, argc - 1) == false)
        it = usages.erase(it);
      else
        it++;
    }
    if (usages.size() == 0) {
      if (argc - 1 < min)
        throw UsageError("missing argument(s)");
      else if (argc - 1 > max)
        throw UsageError("too many arguments");
      else
        throw UsageError("bad argument number");
    } else if (usages.size() > 1)
      throw UsageError("ambiguous syntax");
  }

  if (usages.size() == 1) {
    UsageLine *found = usages[0];
    if (found->getCount() > 0)
      fetchArguments(*found, 0, found->getCount() - 1, argc, argv);
    if (argc > 1) throw UsageError("too many arguments");
    m_selectedUsageLine = found;
  }
}

//---------------------------------------------------------

void UsageImp::fetchArguments(UsageLine &ul, int a, int b, int &argc,
                              char *argv[]) {
  assert(0 <= a && a <= b && b < ul.getCount());
  int i;
  for (i = a; i <= b; i++) {
    if (ul[i] == &bra || ul[i]->isMultiArgument()) break;
    if (ul[i]->isArgument()) {
      Argument *argument = dynamic_cast<Argument *>(ul[i]);
      assert(argument);
      argument->fetch(1, argc, argv);
      argument->select();
    }
  }
  if (i > b) {
  } else if (ul[i] == &bra) {
    int n = 0;
    int j;
    for (j = b; j > i && ul[j] != &ket; j--)
      if (ul[j]->isArgument()) n++;
    assert(j > i + 1);
    if (argc - 1 > n) fetchArguments(ul, i + 1, j - 1, argc, argv);
    if (j + 1 <= b) fetchArguments(ul, j + 1, b, argc, argv);
  } else if (ul[i]->isMultiArgument()) {
    MultiArgument *argument = dynamic_cast<MultiArgument *>(ul[i]);
    assert(argument);
    int n = 0;
    for (int j = i + 1; j <= b; j++)
      if (ul[j]->isArgument()) n++;
    int oldArgc = argc;
    argc -= n;
    argument->fetch(1, argc, argv);
    argument->select();
    argc += n;
    if (n > 0) {
      if (argc < oldArgc)
        for (int j = n; j > 0; j--) argv[argc - j] = argv[oldArgc - j];
      fetchArguments(ul, i + 1, b, argc, argv);
    }
  }
}

//---------------------------------------------------------

bool UsageImp::matchSwitcher(const UsageLine &ul) const {
  for (int i = 0; i < ul.getCount(); i++)
    if (ul[i]->isSwitcher() && !ul[i]->isSelected()) return false;
  return true;
}

//---------------------------------------------------------

bool UsageImp::hasSwitcher(const UsageLine &ul) const {
  for (int i = 0; i < ul.getCount(); i++)
    if (ul[i]->isSwitcher()) return true;
  return false;
}

//---------------------------------------------------------

bool UsageImp::matchArgCount(const UsageLine &ul, int a, int b,
                             int argc) const {
  int n = 0;
  int i;
  for (i = a; i <= b; i++) {
    if (ul[i] == &bra || ul[i]->isMultiArgument()) break;
    if (ul[i]->isArgument()) n++;
  }
  if (i > b)
    return argc == n;
  else if (ul[i] == &bra) {
    int j;
    for (j = b; j > i && ul[j] != &ket; j--)
      if (ul[j]->isArgument()) n++;
    assert(j > i + 1);
    if (n == argc)
      return true;
    else if (n > argc)
      return false;
    else
      return matchArgCount(ul, i + 1, j - 1, argc - n);
  } else {
    assert(ul[i]->isMultiArgument());
    n++;
    for (i++; i <= b; i++)
      if (ul[i]->isArgument()) n++;
    return argc >= n;
  }
}

//---------------------------------------------------------

void UsageImp::getArgCountRange(const UsageLine &ul, int a, int b, int &min,
                                int &max) const {
  min = max = 0;
  int n     = 0;
  int i;
  for (i = a; i <= b; i++) {
    if (ul[i] == &bra || ul[i]->isMultiArgument()) break;
    if (ul[i]->isArgument()) n++;
  }
  if (i > b) {
    min = max = n;
    return;
  } else if (ul[i] == &bra) {
    int j;
    for (j = b; j > i && ul[j] != &ket; j--)
      if (ul[j]->isArgument()) n++;
    assert(j > i + 1);
    min = max = n;
    int lmin, lmax;
    getArgCountRange(ul, i + 1, j - 1, lmin, lmax);
    if (lmax == InfiniteArgCount)
      max = InfiniteArgCount;
    else
      max += lmax;
  } else {
    assert(ul[i]->isMultiArgument());
    n++;
    for (i++; i <= b; i++)
      if (ul[i]->isArgument()) n++;
    min = n;
    max = InfiniteArgCount;
  }
}

//---------------------------------------------------------

void UsageImp::dumpValues(std::ostream &out) const {
  if (m_selectedUsageLine == 0) return;
  cout << "selected usage: ";
  printUsageLine(out, *m_selectedUsageLine);
  unsigned int i;
  for (i = 0; i < m_qlist.size(); i++) m_qlist[i]->dumpValue(out);
  for (i = 0; i < m_args.size(); i++) m_args[i]->dumpValue(out);
  out << endl << endl;
}

//---------------------------------------------------------

void UsageImp::resetValues() {
  unsigned int i;
  for (i = 0; i < m_qlist.size(); i++) m_qlist[i]->resetValue();
  for (i = 0; i < m_args.size(); i++) m_args[i]->resetValue();
}

//=========================================================
//
// Usage
//
//---------------------------------------------------------

Usage::Usage(string progName) : m_imp(new UsageImp(progName)) {}

Usage::~Usage() {}

void Usage::add(const UsageLine &ul) { m_imp->add(ul); }

void Usage::print(std::ostream &out) const { m_imp->print(out); }

void Usage::dumpValues(std::ostream &out) const { m_imp->dumpValues(out); }

void Usage::clear() { m_imp->clear(); }

bool Usage::parse(const char *argvString, std::ostream &err) {
  string s = string(argvString);
  std::vector<char *> argv;
  int len = s.size();
  for (int i = 0; i < len;) {
    while (s[i] == ' ' || s[i] == '\t') i++;
    if (i < len) argv.push_back(&s[i]);
    while (i < len && !(s[i] == ' ' || s[i] == '\t')) i++;
    if (i < len) s[i++] = '\0';
  }
  return parse(argv.size(), &argv[0], err);
}

bool Usage::parse(int argc, char *argv[], std::ostream &err) {
  try {
    m_imp->parse(argc, argv);
    if (help) {
      print(err);
      return false;
    }
    if (release || version) {
      printToonzRelease(err);
      return false;
    }
    if (libRelease) {
      printLibRelease(err);
      return false;
    }
    return true;
  } catch (UsageError e) {
    err << "Usage error: " << e.getError() << endl;
    m_imp->printUsageLines(err);
    return false;
  }
}

//=========================================================
//
// RangeQualifier
//
//---------------------------------------------------------

RangeQualifier::RangeQualifier()
    : Qualifier("-range from to | -frame fr", "frame range")
    , m_from(0)
    , m_to(-1) {}

//---------------------------------------------------------

void RangeQualifier::fetch(int index, int &argc, char *argv[]) {
  assert(index < argc);
  string qname(argv[index]);
  fetchElement(index, argc, argv);
  if (qname == "-range") {
    fetchElement(m_from, index, argc, argv);
    fetchElement(m_to, index, argc, argv);
  } else if (qname == "-frame") {
    fetchElement(m_from, index, argc, argv);
    m_to = m_from;
  } else {
    assert(0);
  }
}

static std::ostream &operator<<(std::ostream &out,
                                const RangeQualifier &range) {
  return out << "[" << range.getFrom() << ", " << range.getTo() << "]";
}

//---------------------------------------------------------

void RangeQualifier::dumpValue(std::ostream &out) const {
  out << m_name << " = ";
  if (m_from <= m_to)
    out << m_from << ", " << m_to;
  else
    out << "undefined";
  out << endl;
}

//---------------------------------------------------------

void RangeQualifier::resetValue() {
  m_from     = 0;
  m_to       = -1;
  m_selected = false;
}
