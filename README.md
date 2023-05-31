# reverse-make

[![C/C++ Build (Linux)](https://github.com/dpemmons/reverse-make/actions/workflows/c-cpp-linux.yml/badge.svg)](https://github.com/dpemmons/reverse-make/actions/workflows/c-cpp-linux.yml)
[![C/C++ Build (MacOS)](https://github.com/dpemmons/reverse-make/actions/workflows/c-cpp-macos.yml/badge.svg)](https://github.com/dpemmons/reverse-make/actions/workflows/c-cpp-macos.yml)

`reverse-make` is a utility that takes a log of `gcc`, `g++`, and `ar` build commands as input and reconstructs the dependencies of built libraries. This utility could be useful for developers migrating to a new build system, analyzing the dependencies in their current build, or automating parts of their build migration process.

This utility is not broadly tested. See [Limitations](#limitations).

## Building and Running

1. Clone the repository:

   ```bash
   git clone https://github.com/dpemmons/reverse-make.git
   ```

2. Navigate to the project directory:

   ```bash
   cd reverse-make
   ```

3. Build the project:

   ```bash
   make
   ```

   By default, this generates a debug build which ends up in `./build/debug/reverse-make`. To do an optimized build, run:

   ```bash
   make BUILD=release
   ```

   This results in `build/release/reverse-make`.

4. Run the utility with your build log as input:

   ```bash
   ./build/debug/reverse-make <your-build-log.txt>
   ```

   or

   ```bash
   ./build/release/reverse-make <your-build-log.txt>
   ```

   Replace `<your-build-log.txt>` with the path to your build log.

Note: Your build log might need some cleanup before running.

## Examples

| Input | Output |
|-------|--------|
| [libuv.input.log](./examples/libuv.input.log) | [libuv.output.txt](./examples/libuv.output.txt) |

## Limitations

* Only tested on Ubuntu 22.10; compatibility with other systems is unknown.
* Only tested with one build log so far (see `./examples`).
* This program assumes that the input file is correctly formatted with one command per line.
* Only `gcc`, `g++` and `ar` commands are supported. Other command types are ignored.
* The only form of `ar` command supported is `ar cr <inputs...> <output>`.
* If a command is not recognized or a command is in an unsupported format, the program will abort.

## Code Structure

It's pretty hacky. The code consists of several key components:

1. Data structures for storing command details, such as `GccCommand` and `ArCommand`.
2. Functions for processing different types of commands, like `process_gcc_command` and `process_ar_command`.
3. The `find_deps` function, which groups and lists the dependencies based on their compile flags.
4. The main function, which orchestrates the reading of input file, processing of commands, and calling the `find_deps` function.

## Contributions

Contributions are welcome! Please submit a Pull Request or open an Issue if you find a bug or want to suggest a feature enhancement. To aid in understanding and reproducing any issues, please include example input with any bug report or pull request.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
