#pragma once

#ifndef CONTENTTYPES_H
#define CONTENTTYPES_H

#include <QCoreApplication>
#include <QHash>
#include <QString>
#include <QStringList>

#include <vector>

// File types shown by the viewers, grouped by content for filtering.
namespace ContentTypes {

struct Definition {
  const char *iconName;
  const char *label;
  QStringList extensions;
};

inline const std::vector<Definition> &definitions() {
  static const std::vector<Definition> defs = {
      {"browser_image_file",
       QT_TRANSLATE_NOOP("ContentTypes", "Images"),
       {"PNG", "TIF", "TIFF", "JPG", "JPEG", "TGA", "BMP", "EXR", "SGI",
        "RGB"}},
      {"browser_toonz_raster_file",
       QT_TRANSLATE_NOOP("ContentTypes", "Toonz Raster"),
       {"TLV"}},
      {"browser_toonz_vector_file",
       QT_TRANSLATE_NOOP("ContentTypes", "Toonz Vector"),
       {"PLI"}},
      {"browser_psd_file", QT_TRANSLATE_NOOP("ContentTypes", "PSD"), {"PSD"}},
      {"browser_svg_file", QT_TRANSLATE_NOOP("ContentTypes", "SVG"), {"SVG"}},
      {"browser_media_file",
       QT_TRANSLATE_NOOP("ContentTypes", "Media"),
       {"MP4", "MOV", "AVI", "WEBM", "GIF", "3GP", "APNG", "WEBP"}}};
  return defs;
}

inline QString label(const Definition &definition) {
  return QCoreApplication::translate("ContentTypes", definition.label);
}

inline int indexOf(const QString &extension) {
  static const QHash<QString, int> table = [] {
    QHash<QString, int> hash;
    const std::vector<Definition> &defs = definitions();
    for (size_t i = 0; i < defs.size(); ++i)
      for (const QString &ext : defs[i].extensions) hash.insert(ext, int(i));
    return hash;
  }();
  return table.value(extension.toUpper(), -1);
}

}  // namespace ContentTypes

#endif
