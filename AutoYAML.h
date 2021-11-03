#pragma once

#include "yaml-cpp/yaml.h"

#define AutoYAML(record) record __attribute__((annotate("AutoYAML")))

namespace YAML {

template<typename T>
void set_value(T &prop, Node const &node, char const *prop_name)
{
  prop = node[prop_name].as<T>();
}

template<typename T>
void set_optional_value(T &prop, Node const &node, char const *prop_name)
{
  if (node[prop_name])
    set_value(prop, node, prop_name);
}

} // end namespace YAML
