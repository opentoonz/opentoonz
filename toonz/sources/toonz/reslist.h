#pragma once

#ifndef RESLIST_H
#define RESLIST_H

#include <vector>
#include <map>

#include <QObject>

//***********************************************************************************************
//    ResListManager class
//***********************************************************************************************

//! The ResListManager is a \a singleton class which manages presets of camera
//! resolutions.
/*!
  This class allows a lightweight management of the resolution presets used in
  camera settings.
  Specific tasks include resolution fetching and the add/remove functions.
  Adding or removing
  presets causes the listChanged() signal to be emitted.

  \note There actually are \a two distinct instances of this class, one managing
  standard cameras
        and the other for cleanup cameras. Presets are stored in two distinct
  files as well.
*/

class ResListManager final : public QObject {
  Q_OBJECT

public:
  class Res {
  public:
    std::string m_name;
    int m_xres, m_yres;
    double m_fx, m_fy; /*- カメラサイズ -*/
    std::string m_xoffset,
        m_yoffset; /*- オフセット（単位付の文字列として保存） -*/
    double m_ar;
    Res()
        : m_name()
        , m_xres(0)
        , m_yres(0)
        , m_ar(0)
        , m_fx(0)
        , m_fy(0)
        , m_xoffset("")
        , m_yoffset("") {}
    Res(std::string name, int xres, int yres, double ar, double fx = 0,
        double fy = 0, std::string xoffset = 0, std::string yoffset = 0)
        : m_name(name)
        , m_xres(xres)
        , m_yres(yres)
        , m_ar(ar)
        , m_fx(fx)
        , m_fy(fy)
        , m_xoffset(xoffset)
        , m_yoffset(yoffset) {}
  };

private:
  bool m_forCleanup;
  std::map<std::string, Res> m_table;

private:
  ResListManager(bool forCleanup);

  void add(const std::string &line);

  void load();
  void save();

public:
  static ResListManager *instance(bool forCleanup);

  void getNames(std::vector<std::string> &names);
  const Res *getRes(const std::string &name) const;

  const Res *add(std::string name, int xres, int yres, double ar, double fx = 0,
                 double fy = 0, std::string xoffset = 0,
                 std::string yoffset = 0);
  void remove(const std::string &name);

signals:

  void listChanged();
};

#endif  // RESLIST_H
