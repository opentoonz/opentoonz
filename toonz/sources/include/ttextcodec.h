#pragma once

#ifndef TTEXTCODEC_INCLUDED
#define TTEXTCODEC_INCLUDED

#include <QByteArray>
#include <QString>
#include <QTextCodec>

namespace TTextCodec {

inline QTextCodec *codecForName(const char *name) {
  QTextCodec *codec = QTextCodec::codecForName(name);
  return codec ? codec : QTextCodec::codecForName("UTF-8");
}

inline QByteArray fromUnicode(const char *name, const QString &text) {
  QTextCodec *codec = codecForName(name);
  return codec ? codec->fromUnicode(text) : text.toUtf8();
}

inline QString toUnicode(const char *name, const QByteArray &bytes) {
  QTextCodec *codec = codecForName(name);
  return codec ? codec->toUnicode(bytes) : QString::fromUtf8(bytes);
}

inline QString toUnicode(const char *name, const char *text) {
  return toUnicode(name, QByteArray(text));
}

}  // namespace TTextCodec

#endif
