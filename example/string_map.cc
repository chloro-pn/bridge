#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "bridge/adaptor.h"
#include "bridge/object.h"
#include "nlohmann/json.hpp"

using namespace bridge;

int main() {
  std::vector<std::unordered_map<std::string, std::string>> info;
  std::vector<std::string> methods = {
      "get_file_from_db",
      "update_timestamp",
      "post_to_db",
      "delete_by_timestamp",
  };

  std::vector<std::string> request_user = {
      "Barbara",
      "Elizabeth",
      "Katharine",
  };
  
  nlohmann::json j;
  for (int i = 0; i < 10000; ++i) {
    std::unordered_map<std::string, std::string> request_info;
    request_info[methods[i % methods.size()]] = request_user[i % request_user.size()];
    info.push_back(request_info);
    nlohmann::json node;
    node[methods[i % methods.size()]] = request_user[i % request_user.size()];
    j.push_back(std::move(node));
  }
  auto content_json = j.dump();
  std::cout << "serialize with json use " << content_json.size() << " bytes" << std::endl;
  auto v = adaptor(info);
  auto content1 = Serialize(std::move(v));
  std::cout << "serialize without string_map use " << content1.size() << " bytes" << std::endl;

  auto v2 = adaptor(info);
  auto content2 = Serialize<SeriType::REPLACE>(std::move(v2));
  std::cout << "serialize with string_map use " << content2.size() << " bytes" << std::endl;

  std::cout << "json / string_map == " << double(content_json.size()) / content2.size() << std::endl;

  std::cout << "no_string_map / string_map == " << double(content1.size()) / content2.size() << std::endl;

  auto root = Parse(content1);
  auto root2 = Parse(content2);
  ObjectWrapper w1(root.get());
  ObjectWrapper w2(root2.get());
  if (w1.Size() != w2.Size()) {
    std::cerr << "check error !" << std::endl;
    exit(-1);
  }
  for (int i = 0; i < w1.Size(); ++i) {
    auto it = w1[i].GetIteraotr().value();
    auto it2 = w2[i].GetIteraotr().value();
    if (it.GetKey() != it2.GetKey() ||
        it.GetValue().Get<std::string>().value() != it2.GetValue().Get<std::string>().value()) {
      std::cerr << "check error !" << std::endl;
      exit(-1);
    }
    ++it;
    ++it2;
    if (it.Valid() || it2.Valid()) {
      std::cerr << "check error !" << std::endl;
      exit(-1);
    }
  }
  std::cout << "check succ !" << std::endl;
  return 0;
}