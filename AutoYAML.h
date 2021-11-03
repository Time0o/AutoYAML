#pragma once

#include <unordered_set>

#include "yaml-cpp/yaml.h"

#define AutoYAML(record) record __attribute__((annotate("AutoYAML")))

namespace YAML {

struct AutoYAMLException : public Exception
{
  AutoYAMLException(std::string const &msg)
  : Exception(Mark::null_mark(), msg)
  {}
};

void check_node(Node const &node)
{
  if (!node.IsMap())
    throw AutoYAMLException("input node must be a map");
}

void check_node_properties(Node const &node,
                           std::unordered_set<std::string> props)
{
  for (YAML::const_iterator it { node.begin() }; it != node.end(); ++it) {
    auto prop { it->first.as<std::string>() };

    if (props.find(prop) == props.end())
      throw AutoYAMLException("input node has unexpected property '" + prop + "'");
  }
}

template<typename T>
void set_field(T &field, Node const &node, char const *prop)
{
  field = node[prop].as<T>();
}

template<typename T>
void set_optional_field(T &field, Node const &node, char const *prop)
{
  if (node[prop])
    set_field(field, node, prop);
}

} // end namespace YAML
