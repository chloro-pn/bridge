#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include "bridge/object.h"

/*
 * 这个例子展示了bridge的基本使用，包括构造数据、序列化、反序列化、dump、通过Wrapper访问数据等基本功能。
 */

int main() {
  bridge::BridgePool bp;
  // Construct the object that need to be serialized
  std::string str("hello world");
  auto v1 = bp.data_view(std::string_view(str));
  auto v2 = bp.data((int32_t)32);
  auto arr = bp.array();
  arr->Insert(std::move(v1));
  arr->Insert(std::move(v2));
  auto root = bp.map();
  root->Insert("key", std::move(arr));
  /* serialize :
     {
       "key" : [
        "hello world",
        32,
       ],
     }
  */
  std::string buf;
  root->dump(buf, 0);
  std::cout << "--- dump --- " << std::endl << buf << std::endl << "------------ " << std::endl;
  std::string tmp = Serialize(std::move(root), bp);
  /* ...
   * transfer, storage, ...
   * ...
   */
  // deserialize
  auto new_root = bridge::Parse(tmp, bp);
  // access new_root through ObjectWrapper proxy
  bridge::ObjectWrapper wrapper(new_root.get());
  std::cout << wrapper["key"][0].Get<std::string>().value() << ", ";
  // if the access path does not meet the requirements, the result's Empty() method would return true.
  assert(wrapper["not_exist_key"][0].Empty() == true);
  assert(wrapper["key"][3].Empty() == true);
  bridge::AsMap(new_root)->Insert("new_key", bp.data("this is bridge"));
  std::cout << wrapper["new_key"].Get<std::string>().value() << std::endl;
  return 0;
}