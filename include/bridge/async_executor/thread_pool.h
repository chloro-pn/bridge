#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace birdge {

class ThreadPool {
 public:
  explicit ThreadPool(size_t work_num) : stop_(false) {
    for (size_t i = 0; i < work_num; ++i) {
      works_.emplace_back([this]() {
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

  ~ThreadPool() { Stop(); }

 private:
  using task_type = std::function<void()>;
  std::vector<std::thread> works_;
  std::queue<task_type> tasks_;
  std::mutex mut_;
  std::condition_variable cond_;
  bool stop_;
};

}  // namespace birdge