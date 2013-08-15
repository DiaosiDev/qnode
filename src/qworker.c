/*
 * See Copyright Notice in qnode.h
 */

#include <unistd.h>
#include <stdio.h>
#include "qalloc.h"
#include "qassert.h"
#include "qactor.h"
#include "qconfig.h"
#include "qengine.h"
#include "qdefines.h"
#include "qlist.h"
#include "qlog.h"
#include "qluautil.h"
#include "qserver.h"
#include "qworker.h"
#include "qthread_log.h"

extern qmsg_pt* worker_msg_handlers[];
qworker_t*      workers[QMAX_WORKER] = {NULL};

static int   worker_msg_handler(qmsg_t *msg, void *reader);
static void* worker_main(void *arg);
static void* worker_alloc(void *ud, void *ptr,
                          size_t osize, size_t nsize);
//static void  recycle_actors(void *data);
static void  free_actors(qworker_t *worker);

static int
worker_msg_handler(qmsg_t *msg, void *reader) {
  qworker_t *worker;

  worker = (qworker_t*)reader;
  qinfo("worker %d handle %d msg", worker->tid, msg->type);
  return (*worker_msg_handlers[msg->type])(msg, reader);
}

static void*
worker_main(void *arg) {
  qworker_t *worker;

  worker = (qworker_t*)arg;
  worker->running = 1;
  qengine_loop(worker->engine);

  qmailbox_free(&(worker->box));
  qengine_destroy(worker->engine);
  free_actors(worker);
#ifdef DEBUG
  ldb_free(worker->ldb);
#endif
  lua_close(worker->state);

  return NULL;
}

qworker_t*
qworker_new(qid_t tid) {
  qworker_t *worker;

  worker = qcalloc(sizeof(qworker_t));
  if (worker == NULL) {
    printf("create worker error\n");
    return NULL;
  }
  worker->engine = qengine_new();
  if (worker->engine == NULL) {
    printf("create worker engine error\n");
    return NULL;
  }
  worker->actors = qcalloc(sizeof(qactor_t *) * MAX_ID);
  if (worker->actors == NULL) {
    printf("create worker actors error\n");
    return NULL;
  }
  qmailbox_init(&(worker->box), worker_msg_handler,
                worker->engine, worker);
  worker->tid = tid;
  worker->current = 0;
  worker->alloc = 0;
  qmutex_init(&(worker->mutex));
  /* create the lua VM for the worker */
  worker->state = qlua_new_state(worker_alloc, worker);
  worker->running = 0;
  pthread_create(&worker->id, NULL,
                 worker_main, worker);

#ifdef DEBUG
  worker->ldb = ldb_new(worker->state);
#else
  worker->ldb = NULL;
#endif
  while (!worker->running) {
    usleep(100);
  }
  return worker;
}

void
qworker_destroy(qworker_t *worker) {
  /* wait for the thread stop */
  pthread_join(worker->id, NULL);
  qmutex_destroy(&(worker->mutex));
  qfree(worker);
}

qid_t
qworker_new_aid(qworker_t *worker) {
  qid_t aid;
  qid_t current;

  qmutex_lock(&(worker->mutex));
  current = worker->current;
  while (worker->actors[current]) {
    ++current;
  }
  aid = encode_aid(current, worker->tid);
  qassert(decode_pid(aid) == worker->tid);
  qassert(decode_id(aid)  == current);
  worker->current = ++current % MAX_ID;
  qmutex_unlock(&(worker->mutex));

  return aid;
}

qactor_t*
qworket_get_actor(qworker_t *worker, qid_t id) {
  qactor_t *actor;

  qmutex_lock(&(worker->mutex));
  actor = worker->actors[id];
  qmutex_unlock(&(worker->mutex));

  return actor;
}

void
qworker_add(qid_t aid, qactor_t *actor) {
  qworker_t *worker;
  qid_t id;

  worker = workers[decode_pid(aid)];
  id = decode_id(aid);
  qmutex_lock(&(worker->mutex));
  worker->actors[id] = actor;
  qmutex_unlock(&(worker->mutex));
}

void
qworker_delete(qid_t aid) {
  qworker_t *worker;
  qid_t id;

  worker = workers[decode_pid(aid)];
  id = decode_id(aid);
  qmutex_lock(&(worker->mutex));
  worker->actors[id] = NULL;
  qmutex_unlock(&(worker->mutex));
}

void
qworker_send(qmsg_t *msg) {
  qworker_t  *worker;
  qmailbox_t *box;

  worker = workers[msg->recver];
  box    = &(worker->box);
  qmailbox_add(box, msg);
}

static void *
worker_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  qworker_t *worker;

  worker = (qworker_t*)ud;
  worker->alloc += nsize - osize;
  if (nsize == 0) {
    qfree(ptr);
    return NULL;
  } else {
    return qrealloc(ptr, nsize);
  }
}

static void
free_actors(qworker_t *worker) {
  int       i;
  qactor_t *actor;
  
  for (i = 0; i < MAX_ID; ++i) {
    actor = worker->actors[i];
    if (!actor) {
      continue;
    }
    qactor_destroy(actor);
  }
  qfree(worker->actors);
  qmutex_destroy(&(worker->mutex));
}
