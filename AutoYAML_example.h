#pragma once

#include <list>
#include <map>
#include <string>
#include <vector>

#include "AutoYAML.h"

AutoYAML(struct) AutoYAML_example
{
  // strings
  std::string s;

  // integer types
  bool b;
  int i;
  double d;

  // enums
  AutoYAML(enum class) E
  {
    E1,
    E2,
    E3
  };

  E e;

  // container types
  std::vector<int> v;
  std::list<int> l;
  std::map<int, int> m;

  // nested records
  AutoYAML(struct) Nested
  {
    int i;
  };

  Nested n;

  // default values
  int i_default = 123;
};
