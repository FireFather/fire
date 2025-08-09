/*
 * Chess Engine - Threading and Search Coordination (Implementation)
 * ----------------------------------------------------------------
 * This file implements the worker thread class and the thread pool.
 *
 * Model:
 *  - A main thread (GUI-facing) and N-1 worker threads.
 *  - Each thread owns a threadinfo with per-thread buffers and stats.
 *  - Threads run idle_loop(): sleep until woken, then begin_search().
 *  - Synchronization via a mutex + condition variable per thread.
 *
 * Key responsibilities:
 *  - threadpool::init/exit: process lifetime and resource setup/teardown.
 *  - threadpool::begin_search: pass params and wake the main worker.
 *  - threadpool::change_thread_count: grow/shrink the worker set.
 *  - thread::idle_loop/wake/wait: cooperative sleep and wake protocol.
 *
 * Notes: comments are ASCII-only to avoid UTF-8 warnings.
 */

#include "thread.h"
#include <iostream>
#include "main.h"

/*
 * thread::thread()
 * -----------------
 * Spawn the native thread and park it in idle_loop().
 * The constructor blocks until the new thread signals it is idle,
 * so the thread object is fully initialized before returning.
 */

thread::thread() : exit_(false),
search_active(true),
thread_index(thread_pool.thread_count){
  std::unique_lock lk(mutex);
  native_thread=std::thread(&thread::idle_loop,this);
  sleep_condition.wait(lk,[&]{
    // Wait until the worker marks itself idle
    return !search_active;
  });
}

/*
 * thread::~thread()
 * -----------------
 * Signal the native thread to exit and join it.
 */

thread::~thread(){
  mutex.lock();
  exit_=true;
  sleep_condition.notify_one();
  mutex.unlock();
  native_thread.join();
}

/*
 * threadpool::init()
 * ------------------
 * Allocate global counter-move history, create the main thread object,
 * set initial thread count to 1, and configure global search settings.
 */

void threadpool::init(){
  cmh_data=static_cast<cmhinfo*>(calloc(sizeof(cmhinfo),true));
  threads[0]=new mainthread;
  thread_count=1;
  change_thread_count(thread_count);
  fifty_move_distance=50;
  multi_pv=1;
  total_analyze_time=0;
}

/*
 * threadpool::begin_search(pos, time)
 * -----------------------------------
 * Ensure no previous search is running, reset stop flags, publish
 * the new search parameters and root position, and wake the main
 * thread to start searching.
 */

void threadpool::begin_search(position& pos,const search_param& time){
  main()->wait_for_search_to_end();
  search::signals.stop_if_ponder_hit=search::signals.stop_analyzing=false;
  search::param=time;
  root_position=&pos;
  main()->wake(true);
}

/*
 * threadpool::delete_counter_move_history()
 * -----------------------------------------
 * Zero the global counter-move history table.
 */

void threadpool::delete_counter_move_history(){
  cmh_data->counter_move_stats.clear();
}

/*
 * threadpool::change_thread_count(num_threads)
 * --------------------------------------------
 * Grow or shrink the set of worker threads. Workers construct their
 * threadinfo on first entry to idle_loop().
 */

void threadpool::change_thread_count(const int num_threads){
  while(thread_count<num_threads) threads[thread_count++]=new thread;
  while(thread_count>num_threads) delete threads[--thread_count];
}

/*
 * threadpool::exit()
 * ------------------
 * Delete all threads and free global per-thread history storage.
 */

void threadpool::exit(){
  while(thread_count>0) delete threads[--thread_count];
  free(cmh_data);
}

/*
 * thread::idle_loop()
 * -------------------
 * The thread's main loop:
 *   1) Allocate per-thread data (threadinfo) and set root_position.
 *   2) Enter a sleep state (search_active == false) until woken.
 *   3) When woken and not exiting, call begin_search().
 *   4) Loop until exit_ is set by destructor or thread_pool::exit().
 */

void thread::idle_loop(){
  cmhi=cmh_data;
  auto* p=calloc(sizeof(threadinfo),true);
  if(p!=nullptr){
    std::memset(p,0,sizeof(threadinfo));
    ti=new(p) threadinfo;
  }
  root_position=&ti->root_position;
  while(!exit_){
    std::unique_lock lk(mutex);
    search_active=false;
    while(!search_active&&!exit_){
      // Notify constructor or waiters that we are idle, then sleep
      sleep_condition.notify_one();
      sleep_condition.wait(lk);// Sleep until wake() is called
    }
    lk.unlock();
    if(!exit_) begin_search();
  }
  free(p);
}

/*
 * thread::wait(condition)
 * -----------------------
 * Block until the given atomic_bool becomes true.
 */

void thread::wait(const std::atomic_bool& condition){
  std::unique_lock lk(mutex);
  sleep_condition.wait(lk,[&]{
    return static_cast<bool>(condition);
  });
}

/*
 * thread::wait_for_search_to_end()
 * --------------------------------
 * Block until this thread's search_active flag becomes false.
 */

void thread::wait_for_search_to_end(){
  std::unique_lock lk(mutex);
  sleep_condition.wait(lk,[&]{
    // Wait until the worker marks itself idle
    return !search_active;
  });
}

/*
 * thread::wake(activate_search)
 * -----------------------------
 * Optionally set search_active and signal the condition variable.
 * Used to start or nudge a sleeping thread.
 */

void thread::wake(const bool activate_search){
  std::unique_lock lk(mutex);
  if(activate_search) search_active=true;// Arm the flag that idle_loop checks
  sleep_condition.notify_one();
}

/*
 * threadpool::visited_nodes()
 * ---------------------------
 * Sum of nodes visited across all active threads' root positions.
 */

uint64_t threadpool::visited_nodes() const{
  uint64_t nodes=0;
  for(auto i=0;i<active_thread_count;++i) nodes+=threads[i]->root_position->visited_nodes();
  return nodes;
}

threadpool thread_pool;
