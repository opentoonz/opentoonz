

#include "tsmartpointer.h"
#include "tthreadmessage.h"

//-------------------------------------------------------------------

namespace {

//-------------------------------------------------------------------

typedef TAtomicVar *TAtomicVarPtr;

const int maxClassCode = 200;
TAtomicVarPtr instanceCounts[maxClassCode + 1];

//-------------------------------------------------------------------

inline TAtomicVar &getInstanceCounter(TINT32 classCode) {
  assert(0 <= classCode && classCode <= maxClassCode);
  TAtomicVarPtr &instanceCountPtr = instanceCounts[classCode];
  if (instanceCountPtr == 0) {
    static TThread::Mutex mutex;
    TThread::MutexLocker g(&mutex);
    if (instanceCountPtr == 0) instanceCountPtr = new TAtomicVar();
  }
  assert(instanceCountPtr);
  return *instanceCountPtr;
}

//-------------------------------------------------------------------

}  // namespace

//-------------------------------------------------------------------

#ifdef INSTANCE_COUNT_ENABLED
const TINT32 TSmartObject::m_unknownClassCode = 0;
#endif

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

TINT32 TSmartObject::getInstanceCount(ClassCode
#ifdef INSTANCE_COUNT_ENABLED
                                          code
#endif
                                      ) {
#ifdef INSTANCE_COUNT_ENABLED
  TAtomicVar &instanceCount = getInstanceCounter(code);
  return instanceCount;
#else
  assert(0);
  return 0;
#endif
}

//-------------------------------------------------------------------
