#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#define FMT_HEADER_ONLY
#include <fmt/core.h>
#include <fmt/ostream.h>

#include "reverse-make/args.h"
#include "reverse-make/commands.h"

using namespace std;

vector<string> split_unescaped_newlines(const string& str) {
  vector<string> result;
  string temp;
  bool is_escaped = false;

  for (const char& c : str) {
    if (c == '\n' && !is_escaped) {
      result.push_back(temp);
      temp.clear();
    } else if (c == '\\') {
      if (is_escaped) {  // means '\\' is found
        temp += c;
        is_escaped = false;
      } else {
        is_escaped = true;
      }
    } else {
      if (is_escaped) {
        temp += '\\';
        is_escaped = false;
      }
      temp += c;
    }
  }
  result.push_back(temp);  // push the last part of the string

  return result;
}

vector<string> split_string_into_parts(const string& str) {
  vector<string> result;
  string arg;
  bool in_quote = false;
  bool is_escaped = false;

  for (const char& c : str) {
    if (c == '"' && !is_escaped) {
      in_quote = !in_quote;
      if (!in_quote && !arg.empty()) {  // end of a quoted string
        result.push_back(arg);
        arg.clear();
      }
    } else if (c == '\\' && !is_escaped) {  // start of an escape sequence
      is_escaped = true;
    } else if (c == ' ' && !in_quote) {  // a space outside of a quoted string
      if (!arg.empty()) {
        result.push_back(arg);
        arg.clear();
      }
      is_escaped = false;
    } else {  // a regular character, or a space inside a quoted string
      if (is_escaped &&
          c != '"') {  // add the escape character if it's not for a quote
        arg += '\\';
      }
      arg += c;
      is_escaped = false;
    }
  }
  // add the last argument if it's not empty
  if (!arg.empty()) {
    result.push_back(arg);
  }

  // Strip out the outer quotes
  for (auto& argument : result) {
    if (argument.front() == '"' && argument.back() == '"') {
      argument.erase(0, 1);  // remove first character
      argument.pop_back();   // remove last character
    }
  }

  return result;
}

bool startsWith(const std::string& str, const std::string& prefix) {
  return str.size() >= prefix.size() &&
         str.compare(0, prefix.size(), prefix) == 0;
}

/*
  https://man7.org/linux/man-pages/man1/gcc.1.html
       gcc [-c|-S|-E] [-std=standard]
           [-g] [-pg] [-Olevel]
           [-Wwarn...] [-Wpedantic]
           [-Idir...] [-Ldir...]
           [-Dmacro[=defn]...] [-Umacro]
           [-foption...] [-mmachine-option...]
           [-o outfile] [@file] infile...
*/
unique_ptr<GccCommand> process_gcc_command(const vector<string>& parts) {
  auto gcc_command = make_unique<GccCommand>();

  if (parts[0].compare("gcc")) {
    gcc_command->compiler = GccCommand::GCC;
  } else if (parts[0].compare("g++")) {
    gcc_command->compiler = GccCommand::GPP;
  } else {
    fmt::print("Unsupported command: {}\n", parts[0]);
    abort();
  }

  // default unless -c, -S, or -E
  gcc_command->command = GccCommand::LINK;

  for (int i = 1; i < parts.size(); i++) {
    if (parts[i].compare("-c") == 0) {
      gcc_command->command = GccCommand::LINK;
    } else if (parts[i].compare("-S") == 0) {
      gcc_command->command = GccCommand::COMPILE_NO_ASSEMBLE;
    } else if (parts[i].compare("-E") == 0) {
      gcc_command->command = GccCommand::PREPROCESS_ONLY;

    // TODO check other flags (go through the man page...)

    } else if (startsWith(parts[i], "-D")) {
      // defines
      gcc_command->defines.insert(parts[i]);
    } else if (startsWith(parts[i], "-I")) {
      // includes
      gcc_command->includes.insert(parts[i]);
    } else if (startsWith(parts[i], "-f")) {
      // clfags
      gcc_command->cflags.insert(parts[i]);
    } else if (startsWith(parts[i], "-W")) {
      // warns
      gcc_command->warns.insert(parts[i]);
    } else if (startsWith(parts[i], "-m")) {
      // target flags
      gcc_command->optimizations.insert(parts[i]);
    } else if (startsWith(parts[i], "-O")) {
      // optimizations
      gcc_command->optimizations.insert(parts[i]);
    } else if (startsWith(parts[i], "-L")) {
      // linker search directories
      gcc_command->link_search_dirs.insert(parts[i]);
    } else if (startsWith(parts[i], "-std")) {
      gcc_command->cflags.insert(parts[i]);
    } else if (parts[i].compare("-pthread") == 0) {
      gcc_command->linkopts.insert(parts[i]);
    } else if (startsWith(parts[i], "-shared") == 0) {
      gcc_command->linkopts.insert(parts[i]);
    } else if (startsWith(parts[i], "-static") == 0) {
      gcc_command->linkopts.insert(parts[i]);
    } else if (startsWith(parts[i], "-g")) {
      gcc_command->cflags.insert(parts[i]);
    } else if (startsWith(parts[i], "-MT") || startsWith(parts[i], "-MQ") ||
               startsWith(parts[i], "-MF")) {
      // also skip the target
      i++;
    } else if (startsWith(parts[i], "-M")) {
      // skip the other dependency generation rules
    } else if (parts[i].compare("-o") == 0) {
      if (i + 1 == parts.size()) {
        fmt::print("No argument after '-o'\n");
        abort();
      }
      gcc_command->output = parts[i++];
    } else if (startsWith(parts[i], ">") || parts[i].compare("2>&1") == 0) {
      // ignore output redirect
    } else {
      gcc_command->inputs.insert(parts[i]);
    }
  }
  return move(gcc_command);
}

unique_ptr<ArCommand> process_ar_command(const vector<string>& parts) {
  auto ar_command = make_unique<ArCommand>();
  return move(ar_command);
}

int main(int argc, const char** argv) {
  // Parse args, or die trying.
  auto maybe_args = Args::ParseArgs(argc, argv);
  if (maybe_args.index() == 1) {
    return get<int>(maybe_args);
  }
  Args args = get<Args>(maybe_args);

  ifstream input_stream(args.getInpuFilename());
  if (!input_stream.is_open()) {
    fmt::print("Unable to open file: {}\n", args.getInpuFilename());
    return 1;
  }

  stringstream buffer;
  buffer << input_stream.rdbuf();
  string file = buffer.str();

  auto commands = split_unescaped_newlines(file);
  int line = 0;
  for (auto command : commands) {
    auto parts = split_string_into_parts(command);
    if (parts.size() > 0) {
      if (parts[0].compare("gcc") == 0) {
        auto c = process_gcc_command(parts);
      } else if (parts[0].compare("g++") == 0) {
        auto c = process_gcc_command(parts);
      } else if (parts[0].compare("ar") == 0) {
        process_ar_command(parts);
      } else {
        fmt::print("Skipping unrecognized command {}\n", parts[0]);
      }
    } else {
      // skip.
      fmt::print("Skipping empty line {}\n", line);
    }
    line++;
  }

  // auto commands = process_file(input_stream);
  input_stream.close();

  return 0;
}