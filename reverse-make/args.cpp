#include "reverse-make/args.h"

#include <stdexcept>

#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>

std::variant<Args, int> Args::ParseArgs(int argc, const char* argv[]) {
  Args args;

  CLI::App app{
      "reverse-make: partially generate makefiles from build logs."};
  args.filename_ = "input.td";
  app.add_option("1, -f,--file", args.filename_, "The input file.");

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    return (app).exit(e);
  }

  return args;
}
