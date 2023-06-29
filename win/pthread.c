#include "pthread.h"

int pthread_mutex_init(pthread_mutex_t* mutex, void* attr)
{
    InitializeCriticalSection(mutex);
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t* mutex)
{
    EnterCriticalSection(mutex);
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex)
{
    LeaveCriticalSection(mutex);
    return 0;
}

int pthread_cond_init(pthread_cond_t* cond, void* attr)
{
    InitializeConditionVariable(cond);
    return 0;
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex)
{
    SleepConditionVariableCS(cond, mutex, INFINITE);
    return 0;
}

int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex, const long long* abstime_millis)
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);

    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    long long current_millis = uli.QuadPart / 10000;

    DWORD delta = (DWORD)(*abstime_millis > current_millis ? (*abstime_millis - current_millis) : 0);
    SleepConditionVariableCS(cond, mutex, delta);
    return 0;
}

int pthread_cond_signal(pthread_cond_t* cond)
{
    WakeConditionVariable(cond);
    return 0;
}

int pthread_cond_destroy(pthread_cond_t* cond)
{
    return 0; // nothing to do
}
