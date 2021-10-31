#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"

#include "AutoYAML_example.h"
#include "AutoYAML_example.AutoYAML.h"

namespace {

static std::string const AutoYAML_example_YAML {

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
)"

};

}

TEST_CASE("YAML files are correctly decoded", "[decode]")
{
  auto node { YAML::Load(AutoYAML_example_YAML) };

  auto example { node.as<AutoYAML_example>() };

  CHECK(example.s == "some string");
  CHECK(example.b == true);
  CHECK(example.i == 42);
  CHECK(example.d == 42.0);
  CHECK(example.e == AutoYAML_example::E::E2);
  CHECK(example.v == std::vector<int>{1, 2, 3});
  CHECK(example.l == std::list<int>{4, 5, 6});
  CHECK(example.m == std::map<int, int>{{1, 2}, {3, 4}, {5, 6}});
  CHECK(example.n.i == 42);
}

TEST_CASE("C++ objects are correctly encoded", "[encode]")
{
  AutoYAML_example example;
  example.s = "some string";
  example.b = true;
  example.i = 42;
  example.d = 42.0;
  example.e = AutoYAML_example::E::E2;
  example.v = std::vector<int>{1, 2, 3};
  example.l = std::list<int>{4, 5, 6};
  example.m = std::map<int, int>{{1, 2}, {3, 4}, {5, 6}};
  example.n.i = 42;

  YAML::Node node { example };

  YAML::Emitter out;
  out << node;

  CHECK(out.c_str() == AutoYAML_example_YAML.substr(1, AutoYAML_example_YAML.size() - 2));
}
