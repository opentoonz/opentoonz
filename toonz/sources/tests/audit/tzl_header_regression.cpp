#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "image/tzl/tiio_tzl.h"

namespace {

int fail(const char *message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

void writeU32(std::ofstream &out, std::uint32_t value) {
  const char bytes[] = {
      static_cast<char>(value & 0xff),
      static_cast<char>((value >> 8) & 0xff),
      static_cast<char>((value >> 16) & 0xff),
      static_cast<char>((value >> 24) & 0xff),
  };
  out.write(bytes, sizeof(bytes));
}

}  // namespace

int main() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "opentoonz_tzl_header_test.tlv";
  const std::uint32_t headerBytes = 8 + 40 + 6 * 4 + 4;
  const std::uint32_t oversizedSuffixLength = 1025;
  const std::uint32_t iconTableOffset =
      headerBytes + 4 + 4 + oversizedSuffixLength + 4 + 4;
  const std::string suffix(oversizedSuffixLength, 'a');

  std::ofstream out(path, std::ios::binary);
  const char magic[8] = {'T', 'L', 'V', '1', '5', '\0', '\0', '\0'};
  const char creator[40] = {};

  out.write(magic, sizeof(magic));
  out.write(creator, sizeof(creator));
  writeU32(out, 0);                // header size
  writeU32(out, 1);                // width
  writeU32(out, 1);                // height
  writeU32(out, 1);                // frame count
  writeU32(out, headerBytes);      // frame offset table
  writeU32(out, iconTableOffset);  // icon offset table
  out.write("LZO ", 4);

  writeU32(out, 1);  // frame number
  writeU32(out, oversizedSuffixLength);
  out.write(suffix.data(), suffix.size());
  writeU32(out, 2048);  // frame data offset
  writeU32(out, 16);    // frame data length

  writeU32(out, 1);     // icon frame number
  writeU32(out, 0);     // icon suffix length
  writeU32(out, 4096);  // icon data offset
  writeU32(out, 16);    // icon data length
  out.close();

  TLevelReaderTzl reader(TFilePath(path.string()));
  TDimension iconSize;
  const bool hasIcon = reader.getIconSize(iconSize);

  std::filesystem::remove(path);

  if (hasIcon)
    return fail("oversized TLV frame suffix was accepted");

  return 0;
}
