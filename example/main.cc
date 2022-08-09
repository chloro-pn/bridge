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
  std::string buf;
  root->dump(buf, 0);
  std::cout << "--- dump --- " << std::endl << buf << std::endl << "------------ " << std::endl;
  std::string tmp = Serialize(std::move(root));
  // deserialize (ref-parse mode)
  auto new_root = bridge::Parse(tmp, false);
  // access new_root through ObjectWrapper proxy
  bridge::ObjectWrapper wrapper(new_root.get());
  std::cout << wrapper["key"][0].Get<std::string>().value() << ", ";

  tmp = Serialize(std::move(new_root));
  new_root = bridge::Parse(tmp);
  bridge::ObjectWrapper new_wrapper(new_root.get());
  // if the access path does not meet the requirements, the final result's Empty() method return true.
  assert(new_wrapper["not_exist_key"][0].Empty() == true);
  assert(new_wrapper["key"][3].Empty() == true);
  bridge::AsMap(new_root)->Insert("new_key", bridge::data("this is bridge"));
  std::cout << new_wrapper["new_key"].Get<std::string>().value() << std::endl;
  return 0;
}