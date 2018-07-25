

// TnzCore includes
#include "tconst.h"
#include "tutil.h"
#include "tstream.h"

// TnzBase includes
#include "tparamcontainer.h"
#include "tfxattributes.h"
#include "texternfx.h"
#include "tpassivecachemanager.h"

// STD includes
#include <set>

#include "tfx.h"

//==============================================================================

TIStream &operator>>(TIStream &in, TFxP &p) {
  TPersist *tmp = 0;
  in >> tmp;
  p = TFxP(dynamic_cast<TFx *>(tmp));
  return in;
}

//==============================================================================
//
// namespace {}
//
//------------------------------------------------------------------------------

namespace {

//------------------------------------------------------------------------------

typedef std::pair<std::string, TFxPort *> NamePort;
typedef std::map<std::string, TFxPort *> PortTable;
typedef std::vector<NamePort> PortArray;

//------------------------------------------------------------------------------

class PortNameEq {
  std::string m_name;

public:
  PortNameEq(const std::string &name) : m_name(name) {}
  ~PortNameEq() {}
  bool operator()(const NamePort &np) { return np.first == m_name; }
};

//------------------------------------------------------------------------------

void skipChild(TIStream &is) {
  while (!is.eos()) {
    std::string dummy = is.getString();
    std::string tagName;
    while (is.openChild(tagName)) {
      skipChild(is);
      if (is.isBeginEndTag()) is.matchTag(tagName);
      is.closeChild();
    }
  }
}

//------------------------------------------------------------------------------

TPointD updateDagPosition(const TPointD &pos, const VersionNumber &tnzVersion) {
  if (tnzVersion < VersionNumber(1, 16)) return TConst::nowhere;
  return pos;
}

}  // namespace

//==============================================================================
//
// TFxParamChange
//
//------------------------------------------------------------------------------

TFxParamChange::TFxParamChange(TFx *fx, double firstAffectedFrame,
                               double lastAffectedFrame, bool dragging)
    : TFxChange(fx, firstAffectedFrame, lastAffectedFrame, dragging) {}

//--------------------------------------------------

TFxParamChange::TFxParamChange(TFx *fx, const TParamChange &src)
    : TFxChange(fx, src.m_firstAffectedFrame, src.m_lastAffectedFrame,
                src.m_dragging) {}

//==============================================================================
//
// TFxChange
//
//------------------------------------------------------------------------------

double TFxChange::m_minFrame = -(std::numeric_limits<double>::max)();
double TFxChange::m_maxFrame = +(std::numeric_limits<double>::max)();

//==============================================================================
//
// TFxPortDynamicGroup
//
//------------------------------------------------------------------------------

TFxPortDynamicGroup::TFxPortDynamicGroup(const std::string &prefix, int minSize)
    : m_portsPrefix(prefix), m_minPortsCount(minSize) {}

//--------------------------------------------------

TFxPortDynamicGroup::~TFxPortDynamicGroup() { clear(); }

//--------------------------------------------------

void TFxPortDynamicGroup::addPort(TFxPort *port) { m_ports.push_back(port); }

//--------------------------------------------------

void TFxPortDynamicGroup::removePort(TFxPort *port) {
  m_ports.resize(std::remove(m_ports.begin(), m_ports.end(), port) -
                 m_ports.begin());
  delete port;
}

//--------------------------------------------------

void TFxPortDynamicGroup::clear() {
  std::for_each(m_ports.begin(), m_ports.end(), TDeleteObjectFunctor());
  m_ports.clear();
}

//==============================================================================
//
// TFxImp
//
//------------------------------------------------------------------------------

class TFxImp {
public:
  TFx *m_fx;                //!< Fx back-pointer
  TFxImp *m_prev, *m_next;  //!< Linked fxs

  std::wstring m_name;
  std::wstring m_fxId;

  PortTable m_portTable;  //!< Name -> port  map
  PortArray m_portArray;  //!< Ports container

  TParamContainer m_paramContainer;

  std::set<TFxPort *> m_outputPort;
  TFxTimeRegion m_activeTimeRegion;
  std::set<TFxObserver *> m_observers;

  TFxAttributes m_attributes;

  static unsigned long m_nextId;
  unsigned long m_id;  //!< Unique fx identifier, per Toonz session.
                       //!< It is intended to be used \b solely to build
                       //!< an internal string description of the fx.
public:
  TFxImp(TFx *fx)
      : m_fx(fx), m_activeTimeRegion(TFxTimeRegion::createUnlimited()), m_id() {
    m_prev = m_next = this;
  }

  ~TFxImp() {
    m_prev->m_next = m_next;
    m_next->m_prev = m_prev;
  }

  bool checkLinks() const {
    assert(m_prev);
    assert(m_next);
    assert(m_prev->m_next == this);
    assert(m_next->m_prev == this);

    return true;
  }

private:
  // not copyable
  TFxImp(const TFxImp &);
  TFxImp &operator=(const TFxImp &);
};

//--------------------------------------------------

unsigned long TFxImp::m_nextId = 0;

//==============================================================================
//
// TFxFactory
//
//------------------------------------------------------------------------------

class TFxFactory  // singleton
{
  typedef std::map<std::string, std::pair<TFxInfo, TFxDeclaration *>> Table;

  Table m_table;
  std::vector<std::string> m_map;

  TFxFactory() {}

public:
  static TFxFactory *instance() {
    static TFxFactory _instance;
    return &_instance;
  }

  void add(const TFxInfo &info, TFxDeclaration *decl) {
    // check for dups ???
    std::pair<TFxInfo, TFxDeclaration *> p(info, decl);
    m_table[info.m_name] = p;
  }

  TFx *create(std::string name) {
    Table::iterator it = m_table.find(name);
    if (it != m_table.end()) {
      TFxDeclaration *decl = it->second.second;

      TPersist *obj = decl->create();
      assert(obj);

      return dynamic_cast<TFx *>(obj);
    } else
      return TExternFx::create(name);
  }

  void getFxInfos(std::vector<TFxInfo> &info) const {
    for (Table::const_iterator it = m_table.begin(); it != m_table.end(); ++it)
      info.push_back(it->second.first);
  }

  TFxInfo getFxInfo(const std::string &fxIdentifier) const {
    Table::const_iterator it = m_table.find(fxIdentifier);
    return (it != m_table.end()) ? it->second.first : TFxInfo();
  }
};

//==============================================================================
//
// TFxDeclaration
//
//------------------------------------------------------------------------------

TFxDeclaration::TFxDeclaration(const TFxInfo &info)
    : TPersistDeclaration(info.m_name) {
  TFxFactory::instance()->add(info, this);
}

//==============================================================================
//
// TFx
//
//------------------------------------------------------------------------------

DEFINE_CLASS_CODE(TFx, 3)

//------------------------------------------------------------------------------

TFx::TFx()
    : TSmartObject(m_classCode)  // TPersist(TFxImp::m_instances)
    , m_imp(new TFxImp(this)) {}

//--------------------------------------------------

TFx::~TFx() {
  for (std::set<TFxPort *>::iterator it = m_imp->m_outputPort.begin();
       it != m_imp->m_outputPort.end(); ++it) {
    TFxPort *port = *it;
    port->setFx(0);
  }

  delete m_imp;
}

//--------------------------------------------------

std::wstring TFx::getName() const { return m_imp->m_name; }

//--------------------------------------------------

void TFx::setName(std::wstring name) { m_imp->m_name = name; }

//--------------------------------------------------

std::wstring TFx::getFxId() const { return m_imp->m_fxId; }

//--------------------------------------------------

void TFx::setFxId(std::wstring id) { m_imp->m_fxId = id; }

//--------------------------------------------------

TFxAttributes *TFx::getAttributes() const { return &m_imp->m_attributes; }

//--------------------------------------------------

const TParamContainer *TFx::getParams() const {
  return &m_imp->m_paramContainer;
}

//--------------------------------------------------

TParamContainer *TFx::getParams() { return &m_imp->m_paramContainer; }

//--------------------------------------------------

TFx *TFx::clone(bool recursive) const {
  TFx *fx = TFx::create(getFxType());
  assert(fx);
  return this->clone(fx, recursive);
}

TFx *TFx::clone(TFx *fx, bool recursive) const {
  // Copy misc stuff

  fx->m_imp->m_activeTimeRegion = m_imp->m_activeTimeRegion;
  fx->setIdentifier(getIdentifier());
  fx->getParams()->copy(getParams());
  fx->setFxId(getFxId());
  fx->setName(getName());

  // Copy attributes
  *fx->getAttributes() = *getAttributes();

  // Clone the dynamic ports pool
  if (hasDynamicPortGroups()) {
    int p, pCount = m_imp->m_portArray.size();
    for (p = 0; p != pCount; ++p) {
      const NamePort &namedPort = m_imp->m_portArray[p];

      int groupIdx = namedPort.second->getGroupIndex();
      if (groupIdx >= 0 && !fx->getInputPort(namedPort.first))
        fx->addInputPort(namedPort.first, new TRasterFxPort, groupIdx);
    }

    assert(pCount == fx->getInputPortCount());
  }

  // copia ricorsiva sulle porte
  if (recursive) {
    int p, pCount = getInputPortCount();
    for (p = 0; p != pCount; ++p) {
      TFxPort *port = getInputPort(p);
      if (port->getFx())
        fx->connect(getInputPortName(p), port->getFx()->clone(true));
    }
  }

  return fx;
}

//--------------------------------------------------

void TFx::linkParams(TFx *fx) {
  if (this == fx) return;
  getParams()->link(fx->getParams());
  m_imp->m_activeTimeRegion = fx->m_imp->m_activeTimeRegion;

  // aggiorno i link
  assert(m_imp->checkLinks());
  assert(fx->m_imp->checkLinks());

  std::swap(m_imp->m_next, fx->m_imp->m_next);
  std::swap(m_imp->m_next->m_prev, fx->m_imp->m_next->m_prev);

  assert(m_imp->checkLinks());
  assert(fx->m_imp->checkLinks());
}

//--------------------------------------------------

void TFx::unlinkParams() {
  // clone dei parametri
  getParams()->unlink();

  assert(m_imp->m_prev);
  assert(m_imp->m_next);
  assert(m_imp->m_prev->m_next == m_imp);
  assert(m_imp->m_next->m_prev == m_imp);
  m_imp->m_prev->m_next = m_imp->m_next;
  m_imp->m_next->m_prev = m_imp->m_prev;
  m_imp->m_next = m_imp->m_prev = m_imp;

  notify(TFxParamsUnlinked(this));
}

//--------------------------------------------------

bool TFx::addInputPort(const std::string &name, TFxPort &port) {
  PortTable::iterator it = m_imp->m_portTable.find(name);
  if (it != m_imp->m_portTable.end()) return false;

  m_imp->m_portTable[name] = &port;
  m_imp->m_portArray.push_back(NamePort(name, &port));
  port.setOwnerFx(this);  // back pointer to the owner...

  return true;
}

//--------------------------------------------------

bool TFx::addInputPort(const std::string &name, TFxPort *port, int groupIndex) {
  if (!port) {
    assert(port);
    return false;
  }

  if (groupIndex >= dynamicPortGroupsCount()) {
    assert(groupIndex < dynamicPortGroupsCount());
    return false;
  }

  if (!addInputPort(name, *port)) return false;

  // Assign the port to the associated group
  port->m_groupIdx = groupIndex;

  TFxPortDG *group = const_cast<TFxPortDG *>(dynamicPortGroup(groupIndex));
  group->addPort(port);

  assert(name.find(group->m_portsPrefix) == 0);

  return true;
}

//--------------------------------------------------

bool TFx::removeInputPort(const std::string &name) {
  m_imp->m_portTable.erase(name);

  PortArray::iterator it = std::find_if(
      m_imp->m_portArray.begin(), m_imp->m_portArray.end(), PortNameEq(name));
  if (it == m_imp->m_portArray.end()) return false;

  TFxPort *port = it->second;
  port->setOwnerFx(0);

  if (port->m_groupIdx >= 0) {
    TFxPortDG *group =
        const_cast<TFxPortDG *>(this->dynamicPortGroup(port->m_groupIdx));
    group->removePort(port);  // The port is DELETED here
  }

  m_imp->m_portArray.erase(it);
  return true;
}

//--------------------------------------------------

namespace {
struct IsPrefix {
  const std::string &m_prefix;
  bool operator()(const NamePort &nameport) {
    return (strncmp(m_prefix.c_str(), nameport.first.c_str(),
                    m_prefix.size()) == 0);
  }
};
}  // namespace

void TFx::clearDynamicPortGroup(int g) {
  TFxPortDG *dg = const_cast<TFxPortDG *>(this->dynamicPortGroup(g));

  const std::string &prefix = dg->portsPrefix();

  std::string prefixEnd = prefix;
  ++prefixEnd[prefixEnd.size() - 1];

  {
    // Delete all associated ports from the ports table
    PortTable::iterator pBegin(m_imp->m_portTable.lower_bound(prefix));
    PortTable::iterator pEnd(m_imp->m_portTable.lower_bound(prefixEnd));

    m_imp->m_portTable.erase(pBegin, pEnd);

    // Traverse the ports array and remove all ports in the group
    IsPrefix func = {prefix};
    m_imp->m_portArray.resize(std::remove_if(m_imp->m_portArray.begin(),
                                             m_imp->m_portArray.end(), func) -
                              m_imp->m_portArray.begin());
  }

  dg->clear();  // Has ports ownership, so deletes them
}

//--------------------------------------------------

bool TFx::addOutputConnection(TFxPort *port) {
  assert(port->getFx() == this);
  return m_imp->m_outputPort.insert(port).second;
}

//--------------------------------------------------

bool TFx::removeOutputConnection(TFxPort *port) {
  std::set<TFxPort *>::iterator it = m_imp->m_outputPort.find(port);
  if (it == m_imp->m_outputPort.end()) return false;

  m_imp->m_outputPort.erase(it);
  return true;
}

//--------------------------------------------------

int TFx::getOutputConnectionCount() const { return m_imp->m_outputPort.size(); }

//--------------------------------------------------

TFxPort *TFx::getOutputConnection(int i) const {
  assert(0 <= i && i <= (int)m_imp->m_outputPort.size());
  std::set<TFxPort *>::iterator it = m_imp->m_outputPort.begin();
  std::advance(it, i);
  if (it == m_imp->m_outputPort.end()) return 0;
  return *it;
}

//--------------------------------------------------

bool TFx::disconnect(const std::string &name) {
  TFxPort *port = getInputPort(name);
  if (!port) return false;

  port->setFx(0);
  return true;
}

//--------------------------------------------------

bool TFx::connect(const std::string &name, TFx *fx) {
  TFxPort *port = getInputPort(name);
  if (!port) return false;

  port->setFx(fx);
  return true;
}

//--------------------------------------------------

int TFx::getInputPortCount() const { return m_imp->m_portArray.size(); }

//--------------------------------------------------

TFxPort *TFx::getInputPort(int index) const {
  assert(0 <= index && index < (int)m_imp->m_portArray.size());
  return m_imp->m_portArray[index].second;
}

//--------------------------------------------------

std::string TFx::getInputPortName(int index) const {
  assert(0 <= index && index < (int)(m_imp->m_portArray.size()));
  return m_imp->m_portArray[index].first;
}

//--------------------------------------------------

bool TFx::renamePort(const std::string &oldName, const std::string &newName) {
  PortTable::iterator it = m_imp->m_portTable.find(oldName);
  if (it == m_imp->m_portTable.end()) return false;

  TFxPort *port = it->second;
  m_imp->m_portTable.erase(it);
  m_imp->m_portTable[newName] = port;

  PortArray::iterator it2;
  for (it2 = m_imp->m_portArray.begin(); it2 != m_imp->m_portArray.end();
       ++it2) {
    if (it2->first != oldName) continue;

    it2->first = newName;
    break;
  }

  return true;
}

//--------------------------------------------------

TFxPort *TFx::getInputPort(const std::string &name) const {
  PortTable::iterator it = m_imp->m_portTable.find(name);
  if (it == m_imp->m_portTable.end()) return 0;

  return m_imp->m_portTable[name];
}

//--------------------------------------------------

int TFx::getReferenceColumnIndex() const {
  if (!m_imp->m_portArray.empty()) {
    TFx *fx = m_imp->m_portArray[0].second->getFx();
    if (fx) return fx->getReferenceColumnIndex();
  }
  return -1;
}

//--------------------------------------------------

TFxTimeRegion TFx::getTimeRegion() const {
  if (m_imp->m_portTable.empty()) return TFxTimeRegion::createUnlimited();

  TFxTimeRegion tr((std::numeric_limits<double>::max)(),
                   -(std::numeric_limits<double>::max)());

  PortTable::iterator it = m_imp->m_portTable.begin();
  for (; it != m_imp->m_portTable.end(); ++it) {
    TFxPort *port = it->second;
    if (port && port->isConnected() && !port->isaControlPort()) {
      TFx *fx = port->getFx();
      assert(fx);
      tr += fx->getTimeRegion();
    }
  }

  return tr;
}

//--------------------------------------------------

void TFx::setActiveTimeRegion(const TFxTimeRegion &tr) {
  m_imp->m_activeTimeRegion = tr;
}

//--------------------------------------------------

TFxTimeRegion TFx::getActiveTimeRegion() const {
  return m_imp->m_activeTimeRegion;
}

//--------------------------------------------------

void TFx::onChange(const TParamChange &c) {
  TFxParamChange change(this, c);
  notify(change);
}

//--------------------------------------------------

void TFx::addObserver(TFxObserver *obs) { m_imp->m_observers.insert(obs); }

//--------------------------------------------------

void TFx::removeObserver(TFxObserver *obs) { m_imp->m_observers.erase(obs); }

//--------------------------------------------------

void TFx::notify(const TFxChange &change) {
  for (std::set<TFxObserver *>::iterator it = m_imp->m_observers.begin();
       it != m_imp->m_observers.end(); ++it)
    (*it)->onChange(change);
}

//--------------------------------------------------

void TFx::notify(const TFxPortAdded &change) {
  for (std::set<TFxObserver *>::iterator it = m_imp->m_observers.begin();
       it != m_imp->m_observers.end(); ++it)
    (*it)->onChange(change);
}

//--------------------------------------------------

void TFx::notify(const TFxPortRemoved &change) {
  for (std::set<TFxObserver *>::iterator it = m_imp->m_observers.begin();
       it != m_imp->m_observers.end(); ++it)
    (*it)->onChange(change);
}

//--------------------------------------------------

void TFx::notify(const TFxParamAdded &change) {
  for (std::set<TFxObserver *>::iterator it = m_imp->m_observers.begin();
       it != m_imp->m_observers.end(); ++it)
    (*it)->onChange(change);
}

//--------------------------------------------------

void TFx::notify(const TFxParamRemoved &change) {
  for (std::set<TFxObserver *>::iterator it = m_imp->m_observers.begin();
       it != m_imp->m_observers.end(); ++it)
    (*it)->onChange(change);
}

//--------------------------------------------------

unsigned long TFx::getIdentifier() const { return m_imp->m_id; }

//--------------------------------------------------

void TFx::setIdentifier(unsigned long id) { m_imp->m_id = id; }

//--------------------------------------------------

void TFx::setNewIdentifier() { m_imp->m_id = ++m_imp->m_nextId; }

//--------------------------------------------------

void TFx::loadData(TIStream &is) {
  std::string tagName;
  VersionNumber tnzVersion = is.getVersion();

  QList<int> groupIds;
  QList<std::wstring> groupNames;
  while (is.openChild(tagName)) {
    if (tagName == "params") {
      while (!is.eos()) {
        std::string paramName;
        while (is.openChild(paramName)) {
          TParamVar *paramVar = getParams()->getParamVar(paramName);
          if (paramVar && paramVar->getParam()) {
            paramVar->getParam()->loadData(is);
            if (paramVar->isObsolete())
              onObsoleteParamLoaded(paramVar->getParam()->getName());
          } else  // il parametro non e' presente -> skip
            skipChild(is);

          is.closeChild();
        }
      }
    } else if (tagName == "paramsLinkedTo") {
      TPersist *p = 0;
      is >> p;
      TFx *fx = dynamic_cast<TFx *>(p);
      if (fx == 0) throw TException("Missing linkedSetRoot");
      linkParams(fx);
    } else if (tagName == "ports") {
      std::string portName;
      while (!is.eos()) {
        while (is.openChild(portName)) {
          VersionNumber version = is.getVersion();
          compatibilityTranslatePort(version.first, version.second, portName);

          // Try to find the specified port
          TFxPort *port = getInputPort(portName);
          if (!port) {
            // Scan dynamic port groups for a containing one - add a port there
            // if found
            int g, gCount = dynamicPortGroupsCount();
            for (g = 0; g != gCount; ++g) {
              TFxPortDG *group = const_cast<TFxPortDG *>(dynamicPortGroup(g));

              if (group->contains(portName)) {
                addInputPort(portName, port = new TRasterFxPort, g);
                break;
              }
            }
          }

          // Could not find (or add) a port with the corresponding name - throw
          if (!port)
            throw TException(std::string("port '") + portName +
                             "' is not present");

          if (!is.eos()) {
            TPersist *p = 0;
            is >> p;
            TFx *fx = dynamic_cast<TFx *>(p);
            port->setFx(fx);
          }

          is.closeChild();
        }
      }
    } else if (tagName == "dagNodePos") {
      TPointD p;
      is >> p.x >> p.y;
      m_imp->m_attributes.setDagNodePos(updateDagPosition(p, tnzVersion));
    } else if (tagName == "numberId") {
      int numberId = 0;
      is >> numberId;
      m_imp->m_attributes.setId(numberId);
    } else if (tagName == "passiveCacheId") {
      int passiveCacheId = 0;
      is >> passiveCacheId;

      assert(passiveCacheId > 0);
      TPassiveCacheManager::instance()->declareCached(this, passiveCacheId);
    } else if (tagName == "name") {
      // passo attraverso un filepath solo per evitare i problemi di blank
      // o caratteri strani dentro il nome (sospetto che tfilepath sia gestito
      // correttamente mentre wstring no
      TFilePath tmp;
      is >> tmp;
      setName(tmp.getWideName());
    } else if (tagName == "fxId") {
      TFilePath tmp;
      is >> tmp;
      setFxId(tmp.getWideName());
    } else if (tagName == "enabled") {
      int flag = 1;
      is >> flag;
      m_imp->m_attributes.enable(flag != 0);
    } else if (tagName == "opened") {
      int opened = 1;
      is >> opened;
      m_imp->m_attributes.setIsOpened(opened);
    } else if (tagName == "groupIds") {
      int groupId;
      while (!is.eos()) {
        is >> groupId;
        groupIds.append(groupId);
      }
    } else if (tagName == "groupNames") {
      std::wstring groupName;
      while (!is.eos()) {
        is >> groupName;
        groupNames.append(groupName);
      }
    } else {
      throw TException("Unknown tag!");
    }
    is.closeChild();
  }
  if (!groupIds.isEmpty()) {
    assert(groupIds.size() == groupNames.size());
    int i;
    for (i = 0; i < groupIds.size(); i++) {
      m_imp->m_attributes.setGroupId(groupIds[i]);
      m_imp->m_attributes.setGroupName(groupNames[i]);
    }
  }
}

//--------------------------------------------------

void TFx::saveData(TOStream &os) {
  TFx *linkedSetRoot = this;
  if (m_imp->m_next != m_imp) {
    TFxImp *imp = m_imp->m_next;
    int guard   = 0;
    while (guard++ < 1000 && imp != m_imp) {
      if (imp->m_fx < linkedSetRoot) linkedSetRoot = imp->m_fx;
      imp                                          = imp->m_next;
    }
    assert(imp == m_imp);
    assert(linkedSetRoot);
  }
  if (linkedSetRoot == this) {
    os.openChild("params");
    for (int i = 0; i < getParams()->getParamCount(); i++) {
      std::string paramName     = getParams()->getParamName(i);
      const TParamVar *paramVar = getParams()->getParamVar(i);
      // skip saving for the obsolete parameters
      if (paramVar->isObsolete()) continue;
      os.openChild(paramName);
      paramVar->getParam()->saveData(os);
      os.closeChild();
    }
    os.closeChild();
  } else {
    os.openChild("paramsLinkedTo");
    os << linkedSetRoot;
    os.closeChild();
  }

  os.openChild("ports");
  for (PortTable::iterator pit = m_imp->m_portTable.begin();
       pit != m_imp->m_portTable.end(); ++pit) {
    os.openChild(pit->first);
    if (pit->second->isConnected())
      os << TFxP(pit->second->getFx()).getPointer();
    os.closeChild();
  }
  os.closeChild();

  TPointD p = m_imp->m_attributes.getDagNodePos();
  if (p != TConst::nowhere) os.child("dagNodePos") << p.x << p.y;
  int numberId = m_imp->m_attributes.getId();
  os.child("numberId") << numberId;
  bool cacheEnabled = TPassiveCacheManager::instance()->cacheEnabled(this);
  if (cacheEnabled)
    os.child("passiveCacheId")
        << TPassiveCacheManager::instance()->getPassiveCacheId(this);
  std::wstring name = getName();
  if (name != L"") os.child("name") << TFilePath(name);
  std::wstring fxId = getFxId();
  os.child("fxId") << fxId;
  if (!m_imp->m_attributes.isEnabled()) os.child("enabled") << 0;
  os.child("opened") << (int)m_imp->m_attributes.isOpened();
  if (m_imp->m_attributes.isGrouped()) {
    os.openChild("groupIds");
    QStack<int> groupIdStack = m_imp->m_attributes.getGroupIdStack();
    int i;
    for (i = 0; i < groupIdStack.size(); i++) os << groupIdStack[i];
    os.closeChild();
    os.openChild("groupNames");
    QStack<std::wstring> groupNameStack =
        m_imp->m_attributes.getGroupNameStack();
    for (i = 0; i < groupNameStack.size(); i++) os << groupNameStack[i];
    os.closeChild();
  }
}

//--------------------------------------------------

void TFx::loadPreset(TIStream &is) {
  std::string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "dvpreset") {
      std::string fxId = is.getTagAttribute("fxId");
      if (fxId != getFxType())
        throw TException("Preset doesn't match the fx type");
    } else if (tagName == "params") {
      while (!is.eos()) {
        std::string paramName;
        while (is.openChild(paramName)) {
          try {
            TParamP param = getParams()->getParam(paramName);
            param->loadData(is);
          } catch (TException &) { /*skip*/
          }                        // il parametro non e' presente
          is.closeChild();
        }
      }
    } else {
      throw TException("Fx preset unknown tag!");
    }
  }
}

//--------------------------------------------------

void TFx::savePreset(TOStream &os) {
  std::map<std::string, std::string> attributes;
  attributes.insert(std::make_pair(std::string("ver"), std::string("1.0")));
  attributes.insert(std::make_pair(std::string("fxId"), getFxType()));

  os.openChild("dvpreset", attributes);

  os.openChild("params");
  for (int i = 0; i < getParams()->getParamCount(); i++) {
    std::string paramName = getParams()->getParamName(i);
    TParam *param         = getParams()->getParam(i);
    os.openChild(paramName);
    param->saveData(os);
    os.closeChild();
  }
  os.closeChild();

  os.closeChild();
}

//--------------------------------------------------

void TFx::disconnectAll() {
  int p, pCount = getInputPortCount();
  for (p = 0; p != pCount; ++p) getInputPort(p)->setFx(0);
}

//--------------------------------------------------

TFx *TFx::create(std::string name) {
  return TFxFactory::instance()->create(name);
}

//--------------------------------------------------

void TFx::listFxs(std::vector<TFxInfo> &info) {
  TFxFactory::instance()->getFxInfos(info);
}

//--------------------------------------------------

TFxInfo TFx::getFxInfo(const std::string &fxIdentifier) {
  return TFxFactory::instance()->getFxInfo(fxIdentifier);
}

//--------------------------------------------------

TFx *TFx::getLinkedFx() const {
  assert(m_imp->m_next);
  assert(m_imp->m_next->m_prev == m_imp);
  assert(m_imp->m_next->m_fx != 0);
  return m_imp->m_next->m_fx;
}

//===================================================
//
// TFxTimeRegion
//
//--------------------------------------------------

//! Creates an unlimited time region.
TFxTimeRegion::TFxTimeRegion()
    : m_start((std::numeric_limits<double>::max)())
    , m_end(-(std::numeric_limits<double>::max)()) {}

//--------------------------------------------------

//! Creates a time region with specified start (included) and end (\b excluded).
TFxTimeRegion::TFxTimeRegion(double start, double end)
    : m_start(start), m_end(end) {}

//--------------------------------------------------

TFxTimeRegion TFxTimeRegion::createUnlimited() {
  return TFxTimeRegion(-(std::numeric_limits<double>::max)(),
                       (std::numeric_limits<double>::max)());
}

//--------------------------------------------------

bool TFxTimeRegion::contains(double time) const {
  return (m_start <= time && time < m_end);
}

//--------------------------------------------------

bool TFxTimeRegion::isUnlimited() const {
  return (m_start == -(std::numeric_limits<double>::max)() ||
          m_end == (std::numeric_limits<double>::max)());
}

//--------------------------------------------------

bool TFxTimeRegion::isEmpty() const { return (m_end <= m_start); }

//--------------------------------------------------

bool TFxTimeRegion::getFrameCount(int &count) const {
  if (isUnlimited()) return false;
  count = tfloor(m_end) - tceil(m_start);
  return true;
}

//--------------------------------------------------

int TFxTimeRegion::getFirstFrame() const { return tceil(m_start); }

//--------------------------------------------------

int TFxTimeRegion::getLastFrame() const {
  if (m_end == (std::numeric_limits<double>::max)())
    return (std::numeric_limits<int>::max)();
  else
    return tceil(m_end) - 1;
}

//===================================================
