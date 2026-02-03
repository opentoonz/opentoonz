#pragma once

#ifndef PLASTICVISUALSETTINGS_H
#define PLASTICVISUALSETTINGS_H

//===========================================================

//    Forward declarations

class TXshColumn;

//===========================================================

//*********************************************************************************************
//    PlasticVisualSettings  definition
//*********************************************************************************************

//! The PlasticVisualSettings class stores the fundamental visualization options
//! that need
//! to be implemented in any painter supporting the plastic framework.

struct PlasticVisualSettings {
  bool m_applyPlasticDeformation;  //!< Whether the deformation must be applied.
                                   //! If false,
  //!< the original image should be displayed instead.
  TXshColumn *m_showOriginalColumn;  //!< As an exception to the above control,
                                     //! one specific
  //!< mesh column can be dispensed from deforming.
  //!< This is typically used in PlasticTool's 'build mode'.
  bool m_drawMeshesWireframe;  //!< Whether any mesh wireframe should be
                               //! displayed
  bool m_drawRigidity;         //!< Whether mesh rigidities should be displayed
  bool m_drawSO;               //!< Whether mesh vertices' stacking order should
                               //!< be displayed
public:
  PlasticVisualSettings()
      : m_applyPlasticDeformation(true)
      , m_showOriginalColumn()
      , m_drawMeshesWireframe(true)
      , m_drawRigidity(false)
      , m_drawSO(false) {}
};

#endif  // PLASTICVISUALSETTINGS_H
