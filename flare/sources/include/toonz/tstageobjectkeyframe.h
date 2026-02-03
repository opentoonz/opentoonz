#pragma once

#ifndef T_STAGE_OBJECT_KEYFRAME_INCLUDED
#define T_STAGE_OBJECT_KEYFRAME_INCLUDED

// TnzBase includes
#include "tdoublekeyframe.h"

// TnzLib includes
#include "toonz/tstageobject.h"

//***********************************************************************************
//    TStageObject::Keyframe  definition
//***********************************************************************************

struct TStageObject::Keyframe {
  TDoubleKeyframe m_channels[T_ChannelCount];  //!< Keyframe per standard stage
                                               //! object channel
  SkDKey m_skeletonKeyframe;  //!< Keyframes for the eventual plastic
                              //!< deformation

  bool m_isKeyframe;  //!< False, in case each channel value was interpolated
  double m_easeIn, m_easeOut;  //!< Uniform Ease parameters, <0 in case the
                               //!< parameters have different eases
  Keyframe() : m_isKeyframe(false), m_easeIn(0), m_easeOut(0) {}
};

#endif  // T_STAGE_OBJECT_KEYFRAME_INCLUDED
