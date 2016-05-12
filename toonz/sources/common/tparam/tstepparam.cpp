

#include "tstepparam.h"

//
// OBSOLETO??
//

/*

PERSIST_IDENTIFIER(TDoubleStepParam, "doubleStepParam")

//------------------------------------------------------------------------------

TDoubleStepParam::TDoubleStepParam(double v)
				: TDoubleParam(v)
{}

//------------------------------------------------------------------------------

TDoubleStepParam::TDoubleStepParam(const TDoubleParam &src)
				: TDoubleParam(src)
{}

//------------------------------------------------------------------------------

TDoubleStepParam::~TDoubleStepParam()
{}

//------------------------------------------------------------------------------

double TDoubleStepParam::getValue(double frame, bool cropped) const
{
  if (getKeyframeCount() == 0)
	return getDefaultValue();

  if(isKeyframe(frame))
	return TDoubleParam::getValue(frame);

  int index = getPrevKeyframe(frame);
  if (index != -1)
	return getKeyframe(index).m_value;
  else
   return getKeyframe(0).m_value;
}

//------------------------------------------------------------------------------
*/
