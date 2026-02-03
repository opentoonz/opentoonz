

#include "tcolumnset.h"

DEFINE_CLASS_CODE(TColumnHeader, 2)

//***************************************************************************************
//    TColumnHeader implementation
//***************************************************************************************

TColumnHeader::TColumnHeader()
    : TSmartObject(m_classCode)
    , m_width(100)
    , m_pos(-1)
    , m_index(-1)
    , m_inColumnsSet(false) {}
