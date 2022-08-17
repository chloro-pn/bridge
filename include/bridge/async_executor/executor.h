#pragma once

#include "async_simple/Executor.h"
#include "bridge/async_executor/thread_pool.h"

namespace bridge {
/*
 * 使用async_simple实现多核并行的编码和解码工作
 * 思路：当某个协程在编码/解码一个Array或者Map时，可以选择性的将Array或Map拆分成1-多个子任务
 * 然后调用collectAll挂起自身并等待子任务完成，继续解析剩余部分
 */
class BridgeExecutor : public async_simple::Executor {
 public:
  using Func = async_simple::Executor::Func;
  using Context = async_simple::Executor::Context;
  using ExecutorStat = async_simple::ExecutorStat;
  using IOExecutor = async_simple::IOExecutor;

  explicit BridgeExecutor(size_t worker_num)
      : async_simple::Executor("bridge_simple_threadpool"), worker_num_(worker_num) {}

  bool schedule(Func func) override {
    auto& pool = ThreadPool::GetOrConstructInstance(worker_num_);
    pool.PushTask(std::move(func));
    return true;
  }

  bool currentThreadInExecutor() const override { return currentContextId() != -1; }

  ExecutorStat stat() const override { return ExecutorStat(); }

  size_t currentContextId() const override {
    auto& pool = ThreadPool::GetOrConstructInstance(worker_num_);
    return pool.GetCurrentId();
  }

  IOExecutor* getIOExecutor() override { return nullptr; }

  ~BridgeExecutor() = default;

 private:
  size_t worker_num_;
};
}  // namespace bridge