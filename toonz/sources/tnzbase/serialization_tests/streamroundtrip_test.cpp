#include "tfilepath.h"
#include "tstream.h"
#include "texception.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <locale>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

class LocaleScope {
  std::locale m_previous;

public:
  explicit LocaleScope(const std::locale& locale)
      : m_previous(std::locale::global(locale)) {}
  ~LocaleScope() { std::locale::global(m_previous); }
};

class GroupedNumbers final : public std::numpunct<char> {
  char do_decimal_point() const override { return '.'; }
  char do_thousands_sep() const override { return ','; }
  std::string do_grouping() const override { return "\3"; }
};

void writeText(const QString& path, const QByteArray& text) {
  QFile file(path);
  require(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
          "Cannot create parser fixture");
  require(file.write(text) == text.size(), "Cannot write parser fixture");
}

void roundTrip(const QString& path, double value) {
  const TFilePath file(path.toStdWString());
  {
    TOStream output(file);
    require(bool(output), "Cannot open output stream");
    output.setFullPrecision(true);
    output.child("value") << value;
    output.child("next") << 42;
    require(output.checkStatus(), "Cannot flush full-precision output");
  }
  TIStream input(file);
  std::string tag;
  require(input.openChild(tag) && tag == "value", "Missing value tag");
  double actual = value;
  input >> actual;
  require(bool(input), "Reading a finite value failed");
  require(actual == value && std::signbit(actual) == std::signbit(value),
          "Finite value did not round-trip exactly");
  input.closeChild();
  require(input.openChild(tag) && tag == "next", "Cannot read following tag");
  int next = -1;
  input >> next;
  require(bool(input) && next == 42, "Cannot read following numeric value");
  input.closeChild();
}

void failedRead(const QString& path, const QByteArray& text) {
  writeText(path, text);
  TIStream input(TFilePath(path.toStdWString()));
  double actual = std::numeric_limits<double>::denorm_min();
  input >> actual;
  require(!input, "Invalid input was accepted as a numeric value");
}

void priorFailure(const QString& path) {
  writeText(path, "invalid 42");
  TIStream input(TFilePath(path.toStdWString()));
  double actual = 0;
  input >> actual;
  require(!input, "Malformed numeric input did not fail");
  actual = std::numeric_limits<double>::denorm_min();
  input >> actual;
  require(!input, "A later read cleared an existing stream failure");
}

void subnormalAtEof(const QString& path) {
  writeText(path, "4.9406564584124654e-324");
  TIStream input(TFilePath(path.toStdWString()));
  double actual = std::numeric_limits<double>::denorm_min();
  input >> actual;
  require(bool(input) && actual == std::numeric_limits<double>::denorm_min(),
          "A subnormal value at EOF failed");
  input >> actual;
  require(!input, "Reading after EOF cleared the failure state");
}

void priorEof(const QString& path) {
  writeText(path, "1");
  TIStream input(TFilePath(path.toStdWString()));
  double actual = 0;
  input >> actual;
  require(bool(input) && actual == 1, "The valid value at EOF did not load");
  actual = std::numeric_limits<double>::denorm_min();
  input >> actual;
  require(!input, "Reading after a prior EOF cleared the failure state");
}

void invalidGrouping(const QString& path) {
  const LocaleScope locale(
      std::locale(std::locale::classic(), new GroupedNumbers));
  failedRead(path, "1,2e-310 42");
}

void legacyText(const QString& path) {
  const TFilePath file(path.toStdWString());
  {
    TOStream output(file);
    output << 0.12345678901234566;
    output << std::numeric_limits<double>::denorm_min();
    require(output.checkStatus(), "Cannot flush legacy output");
  }
  QFile input(path);
  require(input.open(QIODevice::ReadOnly), "Cannot inspect legacy output");
  require(input.readAll() == "0.123457 0 ", "Legacy output bytes changed");
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
  const LocaleScope locale(std::locale::classic());
  QTemporaryDir temporary;
  require(temporary.isValid(), "Cannot create stream test directory");
  const QString path = temporary.filePath("stream.txt");
  int passed = 0, failed = 0;
  const auto run = [&](const std::string& name,
                       const std::function<void()>& test) {
    try {
      test();
      ++passed;
    } catch (const std::exception& error) {
      std::cerr << name << ": " << error.what() << '\n';
      ++failed;
    } catch (const TException& error) {
      std::cerr << name << ": stream exception\n";
      std::wcerr << error.getMessage() << '\n';
      ++failed;
    }
  };
  const double values[] = {
      0.0,
      -0.0,
      0.12345678901234566,
      -0.9876543210987654,
      std::numeric_limits<double>::min(),
      std::numeric_limits<double>::max(),
      1.4e-310,
      -1.4e-310,
      std::numeric_limits<double>::denorm_min(),
      -std::numeric_limits<double>::denorm_min(),
      std::nextafter(std::numeric_limits<double>::min(), 0.0)};
  for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
    run("finite-" + std::to_string(i), [&] { roundTrip(path, values[i]); });
  const QByteArray invalid[] = {"invalid 42", "1e- 42",     "1e309 42",
                                "-1e309 42",  "1e-9999 42", "-1e-9999 42",
                                "",           " \n\t"};
  for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
    run("invalid-" + std::to_string(i), [&] { failedRead(path, invalid[i]); });
  run("prior-failure", [&] { priorFailure(path); });
  run("subnormal-eof", [&] { subnormalAtEof(path); });
  run("prior-eof", [&] { priorEof(path); });
  run("invalid-grouping", [&] { invalidGrouping(path); });
  run("legacy-bytes", [&] { legacyText(path); });
  std::cout << passed << " stream cases passed; " << failed << " failed\n";
  return failed ? 1 : 0;
}
