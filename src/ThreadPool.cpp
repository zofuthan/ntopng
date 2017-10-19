/*
 *
 * (C) 2017 - ntop.org
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "ntop_includes.h"

// #define THREAD_DEBUG 1

/* **************************************************** */

static void* doRun(void* ptr)  {
  ((ThreadPool*)ptr)->run();
  return(NULL);
}

/* **************************************************** */

ThreadPool::ThreadPool(u_int8_t _pool_size) {
  pool_size = _pool_size, queue_len = 0;
  m = new Mutex();
  c = new ConditionalVariable();
  terminating = false;
  
  if((threadsState = (pthread_t*)malloc(sizeof(pthread_t)*pool_size)) == NULL)
    throw("Not enough memory");
  
  for(int i=0; i<pool_size; i++)
    pthread_create(&threadsState[i], NULL, doRun, (void*)this);
}

/* **************************************************** */

ThreadPool::~ThreadPool() {
  void *res;

  shutdown();
  
  for(int i=0; i<pool_size; i++) {
#ifdef THREAD_DEBUG
    ntop->getTrace()->traceEvent(TRACE_NORMAL, "Threads still running %d", pool_size-i);
#endif
    pthread_join(threadsState[i], &res);    
  }
  
  delete m;
  delete c;
}

/* **************************************************** */

void ThreadPool::run() {
#ifdef THREAD_DEBUG
  ntop->getTrace()->traceEvent(TRACE_NORMAL, "*** Starting thread [%u]", pthread_self());
#endif
  
  while(!terminating) {
    ThreadedActivity *t;

    
#ifdef THREAD_DEBUG  
    ntop->getTrace()->traceEvent(TRACE_NORMAL, "*** About to dequeue job [%u][terminating=%d]",
				 pthread_self(), terminating);
#endif
    
    t = dequeueJob(true);

#ifdef THREAD_DEBUG
    ntop->getTrace()->traceEvent(TRACE_NORMAL, "*** Dequeued job [%u][t=%p][terminating=%d]",
				 pthread_self(), t, terminating);
#endif
    
    if((t == NULL) || terminating)
      break;
    else {
      t->runScript();
    }
  }

#ifdef THREAD_DEBUG
  ntop->getTrace()->traceEvent(TRACE_NORMAL, "*** Terminating thread [%u]", pthread_self());
#endif
}

/* **************************************************** */

bool ThreadPool::queueJob(ThreadedActivity *j) {
  if(terminating)
    return(false);

  m->lock(__FILE__, __LINE__);  
  threads.push(j);
  queue_len++;
  m->unlock(__FILE__, __LINE__);

  c->signal(false);
  return(true); /*  TODO: add a max queue len and return false */
}

/* **************************************************** */

ThreadedActivity* ThreadPool::dequeueJob(bool waitIfEmpty) {
  ThreadedActivity *t;

  if(waitIfEmpty) {
    while((queue_len == 0) && (!terminating))
      c->wait();    
  }
  
  if((queue_len == 0) || terminating)
    return(NULL);

  m->lock(__FILE__, __LINE__);
  t = threads.front();
  threads.pop();
  queue_len--;
  m->unlock(__FILE__, __LINE__);
  
  return(t);
}

/* **************************************************** */


void ThreadPool::shutdown() {
#ifdef THREAD_DEBUG
  ntop->getTrace()->traceEvent(TRACE_NORMAL, "*** %s() ***", __FUNCTION__);
#endif
  
  terminating = true;
  c->signal(true /* Broadcast */);
}