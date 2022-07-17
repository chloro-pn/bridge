#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include "bridge/object.h"

using namespace bridge;

int main() {
  // Construct the object that need to be serialized
  std::unique_ptr<Data> v1 = ValueFactory<Data>("hello world");
  std::unique_ptr<Data> v2 = ValueFactory<Data>((int32_t)32);
  std::unique_ptr<Array> arr = ValueFactory<Array>();
  arr->Insert(std::move(v1));
  arr->Insert(std::move(v2));
  std::unique_ptr<Map> root = ValueFactory<Map>();
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
  auto new_root = Parse(tmp, true);
  // access new_root through ObjectWrapper proxy
  ObjectWrapper wrapper(new_root.get());
  std::cout << wrapper["key"][0].GetView().value() << std::endl;

  tmp = Serialize(std::move(new_root));
  new_root = Parse(tmp);
  ObjectWrapper new_wrapper(new_root.get());
  // if the access path does not meet the requirements, the final result's Empty() method return true.
  assert(new_wrapper["not_exist_key"][0].Empty() == true);
  assert(new_wrapper["key"][3].Empty() == true);
  AsMap(new_root)->Insert("new_key", ValueFactory<Data>("new_root"));
  std::cout << new_wrapper["new_key"].Get<std::string>().value() << std::endl;
  return 0;
}