#pragma once

#ifndef SUBSCENECOMMAND_H
#define SUBSCENECOMMAND_H

#include "toonz/tstageobjectid.h"
#include "tfx.h"
#include <set>
#include <QList>

namespace SubsceneCmd {

void collapse(std::set<int> &indices);
void collapse(const QList<TStageObjectId> &objects);
void collapse(const QList<TFxP> &fxs);
void explode(int index);
}

#endif
