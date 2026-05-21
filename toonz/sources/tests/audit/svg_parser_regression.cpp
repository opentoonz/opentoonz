#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "image/svg/tiio_svg.cpp"

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

}  // namespace

int main() {
  const std::string longId(4096, 'i');
  const std::string longUnits(512, 'u');
  const std::string longSpace(512, ' ');
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "opentoonz_svg_parser_test.svg";

  std::ofstream out(path);
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"10" << longUnits
      << "\" height=\"20" << longUnits << "\">"
      << "<path id=\"" << longId << "\" fill=\"rgb(1" << longSpace << "2"
      << longSpace << "3)\" d=\"M 0 0 L 1 1\"/>"
      << "</svg>";
  out.close();

  NSVGimage* image = nsvgParseFromFile(path.string().c_str());
  std::filesystem::remove(path);

  if (!image) return fail("expected SVG parser to accept oversized fields");
  if (!image->shapes) {
    nsvgDelete(image);
    return fail("expected parsed shape");
  }
  if (std::strlen(image->shapes->id) >= sizeof(image->shapes->id)) {
    nsvgDelete(image);
    return fail("shape id was not bounded");
  }
  if (std::strlen(image->wunits) >= sizeof(image->wunits) ||
      std::strlen(image->hunits) >= sizeof(image->hunits)) {
    nsvgDelete(image);
    return fail("SVG units were not bounded");
  }

  nsvgDelete(image);
  return 0;
}
