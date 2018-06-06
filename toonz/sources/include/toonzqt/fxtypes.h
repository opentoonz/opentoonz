#pragma once

#ifndef TFXTYPES_INCLUDED
#define TFXTYPES_INCLUDED

enum eFxSchematicPortType {
  eFxOutputPort     = 200,
  eFxInputPort      = 201,
  eFxLinkPort       = 202,
  eFxGroupedInPort  = 203,
  eFxGroupedOutPort = 204
};

enum eFxType {
  eNormalFx              = 100,
  eZeraryFx              = 101,
  eMacroFx               = 102,
  eColumnFx              = 103,
  eOutpuFx               = 104,
  eXSheetFx              = 106,
  eGroupedFx             = 107,
  eNormalImageAdjustFx   = 108,
  eNormalLayerBlendingFx = 109,
  eNormalMatteFx         = 110
};

#endif
