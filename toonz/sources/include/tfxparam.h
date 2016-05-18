#pragma once

#ifndef TFXPARAM_INCLUDED
#define TFXPARAM_INCLUDED

// #include "tcommon.h"

#include "tfx.h"
#include "tparamcontainer.h"

template <class T>
void bindParam(TFx *fx, std::string name, T &var, bool hidden = false)
{
	fx->getParams()->add(new TParamVarT<T>(name, var, hidden));
	var->addObserver(fx);
}

#endif
