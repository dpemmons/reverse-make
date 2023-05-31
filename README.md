# reverse-make

`reverse-make` is a C++ utility that analyzes `gcc`, `g++`, and `ar` compilation and linking commands. Its purpose is to take a log of build commands and reconstruct the dependencies of built libraries. This is useful when transitioning to declarative build systems such as Bazel or Gyp.

This utility is not broadly tested. See [Limitations](#limitations).

## Program Features

* Parses and processes gcc, g++, and ar commands from an input file.
* Groups dependencies based on their compile flags.
* Lists the groups of dependencies and their corresponding flags.
* Facilitates the construction of build configurations in other build systems.

## Usage

This program expects the commands to be present in a file. The file path should be passed as a command line argument.

### Compilation

The program is compiled using the provided Makefile. You can compile it on a Linux system by running:

```bash
make
```

This will generate a debug build of the `reverse-make` executable, which you can find under `./build/debug/reverse-make`.

If you want to generate an optimized release build, you can do so by specifying the `BUILD` variable:

```bash
make BUILD=release
```

This will place the `reverse-make` executable in the `build/release` directory.

### Running

After successful compilation, the program can be run using the following command:

```bash
./build/debug/reverse-make input_commands.txt
```

or for the release build:

```bash
./build/release/reverse-make input_commands.txt
```

Here, `input_commands.txt` is the input file containing the gcc, g++, and ar commands.

Sample input files can be found in the `examples/` directory.

Please ensure to clean up your compile log to meet the acceptable format before running the program.

## Examples

```bash
./build/debug/reverse-make examples/libuv.input.log > examples/libuv.output.txt
```

See [examples/libuv.input.log](./examples/libuv.input.log) and [examples/libuv.output.txt](./examples/libuv.output.txt)

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
