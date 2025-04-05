#pragma once
#include "../utils/log.h"
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stack>
#include <thread>
#include <unordered_map>
#include <vector>

template <typename T> class SharedRessource {
  std::shared_ptr<std::mutex> m;
  std::shared_ptr<T> resource;

public:
  std::shared_ptr<T> lock() {
    m->lock();
    return resource;
  }

  void unlock() { m->unlock(); }
};

template <typename Data> class Worker {
public:
  typedef int worker_id;

  worker_id id;
  std::shared_ptr<std::condition_variable> dispatcher_cv;
  std::shared_ptr<std::mutex> dispatcher_m;
  std::shared_ptr<std::stack<worker_id>> available_workers;
  std::thread runner;
  std::shared_ptr<Data[]> buffer;

public:
  Worker(worker_id id, std::shared_ptr<std::condition_variable> cv,
         std::shared_ptr<std::mutex> m,
         std::shared_ptr<std::stack<worker_id>> aw)
      : id(id), dispatcher_cv(cv), dispatcher_m(m), available_workers(aw) {}

  void __init(size_t buffer_s) { buffer = std::make_unique<Data[]>(buffer_s); }

  void release() {
    DEBUG("Released worker : " << id);
    std::unique_lock dispatcher_q_lock(*dispatcher_m);
    available_workers->push(id);
    dispatcher_cv->notify_one();
  }

  template <typename F, typename... Args> void run(F f, Args... args) {
    // Run callable
    runner = std::thread(
        [this, f](Args... args) {
          f(args...);
          release();
        },
        args...);
  }

  std::shared_ptr<Data[]> get_buffer() { return buffer; }
};

template <typename Data> class LoadDispatcher {
  using worker_id = typename Worker<Data>::worker_id;

  int max_worker_n = 10;
  int worker_n = 0;
  worker_id next_worker_id = 0;

  size_t buffer_size;

  std::chrono::duration<int, std::micro> time_before_new_worker;

  std::shared_ptr<std::condition_variable> dispatcher_cv;
  std::shared_ptr<std::mutex> dispatcher_m;
  std::shared_ptr<std::stack<worker_id>> available_workers;

  std::unordered_map<worker_id, std::shared_ptr<Worker<Data>>> worker_pool;

public:
  LoadDispatcher(int time_before_new_worker_micro, int buffer_s)
      : buffer_size(buffer_s) {
    dispatcher_m = std::make_shared<std::mutex>();
    dispatcher_cv = std::make_shared<std::condition_variable>();
    available_workers = std::make_shared<std::stack<worker_id>>();

    std::chrono::microseconds us(time_before_new_worker_micro);
    time_before_new_worker = std::chrono::duration<int, std::micro>(us);
  }

  void join() {
    DEBUG("Joining");
    for (auto &worker : worker_pool) {
      auto w = worker.second;
      w->runner.join();
    }
    DEBUG("Joined");
  }

  worker_id _pick_worker() {
    auto worker_id = available_workers->top();
    available_workers->pop();
    worker_pool.at(worker_id)->runner.join();
    return worker_id;
  }

  worker_id _new_worker() {
    auto id = next_worker_id++;
    auto worker = std::make_shared<Worker<Data>>(
        id, dispatcher_cv, dispatcher_m, available_workers);
    DEBUG("Created worker - id = " << worker->id);
    worker->__init(buffer_size);
    worker_pool[id] = worker;
    worker_n++;
    return id;
  }

  std::shared_ptr<Worker<Data>> request_worker() {
    DEBUG("Requesting buffer");
    std::unique_lock dispatcher_q_lock(*dispatcher_m);

    worker_id id;
    if (available_workers->size()) {
      DEBUG("Picking available worker");
      id = _pick_worker();
      DEBUG("Picked worker : " << id);
    } else {
      if (worker_n == max_worker_n) {
        DEBUG("Max worker reached : waiting for worker to become available");
        dispatcher_cv->wait(dispatcher_q_lock);
        id = _pick_worker();
        DEBUG("Picked worker : " << id);
      } else {
        DEBUG("Waiting a bit for available worker to be released");
        dispatcher_cv->wait_for(dispatcher_q_lock, time_before_new_worker);
        if (available_workers->size()) {
          DEBUG("Workder released : picking available worker");
          id = _pick_worker();
          DEBUG("Picked worker : " << id);
        } else {
          DEBUG("Finally creating new worker");
          id = _new_worker();
        }
      }
    }
    return worker_pool.at(id);
  }

  void push_work() {}
};