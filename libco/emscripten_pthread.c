#define LIBCO_C
#include "libco.h"
#include "settings.h"

// Naively emulate coroutines with pthreads;
// should be compiled with
// -sUSE_PTHREADS -sPROXY_TO_PTHREAD -sALLOW_MEMORY_GROWTH=1.
//
// Code released under public domain.

#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*thread_cb)();

typedef struct {
  pthread_t thread;
  sem_t lock;
  thread_cb entrypoint;
  int id;
} context_t;

static unsigned int global_thread_id = 1;

// not thread_local, since we are using pthreads
// XXX: how to still allow multithreading with different coroutines?
static context_t co_primary = {};
static context_t* co_running = NULL;

static void* thread_entry(void* arg) {
  context_t* context = (context_t*)arg;
  sem_wait(&context->lock);
  context->entrypoint();
  // coentry should not return
  LIBCO_ASSERT(0);
  return NULL;
}

static inline void _co_initialize() {
  LIBCO_ASSERT(sem_init(&co_primary.lock, 0, 0) == 0);
  co_running = &co_primary;
}

cothread_t co_active() {
  if (!co_running) _co_initialize();
  return (cothread_t)co_running;
}

cothread_t co_derive(void* memory, unsigned int size, void (*coentry)(void)) {
  return NULL;
}

cothread_t co_create(unsigned int size, void (*coentry)(void)) {
  // TODO: consider size
  if (!co_running) _co_initialize();
  context_t* created = LIBCO_MALLOC(sizeof(context_t));
  if (created) {
    int ret;

    ret = sem_init(&created->lock, 0, 0);
    if (ret) goto fail;
    ret = pthread_create(&created->thread, NULL, thread_entry, (void*)created);
    // FIXME: we assume that a valid pthread id is never itself
    if (ret) { created->thread = pthread_self(); goto fail; }

    created->id = global_thread_id++;
    created->entrypoint = coentry;
  }
  return (cothread_t)created;

  fail:
  co_delete((cothread_t)created);
  return NULL;
}

void co_delete(cothread_t cothread) {
  if (cothread && cothread != &co_primary) {
    pthread_t thread = ((context_t*)cothread)->thread;
    // zero when NOT equal; see comment in co_create
    if (pthread_equal(thread, pthread_self()) == 0) {
      LIBCO_ASSERT(pthread_cancel(thread) == 0);
      LIBCO_ASSERT(pthread_join(thread, NULL) == 0);
    }

    // cothread is ended; safe to destory the lock
    sem_t* lock = &((context_t*)cothread)->lock;
    LIBCO_ASSERT(lock && sem_destroy(lock) == 0);

    LIBCO_FREE(cothread);
  }
}

void co_switch(cothread_t cothread) {
  context_t* co_old = co_running;
  co_running = (context_t*)cothread;
  LIBCO_ASSERT(sem_post(&co_running->lock) == 0);
  if (sem_wait(&co_old->lock) != 0) {
    LIBCO_ASSERT(errno == EINTR);
    pthread_exit(NULL);
  }
}

int co_serializable() {
  return 0;
}

#ifdef __cplusplus
}
#endif
