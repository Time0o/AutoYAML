#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"

#include "AutoYAML_example.h"
#include "AutoYAML_example.AutoYAML.h"

using namespace std::literals::chrono_literals;

namespace {

static std::string AutoYAML_example_no_default_YAML {

R"(
s: some string
b: true
i: 42
d: 42
e: E2
v:
  - 1
  - 2
  - 3
l:
  - 4
  - 5
  - 6
m:
  1: 2
  3: 4
  5: 6
n:
  i: 42
sec: 10)"

};

static std::string AutoYAML_example_default_YAML {

AutoYAML_example_no_default_YAML +

R"(
def: 123)"

};

example::AutoYAML_example AutoYAML_example_reference {
  "some string",
  true,
  42,
  42.0,
  example::AutoYAML_example::E::E2,
  std::vector<int>{1, 2, 3},
  std::list<int>{4, 5, 6},
  std::map<int, int>{{1, 2}, {3, 4}, {5, 6}},
  42,
  10s,
  123,
};

}

namespace YAML {

template<typename Rep, typename Period>
struct convert<std::chrono::duration<Rep, Period>>
{
  static Node encode(const std::chrono::duration<Rep, Period> &obj) {
    return Node(obj.count());
  }

  static bool decode(Node const &node, std::chrono::duration<Rep, Period> &obj) {
    obj = std::chrono::duration<Rep, Period> { node.as<std::size_t>() };
    return true;
  }
};

} // end namespace YAML

TEST_CASE("YAML files are correctly decoded", "[decode]")
{
  auto node { YAML::Load(AutoYAML_example_no_default_YAML.substr(1)) };

  auto example { node.as<example::AutoYAML_example>() };

  CHECK(example.s == "some string");
  CHECK(example.b == true);
  CHECK(example.i == 42);
  CHECK(example.d == 42.0);
  CHECK(example.e == example::AutoYAML_example::E::E2);
  CHECK(example.v == std::vector<int>{1, 2, 3});
  CHECK(example.l == std::list<int>{4, 5, 6});
  CHECK(example.m == std::map<int, int>{{1, 2}, {3, 4}, {5, 6}});
  CHECK(example.n.i == 42);
  CHECK(example.sec == 10s);
  CHECK(example.def == 123);
  CHECK(example == AutoYAML_example_reference);
}

TEST_CASE("C++ objects are correctly encoded", "[encode]")
{
  example::AutoYAML_example example;
  example.s = "some string";
  example.b = true;
  example.i = 42;
  example.d = 42.0;
  example.e = example::AutoYAML_example::E::E2;
  example.v = std::vector<int>{1, 2, 3};
  example.l = std::list<int>{4, 5, 6};
  example.m = std::map<int, int>{{1, 2}, {3, 4}, {5, 6}};
  example.n.i = 42;
  example.sec = 10s;

  YAML::Node node { example };

  YAML::Emitter out;
  out << node;

  CHECK(out.c_str() == AutoYAML_example_default_YAML.substr(1));
}
