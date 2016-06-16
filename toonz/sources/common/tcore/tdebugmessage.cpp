

#include "tdebugmessage.h"
#include <iostream>

using namespace std;

namespace {

TDebugMessage::Manager *debugManagerInstance = 0;
}

void TDebugMessage::setManager(Manager *manager) {
  debugManagerInstance = manager;
}

ostream &TDebugMessage::getStream() {
  if (debugManagerInstance)
    return debugManagerInstance->getStream();
  else
    return cout;
}

void TDebugMessage::flush(int code) {
  if (debugManagerInstance)
    debugManagerInstance->flush(code);
  else
    cout << endl;
}
