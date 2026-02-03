#pragma once

#ifndef TFXPARAM_INCLUDED
#define TFXPARAM_INCLUDED

// #include "tcommon.h"

#include "tfx.h"
#include "tparamcontainer.h"

template <class T>
void bindParam(TFx *fx, std::string name, T &var, bool hidden = false,
               bool obsolete = false) {
  fx->getParams()->add(new TParamVarT<T>(name, &var, 0, hidden, obsolete));
  var->addObserver(fx);
}

template <class T>
void bindPluginParam(TFx *fx, std::string name, T &var, bool hidden = false,
                     bool obsolete = false) {
  fx->getParams()->add(new TParamVarT<T>(name, nullptr, var, hidden, obsolete));
  var->addObserver(fx);
}

#endif
