#pragma once

#ifndef TOONZQT_QTMEDIACOMPAT_H
#define TOONZQT_QTMEDIACOMPAT_H

#include <QtGlobal>
#include <QMediaPlayer>
#include <QObject>
#include <QUrl>

#include <utility>

namespace QtCompat {

inline auto mediaPlayerState(const QMediaPlayer* player) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return player->playbackState();
#else
  return player->state();
#endif
}

inline bool mediaPlayerHasMedia(const QMediaPlayer* player) {
  return player->mediaStatus() != QMediaPlayer::NoMedia;
}

inline void setMediaPlayerSource(QMediaPlayer* player, const QUrl& url) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  player->setSource(url);
#else
  player->setMedia(url);
#endif
}

template <typename Receiver, typename Func>
inline QMetaObject::Connection connectMediaPlayerStateChanged(
    QMediaPlayer* player, Receiver* receiver, Func&& slot) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return QObject::connect(player, &QMediaPlayer::playbackStateChanged, receiver,
                          std::forward<Func>(slot));
#else
  return QObject::connect(player, &QMediaPlayer::stateChanged, receiver,
                          std::forward<Func>(slot));
#endif
}

}  // namespace QtCompat

#endif  // TOONZQT_QTMEDIACOMPAT_H
