#pragma once

#ifndef igs_resource_thread_h
#define igs_resource_thread_h

#if defined _WIN32    //-------------------------------------------------
#include <windows.h>  // HANDLE
#include <process.h>  // _beginthreadex()
#ifndef IGS_RESOURCE_IFX_EXPORT
#define IGS_RESOURCE_IFX_EXPORT
#endif
namespace igs {
namespace resource {
// HANDLE = unsigned long(vc6.0) = void *(vc2005)
IGS_RESOURCE_IFX_EXPORT const HANDLE thread_run(
    unsigned(__stdcall *function)(void *), void *func_arg,
    const int priority = THREAD_PRIORITY_NORMAL
    /*
    priorityに与える値を優先度の高いものから順に並べます。
    THREAD_PRIORITY_TIME_CRITICAL		プロセスにより15 or 31
    THREAD_PRIORITY_HIGHEST			標準より2ポイント高い
    THREAD_PRIORITY_ABOVE_NORMAL		標準より1ポイント高い
    THREAD_PRIORITY_NORMAL			標準
    THREAD_PRIORITY_BELOW_NORMAL		標準より1ポイント低い
    THREAD_PRIORITY_LOWEST			標準より2ポイント低い
    THREAD_PRIORITY_IDLE			プロセスにより1 or 16
    */
    );
IGS_RESOURCE_IFX_EXPORT const bool thread_was_done(const HANDLE thread_id);
IGS_RESOURCE_IFX_EXPORT void thread_join(const HANDLE thread_id);
IGS_RESOURCE_IFX_EXPORT void thread_wait(const HANDLE thread_id);
IGS_RESOURCE_IFX_EXPORT void thread_close(const HANDLE thread_id);
}
}
#else  //--------------------------------------------------------------
#include <pthread.h>  // pthread_t,pthread_create(),pthread_join()
namespace igs {
namespace resource {
// pthread_t = unsigned long int(rhel4)
pthread_t thread_run(
    void *(*function)(void *), void *func_arg,
    const int state = PTHREAD_CREATE_JOINABLE
    /*
    state が
    PTHREAD_CREATE_JOINABLE なら、pthread_join()を呼んで終了を待つ
    PTHREAD_CREATE_DETACHED なら、なにも呼ぶ必要がないが、
            thread終了を知るには自前で仕掛けが必要。
    */
    );
void thread_join(const pthread_t thread_id);
}
}
#endif  //-------------------------------------------------------------

#endif /* !igs_resource_thread_h */
