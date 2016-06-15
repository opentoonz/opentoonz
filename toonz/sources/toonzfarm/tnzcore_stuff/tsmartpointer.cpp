

#include "tsmartpointer.h"
#include "tthread.h"

//-------------------------------------------------------------------

namespace {

//-------------------------------------------------------------------

typedef TAtomicVar *TAtomicVarPtr;

const int maxClassCode = 100;
TAtomicVarPtr instanceCounts[maxClassCode + 1];

//-------------------------------------------------------------------

inline TAtomicVar &getInstanceCounter(long classCode) {
  assert(0 <= classCode && classCode <= maxClassCode);
  TAtomicVarPtr &instanceCountPtr = instanceCounts[classCode];
  if (instanceCountPtr == 0) {
    static TThread::Mutex mutex;
    TThread::ScopedLock g(mutex);
    if (instanceCountPtr == 0) instanceCountPtr = new TAtomicVar();
  }
  assert(instanceCountPtr);
  return *instanceCountPtr;
}

//-------------------------------------------------------------------

}  // namespace

//-------------------------------------------------------------------

const long TSmartObject::m_unknownClassCode = 0;

void TSmartObject::incrementInstanceCount() {
#ifdef INSTANCE_COUNT_ENABLED
  TAtomicVar &instanceCount = getInstanceCounter(m_classCodeRef);
  ++instanceCount;
#else
  assert(0);
#endif
}

//-------------------------------------------------------------------

void TSmartObject::decrementInstanceCount() {
#ifdef INSTANCE_COUNT_ENABLED
  TAtomicVar &instanceCount = getInstanceCounter(m_classCodeRef);
  assert(instanceCount > 0);
  --instanceCount;
#else
  assert(0);
#endif
}

//-------------------------------------------------------------------

long TSmartObject::getInstanceCount(ClassCode code) {
#ifdef INSTANCE_COUNT_ENABLED
  TAtomicVar &instanceCount = getInstanceCounter(code);
  return instanceCount;
#else
  assert(0);
  return 0;
#endif
}

//-------------------------------------------------------------------
