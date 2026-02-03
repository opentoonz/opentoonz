

// TnzCore includes
#include "tparam.h"
#include "tparamchange.h"

DEFINE_CLASS_CODE(TParam, 11)

double TParamChange::m_minFrame = -(std::numeric_limits<double>::max)();
double TParamChange::m_maxFrame = +(std::numeric_limits<double>::max)();

//------------------------------------------------------------------------------

TParamChange::TParamChange(TParam *param, double firstAffectedFrame,
                           double lastAffectedFrame, bool keyframeChanged,
                           bool dragging, bool undoing)
    : m_param(param)
    , m_firstAffectedFrame(firstAffectedFrame)
    , m_lastAffectedFrame(lastAffectedFrame)
    , m_keyframeChanged(keyframeChanged)
    , m_dragging(dragging)
    , m_undoing(undoing) {}
