#ifndef TINYEXR_OTMOD_H_
#define TINYEXR_OTMOD_H_

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

/*
 * This source is based on TinyEXR code, enabling to use file handle
 * as an argument instead of file path in order to fit usage in OpenToonz.
 * TinyEXR code is licensed under the following:
 */

// Start of TinyEXR license -------------------------------------------------
/*
Copyright (c) 2014 - 2021, Syoyo Fujita and many contributors.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Syoyo Fujita nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
// End of TinyEXR license -------------------------------------------------
// TinyEXR contains some OpenEXR code, which is licensed under ------------

///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2002, Industrial Light & Magic, a division of Lucas
// Digital Ltd. LLC
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Industrial Light & Magic nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

// End of OpenEXR license -------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

extern int ParseEXRVersionFromFileHandle(EXRVersion *version, FILE *fp);

extern int ParseEXRHeaderFromFileHandle(EXRHeader *exr_header,
                                        const EXRVersion *exr_version, FILE *fp,
                                        const char **err);

extern int LoadEXRImageFromFileHandle(EXRImage *exr_image,
                                      const EXRHeader *exr_header, FILE *fp,
                                      const char **err);

extern int LoadEXRHeaderFromFileHandle(EXRHeader &exr_header, FILE *file,
                                       const char **err);

extern int LoadEXRImageBufFromFileHandle(float **out_rgba,
                                         EXRHeader &exr_header, FILE *file,
                                         const char **err);

extern int SaveEXRImageToFileHandle(const EXRImage *exr_image,
                                    const EXRHeader *exr_header, FILE *fp,
                                    const char **err);

#ifdef __cplusplus
}
#endif

#endif  // TINYEXR_OTMOD_H_

#ifdef TINYEXR_OTMOD_IMPLEMENTATION
#ifndef TINYEXR_OTMOD_IMPLEMENTATION_DEFINED
#define TINYEXR_OTMOD_IMPLEMENTATION_DEFINED

int ParseEXRVersionFromFileHandle(EXRVersion *version, FILE *fp) {
  if (!fp) {
    return TINYEXR_ERROR_CANT_OPEN_FILE;
  }

  size_t file_size;
  // Compute size
  fseek(fp, 0, SEEK_END);
  file_size = static_cast<size_t>(ftell(fp));
  fseek(fp, 0, SEEK_SET);

  if (file_size < tinyexr::kEXRVersionSize) {
    return TINYEXR_ERROR_INVALID_FILE;
  }

  unsigned char buf[tinyexr::kEXRVersionSize];
  size_t ret = fread(&buf[0], 1, tinyexr::kEXRVersionSize, fp);
  // fclose(fp);

  if (ret != tinyexr::kEXRVersionSize) {
    return TINYEXR_ERROR_INVALID_FILE;
  }

  return ParseEXRVersionFromMemory(version, buf, tinyexr::kEXRVersionSize);
}

int ParseEXRHeaderFromFileHandle(EXRHeader *exr_header,
                                 const EXRVersion *exr_version, FILE *fp,
                                 const char **err) {
  if (exr_header == NULL || exr_version == NULL) {
    tinyexr::SetErrorMessage("Invalid argument for ParseEXRHeaderFromFile",
                             err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }
  if (!fp) {
    tinyexr::SetErrorMessage("Cannot read file ", err);
    return TINYEXR_ERROR_CANT_OPEN_FILE;
  }

  size_t filesize;
  // Compute size
  fseek(fp, 0, SEEK_END);
  filesize = static_cast<size_t>(ftell(fp));
  fseek(fp, 0, SEEK_SET);

  std::vector<unsigned char> buf(filesize);  // @todo { use mmap }
  {
    size_t ret;
    ret = fread(&buf[0], 1, filesize, fp);
    assert(ret == filesize);
    // fclose(fp);

    if (ret != filesize) {
      tinyexr::SetErrorMessage("fread() error", err);
      return TINYEXR_ERROR_INVALID_FILE;
    }
  }

  return ParseEXRHeaderFromMemory(exr_header, exr_version, &buf.at(0), filesize,
                                  err);
}

int LoadEXRImageFromFileHandle(EXRImage *exr_image, const EXRHeader *exr_header,
                               FILE *fp, const char **err) {
  if (exr_image == NULL) {
    tinyexr::SetErrorMessage("Invalid argument for LoadEXRImageFromFile", err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  if (!fp) {
    tinyexr::SetErrorMessage("Cannot read file", err);
    return TINYEXR_ERROR_CANT_OPEN_FILE;
  }

  size_t filesize;
  // Compute size
  fseek(fp, 0, SEEK_END);
  filesize = static_cast<size_t>(ftell(fp));
  fseek(fp, 0, SEEK_SET);

  if (filesize < 16) {
    tinyexr::SetErrorMessage("File size too short", err);
    return TINYEXR_ERROR_INVALID_FILE;
  }

  std::vector<unsigned char> buf(filesize);  // @todo { use mmap }
  {
    size_t ret;
    ret = fread(&buf[0], 1, filesize, fp);
    assert(ret == filesize);
    // fclose(fp);
    (void)ret;
  }

  return LoadEXRImageFromMemory(exr_image, exr_header, &buf.at(0), filesize,
                                err);
}

int LoadEXRHeaderFromFileHandle(EXRHeader &exr_header, FILE *file,
                                const char **err) {
  EXRVersion exr_version;
  InitEXRHeader(&exr_header);

  {
    FILE *_fp = file;
    int ret   = ParseEXRVersionFromFileHandle(&exr_version, _fp);
    if (ret != TINYEXR_SUCCESS) {
      std::stringstream ss;
      ss << "Failed to open EXR file or read version info from EXR file. code("
         << ret << ")";
      tinyexr::SetErrorMessage(ss.str(), err);
      return ret;
    }

    if (exr_version.multipart || exr_version.non_image) {
      tinyexr::SetErrorMessage(
          "Loading multipart or DeepImage is not supported  in LoadEXR() API",
          err);
      return TINYEXR_ERROR_INVALID_DATA;  // @fixme.
    }
  }

  {
    FILE *_fp = file;
    int ret = ParseEXRHeaderFromFileHandle(&exr_header, &exr_version, _fp, err);
    if (ret != TINYEXR_SUCCESS) {
      FreeEXRHeader(&exr_header);
      return ret;
    }
  }
  return TINYEXR_SUCCESS;
}

int LoadEXRImageBufFromFileHandle(float **out_rgba, EXRHeader &exr_header,
                                  FILE *file, const char **err) {
  if (out_rgba == NULL) {
    tinyexr::SetErrorMessage("Invalid argument for LoadEXR()", err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  EXRImage exr_image;
  InitEXRImage(&exr_image);

  // Read HALF channel as FLOAT.
  for (int i = 0; i < exr_header.num_channels; i++) {
    if (exr_header.pixel_types[i] == TINYEXR_PIXELTYPE_HALF) {
      exr_header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
    }
  }

  // TODO: Probably limit loading to layers (channels) selected by layer index
  {
    FILE *_fp = file;
    int ret   = LoadEXRImageFromFileHandle(&exr_image, &exr_header, _fp, err);
    if (ret != TINYEXR_SUCCESS) {
      FreeEXRHeader(&exr_header);
      return ret;
    }
  }

  // RGBA
  int idxR = -1;
  int idxG = -1;
  int idxB = -1;
  int idxA = -1;

  std::vector<std::string> layer_names;
  tinyexr::GetLayers(exr_header, layer_names);

  std::vector<tinyexr::LayerChannel> channels;
  tinyexr::ChannelsInLayer(exr_header, "", channels);

  if (channels.size() < 1) {
    tinyexr::SetErrorMessage("Layer Not Found", err);
    FreeEXRHeader(&exr_header);
    FreeEXRImage(&exr_image);
    return TINYEXR_ERROR_LAYER_NOT_FOUND;
  }

  size_t ch_count = channels.size() < 4 ? channels.size() : 4;
  for (size_t c = 0; c < ch_count; c++) {
    const tinyexr::LayerChannel &ch = channels[c];

    if (ch.name == "R") {
      idxR = int(ch.index);
    } else if (ch.name == "G") {
      idxG = int(ch.index);
    } else if (ch.name == "B") {
      idxB = int(ch.index);
    } else if (ch.name == "A") {
      idxA = int(ch.index);
    }
  }

  if (channels.size() == 1) {
    int chIdx = int(channels.front().index);
    // Grayscale channel only.

    (*out_rgba) = reinterpret_cast<float *>(
        malloc(4 * sizeof(float) * static_cast<size_t>(exr_image.width) *
               static_cast<size_t>(exr_image.height)));

    if (exr_header.tiled) {
      for (int it = 0; it < exr_image.num_tiles; it++) {
        for (int j = 0; j < exr_header.tile_size_y; j++) {
          for (int i = 0; i < exr_header.tile_size_x; i++) {
            const int ii = exr_image.tiles[it].offset_x *
                               static_cast<int>(exr_header.tile_size_x) +
                           i;
            const int jj = exr_image.tiles[it].offset_y *
                               static_cast<int>(exr_header.tile_size_y) +
                           j;
            const int idx = ii + jj * static_cast<int>(exr_image.width);

            // out of region check.
            if (ii >= exr_image.width) {
              continue;
            }
            if (jj >= exr_image.height) {
              continue;
            }
            const int srcIdx    = i + j * exr_header.tile_size_x;
            unsigned char **src = exr_image.tiles[it].images;
            (*out_rgba)[4 * idx + 0] =
                reinterpret_cast<float **>(src)[chIdx][srcIdx];
            (*out_rgba)[4 * idx + 1] =
                reinterpret_cast<float **>(src)[chIdx][srcIdx];
            (*out_rgba)[4 * idx + 2] =
                reinterpret_cast<float **>(src)[chIdx][srcIdx];
            (*out_rgba)[4 * idx + 3] =
                reinterpret_cast<float **>(src)[chIdx][srcIdx];
          }
        }
      }
    } else {
      for (int i = 0; i < exr_image.width * exr_image.height; i++) {
        const float val =
            reinterpret_cast<float **>(exr_image.images)[chIdx][i];
        (*out_rgba)[4 * i + 0] = val;
        (*out_rgba)[4 * i + 1] = val;
        (*out_rgba)[4 * i + 2] = val;
        (*out_rgba)[4 * i + 3] = val;
      }
    }
  } else {
    // Assume RGB(A)

    if (idxR == -1) {
      tinyexr::SetErrorMessage("R channel not found", err);

      FreeEXRHeader(&exr_header);
      FreeEXRImage(&exr_image);
      return TINYEXR_ERROR_INVALID_DATA;
    }

    if (idxG == -1) {
      tinyexr::SetErrorMessage("G channel not found", err);
      FreeEXRHeader(&exr_header);
      FreeEXRImage(&exr_image);
      return TINYEXR_ERROR_INVALID_DATA;
    }

    if (idxB == -1) {
      tinyexr::SetErrorMessage("B channel not found", err);
      FreeEXRHeader(&exr_header);
      FreeEXRImage(&exr_image);
      return TINYEXR_ERROR_INVALID_DATA;
    }

    (*out_rgba) = reinterpret_cast<float *>(
        malloc(4 * sizeof(float) * static_cast<size_t>(exr_image.width) *
               static_cast<size_t>(exr_image.height)));
    if (exr_header.tiled) {
      for (int it = 0; it < exr_image.num_tiles; it++) {
        for (int j = 0; j < exr_header.tile_size_y; j++) {
          for (int i = 0; i < exr_header.tile_size_x; i++) {
            const int ii =
                exr_image.tiles[it].offset_x * exr_header.tile_size_x + i;
            const int jj =
                exr_image.tiles[it].offset_y * exr_header.tile_size_y + j;
            const int idx = ii + jj * exr_image.width;

            // out of region check.
            if (ii >= exr_image.width) {
              continue;
            }
            if (jj >= exr_image.height) {
              continue;
            }
            const int srcIdx    = i + j * exr_header.tile_size_x;
            unsigned char **src = exr_image.tiles[it].images;
            (*out_rgba)[4 * idx + 0] =
                reinterpret_cast<float **>(src)[idxR][srcIdx];
            (*out_rgba)[4 * idx + 1] =
                reinterpret_cast<float **>(src)[idxG][srcIdx];
            (*out_rgba)[4 * idx + 2] =
                reinterpret_cast<float **>(src)[idxB][srcIdx];
            if (idxA != -1) {
              (*out_rgba)[4 * idx + 3] =
                  reinterpret_cast<float **>(src)[idxA][srcIdx];
            } else {
              (*out_rgba)[4 * idx + 3] = 1.0;
            }
          }
        }
      }
    } else {
      for (int i = 0; i < exr_image.width * exr_image.height; i++) {
        (*out_rgba)[4 * i + 0] =
            reinterpret_cast<float **>(exr_image.images)[idxR][i];
        (*out_rgba)[4 * i + 1] =
            reinterpret_cast<float **>(exr_image.images)[idxG][i];
        (*out_rgba)[4 * i + 2] =
            reinterpret_cast<float **>(exr_image.images)[idxB][i];
        if (idxA != -1) {
          (*out_rgba)[4 * i + 3] =
              reinterpret_cast<float **>(exr_image.images)[idxA][i];
        } else {
          (*out_rgba)[4 * i + 3] = 1.0;
        }
      }
    }
  }

  FreeEXRHeader(&exr_header);
  FreeEXRImage(&exr_image);

  return TINYEXR_SUCCESS;
}

int SaveEXRImageToFileHandle(const EXRImage *exr_image,
                             const EXRHeader *exr_header, FILE *fp,
                             const char **err) {
  if (exr_image == NULL || exr_header->compression_type < 0) {
    tinyexr::SetErrorMessage("Invalid argument for SaveEXRImageToFile", err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

#if !TINYEXR_USE_PIZ
  if (exr_header->compression_type == TINYEXR_COMPRESSIONTYPE_PIZ) {
    tinyexr::SetErrorMessage("PIZ compression is not supported in this build",
                             err);
    return TINYEXR_ERROR_UNSUPPORTED_FEATURE;
  }
#endif

#if !TINYEXR_USE_ZFP
  if (exr_header->compression_type == TINYEXR_COMPRESSIONTYPE_ZFP) {
    tinyexr::SetErrorMessage("ZFP compression is not supported in this build",
                             err);
    return TINYEXR_ERROR_UNSUPPORTED_FEATURE;
  }
#endif

  if (!fp) {
    tinyexr::SetErrorMessage("Cannot write a file", err);
    return TINYEXR_ERROR_CANT_WRITE_FILE;
  }

  unsigned char *mem = NULL;
  size_t mem_size    = SaveEXRImageToMemory(exr_image, exr_header, &mem, err);
  if (mem_size == 0) {
    return TINYEXR_ERROR_SERIALZATION_FAILED;
  }

  size_t written_size = 0;
  if ((mem_size > 0) && mem) {
    written_size = fwrite(mem, 1, mem_size, fp);
  }
  free(mem);

  // fclose(fp);

  if (written_size != mem_size) {
    tinyexr::SetErrorMessage("Cannot write a file", err);
    return TINYEXR_ERROR_CANT_WRITE_FILE;
  }

  return TINYEXR_SUCCESS;
}

#endif  // TINYEXR_OTMOD_IMPLEMENTATION_DEFINED
#endif  // TINYEXR_OTMOD_IMPLEMENTATION