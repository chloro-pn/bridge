#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "bridge/util.h"

namespace bridge {

class ThreadPool {
 public:
  template <typename T>
  void PushTask(T&& task) {
    std::unique_lock<std::mutex> lock(mut_);
    if (stop_) return;
    tasks_.emplace(std::forward<T>(task));
    lock.unlock();
    cond_.notify_one();
  }

  void Stop() {
    std::unique_lock<std::mutex> lock(mut_);
    if (stop_ == true) {
      return;
    }
    stop_ = true;
    lock.unlock();
    cond_.notify_all();
    for (auto& worker : works_) {
      worker.join();
    }
  }

  std::pair<size_t, ThreadPool*>* getCurrent() const {
    static thread_local std::pair<size_t, ThreadPool*> current(-1, nullptr);
    return &current;
  }

  size_t GetCurrentId() const {
    auto current = getCurrent();
    if (this == current->second) {
      return current->first;
    }
    return -1;
  }

  ~ThreadPool() { Stop(); }

  static ThreadPool& GetOrConstructInstance(size_t worker_num) {
    static ThreadPool obj(worker_num);
    return obj;
  }

  size_t GetWorkerNum() const { return works_.size(); }

 private:
  using task_type = std::function<void()>;
  std::vector<std::thread> works_;
  std::queue<task_type> tasks_;
  std::mutex mut_;
  std::condition_variable cond_;
  bool stop_;

  explicit ThreadPool(size_t work_num) : stop_(false) {
    for (size_t i = 0; i < work_num; ++i) {
      works_.emplace_back([this, i]() {
        this->getCurrent()->first = i;
        this->getCurrent()->second = this;
        while (true) {
          std::function<void()> task;
          std::unique_lock<std::mutex> lock(this->mut_);
          this->cond_.wait(lock, [this] { return this->stop_ == true || this->tasks_.empty() == false; });
          if (this->stop_ == true && this->tasks_.empty() == true) {
            return;
          }
          task = std::move(this->tasks_.front());
          this->tasks_.pop();
          lock.unlock();
          task();
        }
      });
    }
  }
};

}  // namespace bridge