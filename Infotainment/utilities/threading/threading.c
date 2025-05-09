/**
* @file threading.c
*/

#include "../threading.h"

#include "../defines.h"

// void ms_to_timespec(struct timespec *ts, unsigned int ms);

#ifdef _WIN32
typedef struct {
	void *(*start_routine)(void *);
	void *start_arg;
} win_thread_start_t;

static DWORD WINAPI win_thread_start(void *arg)
{
	win_thread_start_t *data = arg;
	void *(*start_routine)(void *) = data->start_routine;
	void *start_arg = data->start_arg;

	free(data);

	start_routine(start_arg);
	return FUNC_SUCCESS;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
		   void *(*start_routine)(void *), void *arg)
{
	(void)attr;

	if (thread == NULL || start_routine == NULL)
		return 1;

	win_thread_start_t *data = malloc(sizeof(*data));
	data->start_routine = start_routine;
	data->start_arg = arg;

	*thread = CreateThread(NULL, 0, win_thread_start, data, 0, NULL);
	if (*thread == NULL)
		return FUNC_FAILURE;
	return FUNC_SUCCESS;
}

int pthread_join(pthread_t thread, void **value_ptr)
{
	(void)value_ptr;
	WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);
	return FUNC_SUCCESS;
}

int pthread_detach(pthread_t thread)
{
	CloseHandle(thread);
	return FUNC_SUCCESS;
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
	(void)attr;

	if (mutex == NULL)
		return FUNC_FAILURE;

	InitializeCriticalSection(mutex);
	return FUNC_SUCCESS;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	if (mutex == NULL)
		return FUNC_FAILURE;
	DeleteCriticalSection(mutex);
	return FUNC_SUCCESS;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	if (mutex == NULL)
		return FUNC_FAILURE;
	EnterCriticalSection(mutex);
	return FUNC_SUCCESS;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	if (mutex == NULL)
		return FUNC_FAILURE;
	LeaveCriticalSection(mutex);
	return FUNC_SUCCESS;
}

static DWORD timespec_to_ms(const struct timespec *abstime)
{
	if (abstime == NULL)
		return INFINITE;

	DWORD t = ((abstime->tv_sec - time(NULL)) * 1000) +
		  (abstime->tv_nsec / 1000000);
	return t;
}

int pthread_cond_init(thread_cond_t *cond, const pthread_condattr_t *attr)
{
	(void)attr;
	if (cond == NULL)
		return FUNC_FAILURE;
	InitializeConditionVariable(cond);
	return FUNC_SUCCESS;
}

int pthread_cond_destroy(const thread_cond_t *cond)
{
	/* Windows does not have a destroy for conditionals */
	(void)cond;
	return FUNC_SUCCESS;
}

int pthread_cond_wait(thread_cond_t *cond, pthread_mutex_t *mutex)
{
	if (cond == NULL || mutex == NULL)
		return FUNC_FAILURE;
	return pthread_cond_timedwait(cond, mutex, NULL);
}

int pthread_cond_timedwait(thread_cond_t *cond, pthread_mutex_t *mutex,
			   const struct timespec *abstime)
{
	if (cond == NULL || mutex == NULL)
		return FUNC_FAILURE;
	if (!SleepConditionVariableCS(cond, mutex, timespec_to_ms(abstime)))
		return FUNC_FAILURE;
	return FUNC_SUCCESS;
}

int pthread_cond_signal(thread_cond_t *cond)
{
	if (cond == NULL)
		return FUNC_FAILURE;
	WakeConditionVariable(cond);
	return FUNC_SUCCESS;
}

int pthread_cond_broadcast(thread_cond_t *cond)
{
	if (cond == NULL)
		return FUNC_FAILURE;
	WakeAllConditionVariable(cond);
	return FUNC_SUCCESS;
}

int pthread_rwlock_init(pthread_rwlock_t *rwlock,
			const pthread_rwlockattr_t *attr)
{
	(void)attr;
	if (rwlock == NULL)
		return FUNC_FAILURE;
	InitializeSRWLock(&(rwlock->lock));
	rwlock->exclusive = false;
	return FUNC_SUCCESS;
}

int pthread_rwlock_destroy(const pthread_rwlock_t *rwlock)
{
	(void)rwlock;
	return FUNC_SUCCESS;
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
	if (rwlock == NULL)
		return FUNC_FAILURE;
	AcquireSRWLockShared(&(rwlock->lock));
	return FUNC_SUCCESS;
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock)
{
	if (rwlock == NULL)
		return FUNC_FAILURE;
	return !TryAcquireSRWLockShared(&(rwlock->lock));
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
	if (rwlock == NULL)
		return FUNC_FAILURE;
	AcquireSRWLockExclusive(&(rwlock->lock));
	rwlock->exclusive = true;
	return FUNC_SUCCESS;
}

int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock)
{
	if (rwlock == NULL)
		return 1;

	const BOOLEAN ret = TryAcquireSRWLockExclusive(&(rwlock->lock));
	if (ret)
		rwlock->exclusive = true;
	return ret;
}

int pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
	if (rwlock == NULL)
		return FUNC_FAILURE;

	if (rwlock->exclusive) {
		rwlock->exclusive = false;
		ReleaseSRWLockExclusive(&(rwlock->lock));
	} else {
		ReleaseSRWLockShared(&(rwlock->lock));
	}
	return FUNC_SUCCESS;
}

unsigned int threading_get_num_procs()
{
	SYSTEM_INFO sysinfo;

	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
}

#else

#include <unistd.h>
unsigned int threading_get_num_procs()
{
	return (unsigned int)sysconf(_SC_NPROCESSORS_ONLN);
}

#endif
