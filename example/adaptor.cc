#include "bridge/adaptor.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "bridge/object.h"

using namespace bridge;

int main() {
  std::unordered_map<std::string, std::vector<uint32_t>> id_map;
  id_map["row1"] = {0, 1, 2};
  id_map["row2"] = {3, 4, 5};
  id_map["row3"] = {6, 7, 8};
  auto v = adaptor(id_map);
  std::string buf = Serialize<SeriType::NORMAL>(std::move(v));
  // ...
  auto root = Parse(buf);
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
  ClearResource();
  return 0;
}