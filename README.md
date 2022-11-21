### bridge

A high-performance and concise serialization and deserialization Library

[![build_test_benchmark](https://github.com/chloro-pn/bridge/actions/workflows/build_and_test_with_bazel.yaml/badge.svg?branch=master)](https://github.com/chloro-pn/bridge/actions/workflows/build_and_test_with_bazel.yaml/badge.svg?event=push)

### features

* **header-only**
* sufficient unit-test
* support **Parallel Parse** (based on c++20 coroutine and thread-pool)
* support **Pattern Matching** (use the Pattern object to verify whether the Object meets some constraints)
* support serialization of **Arbitrary Binary Data**
* support **Dictionary Encoding**
* convenient access interface (ObjectWrapper)
* support **Serialization Adapters** based on standard containers (only vector and unordered_map up to now)
* support **in-situ Parse**
* support export to readable format

### requirement
* cpp compiler supporting c++20 (like g++-11)
* bazel 5.2.0

### code distribution
```
    46 ./include/bridge/inner.h
    62 ./include/bridge/adaptor.h
    63 ./include/bridge/util.h
   141 ./include/bridge/serialize.h
   136 ./include/bridge/parse.h
    87 ./include/bridge/async_executor/thread_pool.h
    43 ./include/bridge/async_executor/executor.h
  1832 ./include/bridge/object.h
   164 ./include/bridge/type_trait.h
   106 ./include/bridge/data_type.h
   135 ./include/bridge/variant.h
    37 ./include/bridge/object_type.h
    63 ./include/bridge/varint.h
    88 ./include/bridge/string_map.h
   132 ./include/bridge/object_pool.h
   190 ./include/bridge/pattern.h
    83 ./include/bridge/split_info.h
  3408 总用量

  182 ./benchmark/bench_mark.cc
   75 ./example/string_map.cc
   32 ./example/adaptor.cc
  215 ./example/coroutine_parse.cc
   45 ./example/main.cc
   14 ./test/thread_pool_test.cc
  139 ./test/object_test.cc
   13 ./test/json_test.cc
   17 ./test/inner_test.cc
   42 ./test/dump_test.cc
   67 ./test/type_trait_test.cc
  102 ./test/wrapper_test.cc
   23 ./test/object_pool_test.cc
   52 ./test/async_simple_test.cc
   46 ./test/adaptor_test.cc
  100 ./test/variant_test.cc
   82 ./test/pattern_test.cc
   45 ./test/parse_and_seri_test.cc
   39 ./test/secondary_struct_test.cc
   20 ./test/endian_test.cc
  1350 总用量
```
### benchmark
![image](https://github.com/chloro-pn/bridge/blob/master/png/benchmark.PNG)



### example 
```c++
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
```

### supported types
* bridge_built_in_type:
  * uint32_t、int32_t、uint64_t、int64_t
  * std::string、std::vector\<char\>、float、double、char(&)[n]、const char(&)[n]
* bridge_custom_type:
  * any type that implements concept bridge_custom_type (look bridge/type_trait.h for details)
* bridge_adaptor_type:
  * std::vector\<T\>、std::unordered_map<std::string, T>、std::unordered_map<std::string_view, T> (where T is the type listed in "supported types")
