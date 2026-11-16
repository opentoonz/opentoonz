#include "tdoubleparam.h"
#include "tfilepath.h"
#include "tfx.h"
#include "tfxattributes.h"
#include "tfxparam.h"
#include "tstream.h"

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>

#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr double sample       = 0.12345678901234566;
constexpr double legacySample = 0.123457;
constexpr double keyValue     = -0.9876543210987654;
constexpr double legacyKey    = -0.987654;

void require(bool condition, const std::string &message) {
  if (!condition) throw std::runtime_error(message);
}

class OrdinaryFx : public TFx {
  FX_DECLARATION(OrdinaryFx)

  TFxPortT<TFx> m_source;
  TFxPortT<TFx> m_other;
  TDoubleParamP m_value;
  bool m_optIn = false;

public:
  OrdinaryFx() : m_value(sample) {
    bindParam(this, "value", m_value);
    addInputPort("source", m_source);
    addInputPort("other", m_other);
    m_value->setValue(10.0, keyValue);
    getAttributes()->setDagNodePos(TPointD(sample, -sample));
  }
  std::string getPluginId() const override { return ""; }
  bool requiresFullPrecisionSerialization() const override { return m_optIn; }
  void setOptIn(bool value) { m_optIn = value; }
  TDoubleParam *value() const { return m_value.getPointer(); }
};

class PreciseFx final : public OrdinaryFx {
  FX_DECLARATION(PreciseFx)
public:
  PreciseFx() { setOptIn(true); }
};

FX_IDENTIFIER(OrdinaryFx, "serializationTestOrdinary")
FX_IDENTIFIER(PreciseFx, "serializationTestPrecise")

TFxP makeFx(bool precise) {
  return precise ? TFxP(new PreciseFx) : TFxP(new OrdinaryFx);
}

void checkValue(TFx *fx, bool full, const std::string &context) {
  auto *testFx = dynamic_cast<OrdinaryFx *>(fx);
  require(testFx != nullptr, context + ": incorrect loaded FX type");
  require(testFx->value()->getDefaultValue() == (full ? sample : legacySample),
          context + ": default parameter precision changed");
  require(testFx->value()->getValue(10.0) == (full ? keyValue : legacyKey),
          context + ": keyframe parameter precision changed");
}

void checkGraph(TFx *original, TFx *loaded, bool ambient) {
  checkValue(loaded, ambient || original->requiresFullPrecisionSerialization(),
             original->getFxType());
  for (int i = 0; i < original->getInputPortCount(); ++i) {
    TFx *input = original->getInputPort(i)->getFx();
    TFx *copy  = loaded->getInputPort(i)->getFx();
    require(bool(input) == bool(copy),
            "Connection changed during serialization");
    if (input) checkGraph(input, copy, ambient);
  }
  const TPointD pos = loaded->getAttributes()->getDagNodePos();
  require(pos == TPointD(ambient ? sample : legacySample,
                         ambient ? -sample : -legacySample),
          original->getFxType() + ": node metadata precision leaked");
}

void checkSuffix(TIStream &is, bool ambient) {
  std::string tag;
  require(is.openChild(tag) && tag == "suffix", "Missing post-FX sentinel");
  double value = 0;
  is >> value;
  require(value == (ambient ? sample : legacySample),
          "Precision leaked into the post-FX sentinel");
  is.closeChild();
}

TFxP readGraph(const TFilePath &path, bool ambient) {
  TIStream is(path);
  require(bool(is), "Cannot open graph serialization");
  is.setVersion(VersionNumber(1, 19));
  TPersist *persist = nullptr;
  is >> persist;
  std::unique_ptr<TPersist> guard(persist);
  TFx *fx = dynamic_cast<TFx *>(persist);
  require(fx != nullptr, "Graph did not load as an FX");
  TFxP result(fx);
  guard.release();
  checkSuffix(is, ambient);
  return result;
}

void writeGraph(const TFilePath &path, TFx *fx, bool ambient) {
  TOStream os(path);
  require(bool(os), "Cannot create graph serialization");
  require(!os.setFullPrecision(ambient), "Stream did not start in legacy mode");
  os << fx;
  require(os.setFullPrecision(ambient) == ambient,
          "Graph serialization did not restore ambient precision");
  os.child("suffix") << sample;
  require(os.checkStatus(), "Graph serialization write failed");
}

void testGraph(const TFilePath &path, const std::vector<bool> &precisions,
               bool ambient, bool siblings) {
  std::vector<TFxP> nodes;
  for (bool precise : precisions) nodes.push_back(makeFx(precise));
  for (size_t i = 1; i < nodes.size(); ++i) {
    TFx *parent = siblings ? nodes[0].getPointer() : nodes[i - 1].getPointer();
    require(parent->connect(siblings && i == 2 ? "other" : "source",
                            nodes[i].getPointer()),
            "Cannot connect fixture graph");
  }
  writeGraph(path, nodes[0].getPointer(), ambient);
  TFxP loaded = readGraph(path, ambient);
  checkGraph(nodes[0].getPointer(), loaded.getPointer(), ambient);
}

void testPreset(const TFilePath &path, bool precise, bool ambient) {
  TFxP fx = makeFx(precise);
  {
    TOStream os(path);
    os.setFullPrecision(ambient);
    fx->savePreset(os);
    require(os.setFullPrecision(ambient) == ambient,
            "Preset serialization did not restore ambient precision");
    require(os.checkStatus(), "Preset serialization write failed");
  }
  TIStream is(path);
  TFxP loaded = makeFx(precise);
  auto *value = dynamic_cast<OrdinaryFx *>(loaded.getPointer())->value();
  value->setDefaultValue(0);
  value->clearKeyframes();
  loaded->loadPreset(is);
  checkValue(loaded.getPointer(), ambient || precise, "preset");
}

void testLinked(const TFilePath &path, bool ambient) {
  TFxP first(new OrdinaryFx), second(new OrdinaryFx);
  TFx *low  = first.getPointer();
  TFx *high = second.getPointer();
  if (std::less<TFx *>()(high, low)) std::swap(low, high);
  dynamic_cast<OrdinaryFx *>(high)->setOptIn(true);
  high->linkParams(low);
  writeGraph(path, high, ambient);
  TFxP loaded = readGraph(path, ambient);
  checkValue(loaded.getPointer(), ambient, "ordinary linked parameter owner");
}

// Failure injection at the parameter writer boundary exercises the real
// TFx scope unwinding, without replacing the stream or serialization logic.
class ThrowingParam final : public TParam {
public:
  TParam *clone() const override { return new ThrowingParam; }
  void copy(TParam *) override {}
  void addObserver(TParamObserver *) override {}
  void removeObserver(TParamObserver *) override {}
  std::string getValueAlias(double, int) override { return ""; }
  bool isAnimatable() const override { return false; }
  bool isKeyframe(double) const override { return false; }
  void deleteKeyframe(double) override {}
  void clearKeyframes() override {}
  void assignKeyframe(double, const TParamP &, double, bool) override {}
  const TPersistDeclaration *getDeclaration() const override { return nullptr; }
  void loadData(TIStream &) override {}
  void saveData(TOStream &os) override {
    os.child("beforeFailure") << sample;
    throw std::runtime_error("injected parameter write failure");
  }
};

void testException(const TFilePath &path, bool preset, bool ambient) {
  PreciseFx fx;
  TParamP fault(new ThrowingParam);
  bindParam(&fx, "failure", fault);
  TOStream os(path);
  os.setFullPrecision(ambient);
  bool caught = false;
  try {
    if (preset)
      fx.savePreset(os);
    else
      fx.saveData(os);
  } catch (const std::runtime_error &error) {
    require(std::string(error.what()) == "injected parameter write failure",
            "Unexpected serialization exception");
    caught = true;
  }
  require(caught, "Fault injection did not throw");
  require(os.setFullPrecision(ambient) == ambient,
          "Exception did not restore ambient precision");
  while (!os.getCurrentTagName().empty()) os.closeChild();
  os.child("suffix") << sample;
  require(os.checkStatus(), "Cannot write after injected failure");
}

}  // namespace

int main(int argc, char *argv[]) {
  QCoreApplication application(argc, argv);
  QTemporaryDir temporary;
  const QString output =
      argc > 1 ? QString::fromLocal8Bit(argv[1]) : temporary.path();
  require(QDir().mkpath(output), "Cannot create test output directory");
  int passed = 0, failed = 0;
  auto run = [&](const std::string &name,
                 const std::function<void(TFilePath)> &test) {
    const TFilePath path(
        (output + "/" + QString::fromStdString(name) + ".fx").toStdWString());
    try {
      test(path);
      ++passed;
    } catch (const std::exception &error) {
      std::cerr << name << ": " << error.what() << '\n';
      ++failed;
    }
  };
  struct GraphCase {
    std::string name;
    std::vector<bool> precisions;
    bool siblings = false;
  };
  const std::vector<GraphCase> graphs = {
      {"ordinary", {false}},
      {"precise", {true}},
      {"ordinary-ordinary", {false, false}},
      {"precise-ordinary", {true, false}},
      {"ordinary-precise", {false, true}},
      {"precise-precise", {true, true}},
      {"precise-ordinary-precise", {true, false, true}},
      {"siblings", {true, false, true}, true}};
  for (bool ambient : {false, true}) {
    const std::string mode = ambient ? "full" : "legacy";
    for (const GraphCase &graph : graphs)
      run(mode + "-" + graph.name, [&](const TFilePath &path) {
        testGraph(path, graph.precisions, ambient, graph.siblings);
      });
    for (bool precise : {false, true})
      run(mode + "-preset-" + std::to_string(precise),
          [&](const TFilePath &path) { testPreset(path, precise, ambient); });
    run(mode + "-linked",
        [&](const TFilePath &path) { testLinked(path, ambient); });
    for (bool preset : {false, true})
      run(mode + "-exception-" + std::to_string(preset),
          [&](const TFilePath &path) { testException(path, preset, ambient); });
  }
  std::cout << passed << " serialization cases passed; " << failed
            << " failed\n";
  return failed ? 1 : 0;
}
