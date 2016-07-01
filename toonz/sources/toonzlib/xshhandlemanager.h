#pragma once

#ifndef XSHHANDLEMANAGER_INCLUDED
#define XSHHANDLEMANAGER_INCLUDED

#include "toonz/tstageobjecttree.h"

//===============================================================

//    Forward declarations

class TXsheet;
class ToonzScene;

//===============================================================

//*********************************************************************************
//    XshHandleManager  declaration
//*********************************************************************************

class XshHandleManager final : public HandleManager {
  TXsheet *m_xsh;

public:
  XshHandleManager(TXsheet *xsh) : m_xsh(xsh) {}

  TPointD getHandlePos(const TStageObjectId &id, const std::string &handle,
                       int row) const override;
};

#endif
