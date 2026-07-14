#include <cstdio>
#include <filesystem>
#include <string>

#include "texternfx.h"
#include "ttile.h"

namespace {

int fail(const char *message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

std::string quote(const std::filesystem::path &path) {
  return "\"" + path.string() + "\"";
}

}  // namespace

int main() {
  const std::filesystem::path marker =
      std::filesystem::temp_directory_path() / "opentoonz_externfx_marker";
  std::filesystem::remove(marker);

  TRaster32P raster(1, 1);
  TTile tile(raster);
  TRenderSettings settings;

  TExternalProgramFx fx;
  fx.addPort("out", "png", false);
  fx.setExecutable(TFilePath("/usr/bin/true"),
                   "; /usr/bin/touch " + quote(marker) + " $out");
  fx.doCompute(tile, 1.0, settings);

  if (std::filesystem::exists(marker)) {
    std::filesystem::remove(marker);
    return fail("external FX arguments were interpreted by a shell");
  }

  return 0;
}
