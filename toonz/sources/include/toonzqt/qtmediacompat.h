#pragma once

#ifndef TOONZQT_QTMEDIACOMPAT_H
#define TOONZQT_QTMEDIACOMPAT_H

#include <QtGlobal>
#include <QMediaPlayer>
#include <QUrl>

namespace QtCompat {

inline auto mediaPlayerState(const QMediaPlayer* player) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return player->playbackState();
#else
  return player->state();
#endif
}

inline void setMediaPlayerSource(QMediaPlayer* player, const QUrl& url) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  player->setSource(url);
#else
  player->setMedia(url);
#endif
}

}  // namespace QtCompat

#endif  // TOONZQT_QTMEDIACOMPAT_H
