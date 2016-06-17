

//=========================================================
//=========================================================
//=========================================================
//
// OBSOLETO: eliminare
//
//=========================================================
//=========================================================
//=========================================================
#ifdef OBSOLETO

#include "toonz/imagelocation.h"
#include "toonz/txsheet.h"
#include "application_imp.h"

TImageLocation::TImageLocation()
    : m_row(0)
    , m_col(0)
    , m_fid()
    , m_spline(0)
    , m_levelName(L"")
    , m_type(None) {}

TImageLocation::TImageLocation(int row, int col)
    : m_row(row)
    , m_col(col)
    , m_fid()
    , m_spline(0)
    , m_levelName(L"")
    , m_type(XsheetImage) {}

/*
TImageLocation::TImageLocation(const TStageObjectId &pid)
                      : m_row(0)
                      , m_col(0)
                      , m_fid()
                      , m_pid(pid)
                      , m_levelName(L"")
                      , m_type(PathImage)
{
}
*/
TImageLocation::TImageLocation(TStageObjectSpline *spline)
    : m_row(0)
    , m_col(0)
    , m_fid()
    , m_spline(spline)
    , m_levelName(L"")
    , m_type(PathImage) {}

TImageLocation::TImageLocation(std::wstring levelName, const TFrameId &fid)
    : m_row(0)
    , m_col(0)
    , m_fid(fid)
    , m_spline(0)
    , m_levelName(levelName)
    , m_type(LevelStripImage) {}

#endif
