#include "tbasefx.h"
#include "tdoubleparam.h"
#include "tfxparam.h"
#include "tmacrofx.h"
#include "tspectrumparam.h"

#include <QCoreApplication>

#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <locale>
#include <set>
#include <stdexcept>
#include <string>

namespace {

class SpectrumAliasFx final : public TBaseRasterFx {
  FX_DECLARATION(SpectrumAliasFx)

public:
  TSpectrumParamP spectrum{new TSpectrumParam};
  TDoubleParamP amount{0.123456};
  bool precise = true;
  TRasterFxPort source;

  SpectrumAliasFx() {
    bindParam(this, "ramp", spectrum);
    bindParam(this, "amount", amount);
    addInputPort("source", source);
    spectrum->getColor(0)->getRed()->setDefaultValue(0.25);
    spectrum->getColor(0)->getGreen()->setDefaultValue(0.12345678901234566);
    spectrum->getColor(0)->getBlue()->setDefaultValue(1.0);
    spectrum->getColor(0)->getMatte()->setDefaultValue(0.5);
  }

  bool requiresFullPrecisionSerialization() const override { return precise; }
  bool canHandle(const TRenderSettings &, double) override { return true; }
  bool doGetBBox(double, TRectD &, const TRenderSettings &) override {
    return false;
  }
  void doCompute(TTile &, double, const TRenderSettings &) override {}
};

FX_IDENTIFIER(SpectrumAliasFx, "spectrumAliasTest")

void require(bool condition, const std::string &message) {
  if (!condition) throw std::runtime_error(message);
}

std::string alias(const TRasterFx &fx, double frame = 0.0) {
  return fx.getAlias(frame, TRenderSettings());
}

std::string legacyAlias(const SpectrumAliasFx &fx) {
  return fx.getFxType() + "[,ramp=" + fx.spectrum->getValueAlias(0.0, 3) +
         "amount=" + fx.amount->getValueAlias(0.0, 3) + "]";
}

void testChannelSeparation() {
  SpectrumAliasFx fx;
  const TPixelParamP color                    = fx.spectrum->getColor(0);
  const std::array<TDoubleParamP, 4> channels = {
      color->getRed(), color->getGreen(), color->getBlue(), color->getMatte()};
  for (const TDoubleParamP &channel : channels) {
    const std::string before = alias(fx);
    const double value       = channel->getDefaultValue();
    channel->setDefaultValue(std::nextafter(value, 0.0));
    require(alias(fx) != before,
            "Adjacent double channel values reused the cache alias");
    channel->setDefaultValue(value);
    require(alias(fx) == before,
            "Restoring a channel failed to restore the cache alias");
  }
}

void testPositionSeparation() {
  SpectrumAliasFx fx;
  const TDoubleParamP position = fx.spectrum->getPosition(0);
  position->setDefaultValue(0.12345678901234566);
  const std::string before = alias(fx);
  position->setDefaultValue(std::nextafter(position->getDefaultValue(), 1.0));
  require(alias(fx) != before,
          "Adjacent double stop positions reused the cache alias");
}

void testHdrAndFiniteExtremes() {
  SpectrumAliasFx fx;
  std::set<std::string> aliases;
  const double values[] = {1.0,
                           2.5,
                           -0.25,
                           1e100,
                           -1e100,
                           std::numeric_limits<double>::max(),
                           std::numeric_limits<double>::lowest(),
                           std::numeric_limits<double>::min(),
                           std::numeric_limits<double>::denorm_min(),
                           0.0};
  for (double value : values) {
    fx.spectrum->getColor(0)->getRed()->setDefaultValue(value);
    require(aliases.insert(alias(fx)).second,
            "Distinct finite HDR values reused the cache alias");
  }
}

void testAnimation() {
  SpectrumAliasFx fx;
  const TDoubleParamP red = fx.spectrum->getColor(0)->getRed();
  red->setValue(0.0, 0.1);
  red->setValue(10.0, 0.10001);
  require(alias(fx, 0.0) != alias(fx, 10.0),
          "Different keyed colors shared an alias");
  require(alias(fx, 1.0) != alias(fx, 2.0),
          "Different interpolated colors shared an alias");
}

void testHugeConversion() {
  SpectrumAliasFx fx;
  fx.spectrum->getColor(0)->getRed()->setDefaultValue(1e100);
  require(alias(fx).find("1e+100") != std::string::npos,
          "Huge finite channel was converted through an integer color");
}

void testLegacyAndOtherParameters() {
  SpectrumAliasFx fx;
  fx.precise = false;
  require(alias(fx) == legacyAlias(fx), "Ordinary FX alias changed");
  const std::string before = alias(fx);
  fx.spectrum->getColor(0)->getGreen()->setDefaultValue(0.12345678901234568);
  require(alias(fx) == before,
          "Ordinary FX no longer uses its legacy color alias");
  fx.precise                      = true;
  const std::string preciseBefore = alias(fx);
  fx.amount->setDefaultValue(0.123457);
  require(alias(fx) == preciseBefore, "Non-spectrum parameter alias changed");
}

class CommaDecimal final : public std::numpunct<char> {
  char do_decimal_point() const override { return ','; }
};

void testLocale() {
  SpectrumAliasFx fx;
  const std::string before = alias(fx);
  const std::locale original;
  std::locale::global(std::locale(std::locale::classic(), new CommaDecimal));
  const std::string localized = alias(fx);
  std::locale::global(original);
  require(localized.substr(0, localized.find("amount=")) ==
              before.substr(0, before.find("amount=")),
          "Spectrum alias depends on the process decimal separator");
}

void testConnectedOrdinaryFx() {
  SpectrumAliasFx fx;
  auto *input = new SpectrumAliasFx;
  TRasterFxP owner(input);
  input->precise = false;
  require(fx.connect("source", input), "Cannot connect ordinary input fixture");
  const std::string inputAlias = alias(*input);
  require(inputAlias == legacyAlias(*input),
          "Connected ordinary FX changed its own alias");
  require(alias(fx).find("[" + inputAlias + ",ramp=") != std::string::npos,
          "Precise FX altered its ordinary input's cache alias");
}

TRasterFxP makeMacro(const TFxP &leaf) {
  auto *root = new SpectrumAliasFx;
  TFxP owner(root);
  root->precise = false;
  root->setFxId(L"root");
  require(root->connect("source", leaf.getPointer()),
          "Cannot connect macro fixture");
  TMacroFx *macro = TMacroFx::create({leaf, owner});
  require(macro != nullptr, "Cannot create macro fixture");
  return macro;
}

void testMacro(bool nested, bool huge) {
  auto *leaf = new SpectrumAliasFx;
  TFxP owner(leaf);
  leaf->setFxId(L"leaf");
  TRasterFxP macro = makeMacro(owner);
  if (nested) macro = makeMacro(TFxP(macro.getPointer()));
  const std::string before  = alias(*macro);
  const TDoubleParamP green = leaf->spectrum->getColor(0)->getGreen();
  green->setDefaultValue(huge ? 1e100
                              : std::nextafter(green->getDefaultValue(), 1.0));
  require(alias(*macro) != before,
          "Macro ignored its precise member's channel edit");
}

void testLegacyMacro(bool nested) {
  auto *leaf = new SpectrumAliasFx;
  TFxP owner(leaf);
  leaf->precise = false;
  leaf->setFxId(L"leaf");
  TRasterFxP macro = makeMacro(owner);
  if (nested) macro = makeMacro(TFxP(macro.getPointer()));
  const std::string before = alias(*macro);
  const std::string member =
      "ramp=((0.00,(64,31,255,128)),(1.00,(255,255,255,255))),amount=0.12";
  const std::string expected = "macroFx[,(" + member +
                               (nested ? "," + member : "") + "),(" + member +
                               ")]";
  require(before == expected,
          "Ordinary macro alias differs from the upstream baseline");
  const TDoubleParamP green = leaf->spectrum->getColor(0)->getGreen();
  green->setDefaultValue(std::nextafter(green->getDefaultValue(), 1.0));
  require(alias(*macro) == before,
          "Ordinary macro no longer uses its legacy color alias");
}

}  // namespace

int main(int argc, char **argv) {
  QCoreApplication application(argc, argv);
  const std::pair<const char *, std::function<void()>> tests[] = {
      {"channels", testChannelSeparation},
      {"positions", testPositionSeparation},
      {"hdr", testHdrAndFiniteExtremes},
      {"huge", testHugeConversion},
      {"animation", testAnimation},
      {"legacy", testLegacyAndOtherParameters},
      {"locale", testLocale},
      {"connected", testConnectedOrdinaryFx},
      {"macro", []() { testMacro(false, false); }},
      {"nested_macro", []() { testMacro(true, false); }},
      {"macro_huge", []() { testMacro(true, true); }},
      {"legacy_macro", []() { testLegacyMacro(false); }},
      {"legacy_nested_macro", []() { testLegacyMacro(true); }}};
  int failures = 0;
  int cases    = 0;
  for (const auto &test : tests) {
    if (argc == 2 && std::string(argv[1]) != test.first) continue;
    ++cases;
    try {
      test.second();
      std::cout << "PASS: " << test.first << '\n';
    } catch (const std::exception &error) {
      ++failures;
      std::cerr << "FAIL: " << test.first << ": " << error.what() << '\n';
    }
  }
  if (!cases) std::cerr << "Unknown spectrum alias test\n";
  return failures || !cases ? 1 : 0;
}
