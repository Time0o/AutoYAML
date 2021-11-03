# AutoYAML

`AutoYAML` is a Clang/LLVM/LibTooling-based program that automatically
generates [yaml-cpp](https://github.com/jbeder/yaml-cpp) conversion code for
C++ record types.

## Usage

As a motivational example, say you are writing a C++ program and want to start
parsing configuration files such as the following (`MyConfiguration.yaml`):

```yaml
a_number: 42

a_list_of_numbers:
  - 1
  - 2
  - 3

a_boolean_flag: true

a_dictionary:
  - a: alpaca
  - b: beaver
  - c: capybara

# ...
```

With AutoYAML you can directly transform this YAML file into an equivalent C++
object. To this end, first define an appropriate C++ record type in some header
file (`MyConfiguration.h`) and mark it as an `AutoYAML` target with the
`AutoYAML()` macro:

```c++
#include "AutoYAML.h"

AutoYAML(struct) MyConfiguration
{
  int a_number;

  std::vector<int> a_list_of_numbers;

  bool a_boolean_flag;

  std::map<std::string, std::string> a_dictionary;

  // ...
};
```

Then run the `AutoYAML` tool over this header file as such:

```
AutoYAML MyConfiguration.h --out-dir $MY_OUTPUT_DIRECTORY --
```

This will generate another header file `MyConfiguration.AutoYAML.h` in
`$MY_OUTPUT_DIRECTORY`. You can then include both these header files into your
program and start parsing configuration files like nobody's business:

```c++
#include <iostream>

#include "yaml-cpp/yaml.h"

#include "MyConfiguration.h"
#include "MyConfiguration.AutoYAML.h"

int main()
{
  auto node { YAML::LoadFile("MyConfiguration.yaml") };

  auto config { node.as<MyConfiguration>() };

  std::cout << config.a_number << '\n';              // 42
  std::cout << config.a_list_of_numbers[0] << '\n';  // 1
  std::cout << config.a_boolean_flag << '\n';        // 1
  std::cout << config.a_dictionary["a"] << '\n';     // "alpaca"

  // ...
}
```

## How to Build

You will need to install Clang/LLVM development files in order to build
`AutoYAML`, how to do this is up to you. See
[here](https://clang.llvm.org/docs/LibTooling.html) for more information on
LibTooling.

To build `AutoYAML`, execute the following commands from the root of the
repository:

```
mkdir build && cd build
cmake .. -DCMAKE_CXX_COMPILER=clang++
cmake --build .
```

This will create the `AutoYAML` executable at the root of the build directory.
You can install it to `${CMAKE_INSTALL_PREFIX}/bin` by running `cmake --install.

## TODO

* More tests
* CI
* Proper CMake integration
