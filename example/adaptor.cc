#include "bridge/adaptor.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "bridge/object.h"

using namespace bridge;

/*
 * 这个例子展示了使用bridge::adaptor序列化的能力:
 * bridge支持数种标准库容器的任意嵌套（vector、unordered_map<string, T>、unordered_map<string_view, T>)
 * 这里的T可以是上述三种类型之一，也可以是满足 concept bridge_type 的类型（见include/bridge/type_trait.h)
 */

int main() {
  BridgePool bp;
  std::unordered_map<std::string, std::vector<uint32_t>> id_map;
  id_map["row1"] = {0, 1, 2};
  id_map["row2"] = {3, 4, 5};
  id_map["row3"] = {6, 7, 8};
  auto v = adaptor(id_map, bp);
  std::string buf = Serialize(std::move(v), bp);
  // ...
  auto root = Parse(buf, bp);
  ObjectWrapper w(root.get());
  auto it = w.GetIteraotr().value();
  for (; it.Valid(); ++it) {
    std::cout << it.GetKey() << " ";
    auto arr = it.GetValue();
    for (int j = 0; j < arr.Size(); ++j) {
      std::cout << arr[j].Get<uint32_t>().value() << " ";
    }
    std::cout << std::endl;
  }
  return 0;
}