

#include "tmacrofx.h"

// TnzBase includes
#include "tparamcontainer.h"
#include "tfxattributes.h"

// TnzCore includes
#include "tstream.h"

//--------------------------------------------------

namespace {

class MatchesFx {
public:
  MatchesFx(const TFxP &fx) : m_fx(fx) {}

  bool operator()(const TFxP &fx) {
    return m_fx.getPointer() == fx.getPointer();
  }

  TFxP m_fx;
};

//--------------------------------------------------

void pushParents(const TFxP &root, std::vector<TFxP> &fxs,
                 const std::vector<TFxP> &selectedFxs) {
  int i, count = root->getInputPortCount();
  if (count == 0) {
    std::vector<TFxP>::const_iterator found =
        std::find_if(fxs.begin(), fxs.end(), MatchesFx(root));
    if (found == fxs.end()) fxs.push_back(root);
    return;
  }
  for (i = 0; i < count; i++) {
    TFxP inutFx = root->getInputPort(i)->getFx();
    std::vector<TFxP>::const_iterator found =
        std::find_if(selectedFxs.begin(), selectedFxs.end(), MatchesFx(inutFx));
    if (found != selectedFxs.end()) pushParents(inutFx, fxs, selectedFxs);
  }
  std::vector<TFxP>::const_iterator found =
      std::find_if(fxs.begin(), fxs.end(), MatchesFx(root));
  if (found == fxs.end()) fxs.push_back(root);
}

//--------------------------------------------------

std::vector<TFxP> sortFxs(const std::vector<TFxP> &fxs) {
  std::vector<TFxP> app;
  std::vector<TFxP> roots;
  // find fxs that could be in back of the vector.
  int i;
  for (i = 0; i < (int)fxs.size(); i++) {
    TFxP fx = fxs[i];
    int j, count = (int)fx->getOutputConnectionCount();
    if (count == 0) {
      roots.push_back(fx);
      continue;
    }
    for (j = 0; j < count; j++) {
      TFxP connectedFx = fx->getOutputConnection(j)->getOwnerFx();
      std::vector<TFxP>::const_iterator found =
          std::find_if(fxs.begin(), fxs.end(), MatchesFx(connectedFx));
      if (found == fxs.end()) {
        roots.push_back(fx);
        break;
      }
    }
  }
  for (i = 0; i < (int)roots.size(); i++) pushParents(roots[i], app, fxs);
  assert(fxs.size() == app.size());
  return app;
}

//--------------------------------------------------

// raccoglie tutti i parametri dai vari TFx e li assegna anche alla macro
void collectParams(TMacroFx *macroFx) {
  int k;
  for (k = 0; k < (int)macroFx->m_fxs.size(); k++) {
    TFxP fx = macroFx->m_fxs[k];
    int j;
    for (j = 0; j < fx->getParams()->getParamCount(); j++)
      macroFx->getParams()->add(fx->getParams()->getParamVar(j)->clone());
  }
}

}  // anonymous namespace

//--------------------------------------------------

bool TMacroFx::analyze(const std::vector<TFxP> &fxs, TFxP &root,
                       std::vector<TFxP> &roots, std::vector<TFxP> &leafs) {
  if (fxs.size() == 1)
    return false;
  else {
    leafs.clear();
    roots.clear();
    std::vector<TFxP>::const_iterator it = fxs.begin();
    for (; it != fxs.end(); ++it) {
      TFxP fx                      = *it;
      int inputInternalConnection  = 0;
      int inputExternalConnection  = 0;
      int outputInternalConnection = 0;
      int outputExternalConnection = 0;

      int i;

      // calcola se ci sono connessioni in input dall'esterno
      // verso l'interno e/o internamente a orderedFxs
      int inputPortCount = fx->getInputPortCount();
      for (i = 0; i < inputPortCount; ++i) {
        TFxPort *inputPort = fx->getInputPort(i);
        TFx *inputPortFx   = inputPort->getFx();
        if (inputPortFx) {
          if (std::find_if(fxs.begin(), fxs.end(), MatchesFx(inputPortFx)) !=
              fxs.end())
            ++inputInternalConnection;
          else
            ++inputExternalConnection;
        }
      }

      // calcola se ci sono connessioni in output dall'interno
      // verso l'esterno e/o internamente a orderedFxs
      int outputPortCount = fx->getOutputConnectionCount();
      for (i = 0; i < outputPortCount; ++i) {
        TFxPort *outputPort = fx->getOutputConnection(i);
        TFx *outputFx       = outputPort->getOwnerFx();
        if (outputFx) {
          if (std::find_if(fxs.begin(), fxs.end(), MatchesFx(outputFx)) !=
              fxs.end())
            ++outputInternalConnection;
          else
            ++outputExternalConnection;
        }
      }

      // se fx e' una radice
      if ((outputExternalConnection > 0) ||
          (outputExternalConnection == 0 && outputInternalConnection == 0)) {
        root = fx;
        roots.push_back(fx);
      }

      // se fx e' una foglia
      if (inputExternalConnection > 0 || fx->getInputPortCount() == 0 ||
          (inputExternalConnection == 0 &&
           inputInternalConnection < fx->getInputPortCount())) {
        leafs.push_back(fx);
      }
    }

    if (roots.size() != 1)
      return false;
    else {
      if (leafs.size() == 0) return false;
    }

    return true;
  }
}

//--------------------------------------------------

bool TMacroFx::analyze(const std::vector<TFxP> &fxs) {
  TFxP root = 0;
  std::vector<TFxP> leafs;
  std::vector<TFxP> roots;
  return analyze(fxs, root, roots, leafs);
}

//--------------------------------------------------

bool TMacroFx::isaLeaf(TFx *fx) const {
  int count = fx->getInputPortCount();
  if (count == 0) return true;

  for (int i = 0; i < count; ++i) {
    TFxPort *port = fx->getInputPort(i);
    TFx *inputFx  = port->getFx();
    if (inputFx) {
      if (std::find_if(m_fxs.begin(), m_fxs.end(), MatchesFx(inputFx)) ==
          m_fxs.end()) {
        // il nodo di input non appartiene al macroFx
        return true;
      }
    } else {
      // la porta di input non e' connessa
      return true;
    }
  }

  // tutte le porte di input sono connesse verso nodi appartenenti al macroFx
  return false;
}

//--------------------------------------------------

TMacroFx::TMacroFx() : m_isEditing(false) { enableComputeInFloat(true); }

//--------------------------------------------------

TMacroFx::~TMacroFx() {}

//--------------------------------------------------

TFx *TMacroFx::clone(bool recursive) const {
  int n = m_fxs.size();
  std::vector<TFxP> clones(n);
  std::map<TFx *, int> table;
  std::map<TFx *, int>::iterator it;
  int i, rootIndex = -1;
  // nodi
  for (i = 0; i < n; ++i) {
    TFx *fx = m_fxs[i].getPointer();
    assert(fx);
    clones[i] = fx->clone(false);
    assert(table.count(fx) == 0);
    table[fx] = i;
    if (fx == m_root.getPointer()) rootIndex = i;
    TFx *linkedFx = fx->getLinkedFx();
    if (linkedFx && table.find(linkedFx) != table.end())
      clones[i]->linkParams(clones[table[linkedFx]].getPointer());
  }
  assert(rootIndex >= 0);
  // connessioni
  for (i = 0; i < n; i++) {
    TFx *fx = m_fxs[i].getPointer();
    for (int j = 0; j < fx->getInputPortCount(); j++) {
      TFxPort *port = fx->getInputPort(j);
      TFx *inputFx  = port->getFx();
      if (!inputFx) continue;
      it = table.find(inputFx);
      if (it == table.end()) {
        // il j-esimo input di fx e' esterno alla macro
        if (recursive)
          clones[i]->connect(fx->getInputPortName(j), inputFx->clone(true));
      } else {
        // il j-esimo input di fx e' interno alla macro
        clones[i]->connect(fx->getInputPortName(j),
                           clones[it->second].getPointer());
      }
    }
  }

  // TFx *rootClone =
  //  const_cast<TMacroFx*>(this)->
  //  clone(m_root.getPointer(), recursive, visited, clones);

  TMacroFx *clone = TMacroFx::create(clones);
  clone->setName(getName());
  clone->setFxId(getFxId());

  // Copy the index of the passive cache manager.
  clone->getAttributes()->passiveCacheDataIdx() =
      getAttributes()->passiveCacheDataIdx();

  assert(clone->getRoot() == clones[rootIndex].getPointer());

  return clone;
}

//--------------------------------------------------

bool TMacroFx::doGetBBox(double frame, TRectD &bBox,
                         const TRenderSettings &info) {
  return m_root->doGetBBox(frame, bBox, info);
}

//--------------------------------------------------

void TMacroFx::doDryCompute(TRectD &rect, double frame,
                            const TRenderSettings &info) {
  assert(m_root);
  m_root->dryCompute(rect, frame, info);
}

//--------------------------------------------------

void TMacroFx::doCompute(TTile &tile, double frame, const TRenderSettings &ri) {
  assert(m_root);
  m_root->compute(tile, frame, ri);
}

//--------------------------------------------------

TFxTimeRegion TMacroFx::getTimeRegion() const {
  return m_root->getTimeRegion();
}

//--------------------------------------------------

std::string TMacroFx::getPluginId() const { return "Base"; }

//--------------------------------------------------

void TMacroFx::setRoot(TFx *root) {
  m_root = root;
  // TFx::m_imp->m_outputPort = root->m_imp->m_outputPort;
}

//--------------------------------------------------

TFx *TMacroFx::getRoot() const { return m_root.getPointer(); }

//--------------------------------------------------

TFx *TMacroFx::getFxById(const std::wstring &id) const {
  int i;
  for (i = 0; i < (int)m_fxs.size(); i++) {
    TFx *fx = m_fxs[i].getPointer();
    if (fx->getFxId() == id) return fx;
  }
  return 0;
}

//--------------------------------------------------

const std::vector<TFxP> &TMacroFx::getFxs() const { return m_fxs; }

//--------------------------------------------------

std::string TMacroFx::getMacroFxType() const {
  std::string name = getFxType() + "(";
  for (int i = 0; i < (int)m_fxs.size(); i++) {
    if (i > 0) name += ",";
    if (TMacroFx *childMacro = dynamic_cast<TMacroFx *>(m_fxs[i].getPointer()))
      name += childMacro->getMacroFxType();
    else
      name += m_fxs[i]->getFxType();
  }
  return name + ")";
}

//--------------------------------------------------

TMacroFx *TMacroFx::create(const std::vector<TFxP> &fxs) {
  std::vector<TFxP> leafs;
  std::vector<TFxP> roots;
  TFxP root = 0;

  std::vector<TFxP> orederedFxs = sortFxs(fxs);

  // verifica che gli effetti selezionati siano idonei ad essere raccolti
  // in una macro. Ci deve essere un solo nodo terminale
  // (roots.size()==1, roots[0] == root) e uno o piu' nodi di ingresso
  // (assert leafs.size()>0)
  if (!analyze(orederedFxs, root, roots, leafs)) return 0;

  // -----------------------------

  TMacroFx *macroFx = new TMacroFx;

  // tutti i nodi vengono spostati (e non copiati) nella macro stessa
  std::vector<TFxP>::const_iterator it = orederedFxs.begin();
  for (; it != orederedFxs.end(); ++it) macroFx->m_fxs.push_back(*it);

  // i nodi di ingresso vengono messi in collegamento con le
  // porte di ingresso della macro
  for (int i = 0; i < (int)leafs.size(); i++) {
    TFxP fx   = leafs[i];
    int k     = 0;
    int count = fx->getInputPortCount();
    for (; k < count; k++) {
      TFxPort *port        = fx->getInputPort(k);
      std::string portName = fx->getInputPortName(k);
      std::string fxId     = ::to_string(fx->getFxId());
      portName +=
          "_" + std::to_string(macroFx->getInputPortCount()) + "_" + fxId;
      TFx *portFx = port->getFx();
      if (portFx) {
        // se la porta k-esima del nodo di ingresso i-esimo e' collegata
        // ad un effetto, la porta viene inserita solo se l'effetto non fa
        // gia' parte della macro
        if (std::find_if(orederedFxs.begin(), orederedFxs.end(),
                         MatchesFx(portFx)) == orederedFxs.end())
          macroFx->addInputPort(portName, *port);
      } else
        macroFx->addInputPort(portName, *port);
    }
  }

  // le porte di uscita di root diventano le porte di uscita della macro
  int count = root->getOutputConnectionCount();
  int k     = count - 1;
  for (; k >= 0; --k) {
    TFxPort *port = root->getOutputConnection(k);
    port->setFx(macroFx);
  }

  macroFx->setRoot(root.getPointer());

  // tutti i parametri delle funzioni figlie diventano parametri della macro
  collectParams(macroFx);
  return macroFx;
}

//--------------------------------------------------

bool TMacroFx::canHandle(const TRenderSettings &info, double frame) {
  return m_root->canHandle(info, frame);
}

//--------------------------------------------------

std::string TMacroFx::getAlias(double frame,
                               const TRenderSettings &info) const {
  std::string alias = getFxType();
  alias += "[";

  // alias degli effetti connessi alle porte di input separati da virgole
  // una porta non connessa da luogo a un alias vuoto (stringa vuota)
  int i;
  for (i = 0; i < getInputPortCount(); i++) {
    TFxPort *port = getInputPort(i);
    if (port->isConnected()) {
      TRasterFxP ifx = port->getFx();
      assert(ifx);
      alias += ifx->getAlias(frame, info);
    }
    alias += ",";
  }

  // alias dei valori dei parametri dell'effetto al frame dato
  for (int j = 0; j < (int)m_fxs.size(); j++) {
    alias += (j == 0) ? "(" : ",(";
    for (i = 0; i < m_fxs[j]->getParams()->getParamCount(); i++) {
      if (i > 0) alias += ",";
      TParam *param = m_fxs[j]->getParams()->getParam(i);
      alias += param->getName() + "=" + param->getValueAlias(frame, 2);
    }
    alias += ")";
  }

  alias += "]";
  return alias;
}

//--------------------------------------------------

void TMacroFx::compatibilityTranslatePort(int major, int minor,
                                          std::string &portName) {
  // Reroute translation to the actual fx associated to the port
  const std::string &fxId =
      portName.substr(portName.find_last_of('_') + 1, std::string::npos);

  if (TFx *fx = getFxById(::to_wstring(fxId))) {
    size_t opnEnd = portName.find_first_of('_');

    std::string originalPortName = portName.substr(0, opnEnd);
    fx->compatibilityTranslatePort(major, minor, originalPortName);

    portName.replace(0, opnEnd, originalPortName);
  }

  // Seems that at a certain point, the port name got extended...
  if (VersionNumber(major, minor) == VersionNumber(1, 16)) {
    for (int i = 0; i < getInputPortCount(); ++i) {
      const std::string &name = getInputPortName(i);
      if (name.find(portName) != std::string::npos) {
        portName = name;
        break;
      }
    }
  }
}

//--------------------------------------------------

void TMacroFx::loadData(TIStream &is) {
  VersionNumber tnzVersion = is.getVersion();
  std::string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "root") {
      // set the flag here in order to prevent the leaf macro fx in the tree
      // to try to link this fx before finish loading
      m_isLoading = true;
      TPersist *p = 0;
      is >> p;
      m_root = dynamic_cast<TFx *>(p);
      // release the flag
      m_isLoading = false;
    } else if (tagName == "nodes") {
      while (!is.eos()) {
        TPersist *p = 0;
        is >> p;

        // NOTE: In current implementation p is sharedly owned by is - it's
        // automatically
        //       released upon stream destruction if the below assignment fails

        if (TFx *fx = dynamic_cast<TFx *>(p)) {
          m_fxs.push_back(fx);
        }
      }
      // collecting params just after loading nodes since they may need on
      // loading "super" tag in case it is linked with another macro fx
      collectParams(this);
      // link parameters if there is a waiting fx for linking with this
      if (m_waitingLinkFx) {
        m_waitingLinkFx->linkParams(this);
        m_waitingLinkFx = nullptr;
      }
    } else if (tagName == "ports") {
      int i = 0;
      while (is.matchTag(tagName)) {
        if (tagName == "port") {
          std::string name = is.getTagAttribute("name");
          if (tnzVersion < VersionNumber(1, 16) && name != "") {
            TRasterFxPort *port = new TRasterFxPort();
            addInputPort(name, *port);
          } else {
            name = is.getTagAttribute("name_inFx");
            if (tnzVersion < VersionNumber(1, 17) &&
                tnzVersion != VersionNumber(0, 0))
              name.insert(name.find("_"), "_" + std::to_string(i));

            compatibilityTranslatePort(tnzVersion.first, tnzVersion.second,
                                       name);

            std::string inPortName = name;
            inPortName.erase(inPortName.find("_"), inPortName.size() - 1);

            std::string inFxId = name;
            inFxId.erase(0, inFxId.find_last_of("_") + 1);

            for (int i = 0; i < (int)m_fxs.size(); i++) {
              std::wstring fxId = m_fxs[i]->getFxId();
              if (fxId == ::to_wstring(inFxId)) {
                if (TFxPort *port = m_fxs[i]->getInputPort(inPortName))
                  addInputPort(name, *port);
              }
            }
          }
          i++;
        } else
          throw TException("unexpected tag " + tagName);
      }
    } else if (tagName == "super") {
      TRasterFx::loadData(is);
    } else
      throw TException("unexpected tag " + tagName);
    is.closeChild();
  }
}

//--------------------------------------------------

void TMacroFx::saveData(TOStream &os) {
  int i;
  os.openChild("root");
  TPersist *p = m_root.getPointer();
  os << p;
  os.closeChild();
  os.openChild("nodes");
  for (i = 0; i < (int)m_fxs.size(); i++) {
    TFxP fx     = m_fxs[i];
    TPersist *p = fx.getPointer();
    os << p;
  }
  os.closeChild();
  os.openChild("ports");
  for (i = 0; i < getInputPortCount(); i++) {
    std::string portName = getInputPortName(i);
    std::map<std::string, std::string> attr;
    attr["name_inFx"] = portName;
    os.openCloseChild("port", attr);
  }
  os.closeChild();
  os.openChild("super");
  TRasterFx::saveData(os);
  os.closeChild();
}

//--------------------------------------------------

void TMacroFx::linkParams(TFx *src) {
  // in case the src fx is not yet loaded
  // (i.e. we are in loading the src fx tree),
  // wait linking the parameters until loading src is completed
  TMacroFx *srcMacroFx = dynamic_cast<TMacroFx *>(src);
  if (srcMacroFx && srcMacroFx->isLoading()) {
    srcMacroFx->setWaitingLinkFx(this);
    return;
  }

  TFx::linkParams(src);
}

//--------------------------------------------------
FX_IDENTIFIER(TMacroFx, "macroFx")
// FX_IDENTIFIER_IS_HIDDEN(TMacroFx, "macroFx")
