### bridge

A high-performance and concise serialization and deserialization Library

### features

* header-only
* sufficient unit-test
* support pattern matching in code form (use the Pattern object to verify whether the Object meets some constraints)
* support serialization of arbitrary binary data
* convenient access interface ( objectwrapper )
* compile time detection (based on c++20:concept)
* support customization of serialization interface of custom type
* support serialization adapters based on standard containers (only vector and unordered_map up to now)
* support ref-parse mode (avoid the cost of a large number of small object copies by holding the original byte array's pointer)

### requirement
* cpp compiler supporting c++20
* bazel

### example 
```c++
#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include "bridge/object.h"

int main() {
  // Construct the object that need to be serialized
  std::string str("hello world");
  auto v1 = bridge::data_view(std::string_view(str));
  auto v2 = bridge::data((int32_t)32);
  auto arr = bridge::array();
  arr->Insert(std::move(v1));
  arr->Insert(std::move(v2));
  auto root = bridge::map();
  root->Insert("key", std::move(arr));
  /* serialize :
     {
       "key" : [
        "hello world",
        32,
       ],
     }
  */
  std::string tmp = Serialize(std::move(root));
  // deserialize (ref-parse mode)
  auto new_root = bridge::Parse(tmp, true);
  // access new_root through ObjectWrapper proxy
  bridge::ObjectWrapper wrapper(new_root.get());
  std::cout << wrapper["key"][0].GetView().value() << std::endl;

  tmp = Serialize(std::move(new_root));
  new_root = bridge::Parse(tmp);
  bridge::ObjectWrapper new_wrapper(new_root.get());
  // if the access path does not meet the requirements, the final result's Empty() method return true.
  assert(new_wrapper["not_exist_key"][0].Empty() == true);
  assert(new_wrapper["key"][3].Empty() == true);
  bridge::AsMap(new_root)->Insert("new_key", bridge::data("new_root"));
  std::cout << new_wrapper["new_key"].Get<std::string>().value() << std::endl;
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
