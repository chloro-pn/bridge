#include <stdexcept>

#include "async_simple/coro/Lazy.h"
#include "async_simple/coro/SyncAwait.h"
#include "bridge/async_executor/executor.h"
#include "bridge/async_executor/task.h"
#include "bridge/object.h"
namespace bridge {

unique_ptr<Object> CoroutineScheduler::Parse() {
  size_t meta_size = GetMetaSize();
  // 判断根节点是否需要并行，如果不需要，直接构造NormalSchedule解析
  if (NeedToSplit() == false) {
    NormalScheduler ns(content_, parse_ref_, GetBridgePool());
    return ns.Parse();
  }
  BridgeExecutor e(worker_num_);
  async_simple::coro::RescheduleLazy Scheduled =
      ParseMap(content_, GetSplitInfo(), GetStringMap(), meta_size, parse_ref_).via(&e);
  auto root = async_simple::coro::syncAwait(Scheduled);
  GetBridgePool().Merge(std::move(root.bp));
  return parse_ref_ == false ? AsMap(root.v)->Get("root") : static_cast<MapView*>(root.v.get())->Get("root");
}

unique_ptr<Object> Parse(const std::string& content, BridgePool& bp, ParseOption po) {
  if (po.type == SchedulerType::Normal) {
    NormalScheduler ns(content, po.parse_ref, bp);
    return ns.Parse();
  }
  if (po.type == SchedulerType::Coroutine) {
    CoroutineScheduler cs(content, po.parse_ref, po.worker_num_, bp);
    return cs.Parse();
  }
  throw std::logic_error("invalid po.type");
  return unique_ptr<Object>(nullptr, object_pool_deleter);
}

}  // namespace bridge