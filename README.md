### bridge

A high-performance and concise serialization and deserialization Library

### features

* **header-only**
* sufficient unit-test
* support **Pattern Matching** (use the Pattern object to verify whether the Object meets some constraints)
* support serialization of **Arbitrary Binary Data**
* support **Dictionary Encoding**
* convenient access interface ( ObjectWrapper )
* support **Serialization Adapters** based on standard containers (only vector and unordered_map up to now)
* support **in-situ Parse**
* support export to readable format

### requirement
* cpp compiler supporting c++20 (like g++-11)
* bazel 5.2.0

### example 
```c++
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "bridge/object.h"
#include "bridge/adaptor.h"

using namespace bridge;

int main() {
  std::unordered_map<std::string, std::vector<uint32_t>> id_map;
  id_map["row1"] = {0, 1, 2};
  id_map["row2"] = {3, 4, 5};
  id_map["row3"] = {6, 7, 8};
  auto v = adaptor(id_map);
  std::string buf = Serialize<SeriType::NORMAL>(std::move(v));
  // ...
  auto root = Parse(buf, false);
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
```

### supported types
* bridge_data_type:
  * uint32_t、int32_t、uint64_t、int64_t
  * std::string、std::vector\<char\>、float、double、char(&)[n]、const char(&)[n]
* bridge_custom_type:
  * any type that implements concept bridge_custom_type (look bridge/type_trait.h for details)
* bridge_adaptor_type:
  * std::vector\<T\>、std::unordered_map<std::string, T>、std::unordered_map<std::string_view, T> (where T is the type listed in "supported types")
