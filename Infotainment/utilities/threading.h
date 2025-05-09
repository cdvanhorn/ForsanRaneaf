/**
 * @file threading.h
 * https://nachtimwald.com/2019/04/05/cross-platform-thread-wrapper/
 */

#ifndef THREADING_H
#define THREADING_H

#ifdef _WIN32
#include <stdbool.h>
#include <windows.h>
#include <synchapi.h>
#include <time.h>
#else
#include <pthread.h>
#endif

#ifdef _WIN32
typedef CRITICAL_SECTION pthread_mutex_t;
typedef void pthread_mutexattr_t;
typedef void pthread_attr_t;
typedef void pthread_condattr_t;
typedef void pthread_rwlockattr_t;
typedef HANDLE pthread_t;
typedef CONDITION_VARIABLE thread_cond_t;

typedef struct {
	SRWLOCK lock;
	bool exclusive;
} pthread_rwlock_t;

#endif

#ifdef _WIN32
int pthread_create(pthread_t *thread, pthread_attr_t *attr,
		   void *(*start_routine)(void *), void *arg);
int pthread_join(pthread_t thread, void **value_ptr);
int pthread_detach(pthread_t);

int pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

int pthread_cond_init(thread_cond_t *cond, pthread_condattr_t *attr);
int pthread_cond_destroy(const thread_cond_t *cond);
int pthread_cond_wait(thread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_timedwait(thread_cond_t *cond, pthread_mutex_t *mutex,
			   const struct timespec *abstime);
int pthread_cond_signal(thread_cond_t *cond);
int pthread_cond_broadcast(thread_cond_t *cond);

int pthread_rwlock_init(pthread_rwlock_t *rwlock,
			const pthread_rwlockattr_t *attr);
int pthread_rwlock_destroy(const pthread_rwlock_t *rwlock);
int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_unlock(pthread_rwlock_t *rwlock);
#endif

unsigned int threading_get_num_procs();

#endif //THREADING_H
