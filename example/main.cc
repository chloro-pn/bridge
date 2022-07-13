#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "bridge/object.h"

using namespace bridge;

int main() {
  // 使用data、array和map元素构造需要序列化的对象
  std::unique_ptr<Data> v1 = ValueFactory<Data>("hello world");
  std::unique_ptr<Data> v2 = ValueFactory<Data>((int32_t)32);
  std::unique_ptr<Array> arr = ValueFactory<Array>();
  arr->Insert(std::move(v1));
  arr->Insert(std::move(v2));
  std::unique_ptr<Map> root = ValueFactory<Map>();
  root->Insert("key", std::move(arr));
  // 序列化
  std::string tmp = Serialize(std::move(root));
  // 反序列化
  auto new_root = Parse(tmp);
  // 通过ObjectWrapper代理访问new_root，其提供了便捷的接口以及类型检查。
  ObjectWrapper wrapper(new_root.get());
  std::cout << wrapper["key"][0].Get<std::string>().value() << std::endl;
  std::cout << wrapper["key"][1].Get<int32_t>().value() << std::endl;
  // 也可以将反序列化得到的new_root重新进行修改
  AsMap(new_root)->Insert("new_key", ValueFactory<Data>("new_value"));
  tmp = Serialize(std::move(new_root));
  new_root = Parse(tmp);
  // 注意，ObjectWrapper代理仅持有root对象的指针，并不影响其生命周期，因此使用者应该注意当root对象析构后，对应的代理对象不能再访问
  ObjectWrapper new_wrapper(new_root.get());
  std::cout << new_wrapper["new_key"].Get<std::string>().value() << std::endl;
  return 0;
}