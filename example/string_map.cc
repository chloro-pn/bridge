#include "bridge/adaptor.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "bridge/object.h"

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
  for(int i = 0; i < 10000; ++i) {
    std::unordered_map<std::string, std::string> request_info;
    request_info[methods[i % methods.size()]] = request_user[i % request_user.size()];
    info.push_back(request_info);
  }
  auto v = adaptor(info);
  auto content1 = Serialize(std::move(v));
  std::cout << "serialize without string_map use " << content1.size() << " bytes" << std::endl;

  auto v2 = adaptor(info);
  auto content2 = Serialize<SeriType::REPLACE>(std::move(v2));
  std::cout << "serialize with string_map use " << content2.size() << " bytes" << std::endl;

  std::cout << "without string_map / with string_map == " << double(content1.size()) / content2.size() << std::endl;
  return 0;
}