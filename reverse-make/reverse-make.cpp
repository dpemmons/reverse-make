#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#define FMT_HEADER_ONLY
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include "reverse-make/args.h"
#include "reverse-make/commands.h"

using namespace std;

/* Make filesystem::path formattable.
 */
template <>
struct fmt::formatter<filesystem::path> : fmt::formatter<string_view> {
  template <typename FormatContext>
  auto format(const filesystem::path& path, FormatContext& ctx) {
    return fmt::formatter<string_view>::format(path.string(), ctx);
  }
};

template <>
struct fmt::formatter<vector<filesystem::path>> {
  // parse is trivial and doesn't require anything
  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  // format prints each path using your custom formatter
  template <typename FormatContext>
  auto format(const vector<filesystem::path>& paths, FormatContext& ctx) {
    // start with the first path
    auto it = paths.begin();

    // if the vector is not empty, print the first path
    if (it != paths.end()) {
      fmt::format_to(ctx.out(), "{}", it->string());
      ++it;
    }

    // print the rest of the paths, prefixed with ", "
    for (; it != paths.end(); ++it) {
      fmt::format_to(ctx.out(), ", {}", it->string());
    }

    return ctx.out();
  }
};

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

bool startsWith(const string& str, const string& prefix) {
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
shared_ptr<GccCommand> process_gcc_command(const vector<string>& parts) {
  auto gcc_command = make_shared<GccCommand>();

  if (parts[0] == "gcc") {
    gcc_command->compiler = GccCommand::GCC;
  } else if (parts[0] == "g++") {
    gcc_command->compiler = GccCommand::GPP;
  } else {
    fmt::print("Unsupported command: {}\n", parts[0]);
    abort();
  }

  // default unless -c, -S, or -E
  gcc_command->command = GccCommand::LINK;

  for (int i = 1; i < parts.size(); i++) {
    // note I'm completely ignoring "GCC Developer Options"
    if (parts[i] == "-c") {
      gcc_command->command = GccCommand::COMPILE;
    } else if (parts[i] == "-S") {
      gcc_command->command = GccCommand::COMPILE_NO_ASSEMBLE;
    } else if (parts[i] == "-E") {
      gcc_command->command = GccCommand::PREPROCESS_ONLY;

      // TODO check other flags (go through the man page...)

    } else if (parts[i] == "-D") {
      // but we *should* handle these! just need some examples...
      fmt::print("Unhandled argument type: \"{}\"\n", parts[i]);
      abort();
    } else if (startsWith(parts[i], "-D")) {
      // defines
      gcc_command->defines.insert(parts[i]);
    } else if (startsWith(parts[i], "-I")) {
      // includes
      gcc_command->includes.insert(parts[i]);
    } else if (startsWith(parts[i], "-fuse")) {
      if (i + 1 == parts.size()) {
        fmt::print("No argument after '-fuse'\n");
        abort();
      }
      // pass the whole thing.
      gcc_command->linkopts.insert(fmt::format("{} {}", parts[i], parts[++i]));
    } else if (startsWith(parts[i], "-f") || parts[i] == "-p" ||
               parts[i] == "-pg" || parts[i] == "--coverage" ||
               parts[i] == "-undef") {
      // clfags
      gcc_command->cflags.insert(parts[i]);
    } else if (startsWith(parts[i], "-W") || parts[i] == "-w" ||
               parts[i] == "-pedantic" || parts[i] == "-pedantic-errors") {
      // warns
      gcc_command->warns.insert(parts[i]);
    } else if (startsWith(parts[i], "-m")) {
      // target flags
      gcc_command->target_opts.insert(parts[i]);
    } else if (startsWith(parts[i], "-O")) {
      // optimizations
      gcc_command->optimizations.insert(parts[i]);
    } else if (startsWith(parts[i], "-L")) {
      // linker search directories
      gcc_command->link_search_dirs.insert(parts[i]);
    } else if (parts[i] == "-lobj" || parts[i] == "-nodefaultlibs" ||
               parts[i] == "-nolibc" || parts[i] == "-nodefaultlibs" ||
               parts[i] == "-nostdlib" || parts[i] == "-pie" ||
               parts[i] == "-no-pie" || parts[i] == "-static-pie" ||
               parts[i] == "-pthread" || parts[i] == "-r" ||
               parts[i] == "-rdynamic" || parts[i] == "-s" ||
               startsWith(parts[i], "-shared") ||
               startsWith(parts[i], "-static") || parts[i] == "-symbolic" ||
               false) {
      gcc_command->linkopts.insert(parts[i]);

    } else if (parts[i] == "-Xlinker") {
      if (i + 1 == parts.size()) {
        fmt::print("No argument after '-Xlinker'\n");
        abort();
      }
      // pass the whole thing.
      gcc_command->linkopts.insert(fmt::format("{} {}", parts[i], parts[++i]));
    } else if (parts[i] == "-l") {
      if (i + 1 == parts.size()) {
        fmt::print("No argument after '-l'\n");
        abort();
      }
      // pass the whole thing.
      gcc_command->link_libs.insert(fmt::format("{} {}", parts[i], parts[++i]));
    } else if (startsWith(parts[i], "-l")) {
      // Note that we're losing positional information. According to the gcc man
      // file:
      // "It makes a difference where in the command you write this option;
      // the linker searches and processes libraries and object files in the
      // order they are specified."
      // Unclear to me how much of a problem this is in practice.
      gcc_command->link_libs.insert(parts[i]);
    } else if (startsWith(parts[i], "-std") || parts[i] == "-ansi") {
      gcc_command->cflags.insert(parts[i]);
    } else if (startsWith(parts[i], "-g")) {
      gcc_command->debug.insert(parts[i]);
    } else if (startsWith(parts[i], "-MT") || startsWith(parts[i], "-MQ") ||
               startsWith(parts[i], "-MF")) {
      // also skip the target
      i++;
    } else if (startsWith(parts[i], "-M")) {
      // skip the other dependency generation rules
    } else if (parts[i] == "-v" || parts[i] == "-###" || parts[i] == "-pipe") {
      // skip
    } else if (startsWith(parts[i], "-x") || parts[i] == "--version" ||
               parts[i] == "-pass-exit-codes" ||
               startsWith(parts[i], "--help") ||
               startsWith(parts[i], "--target-help") ||
               startsWith(parts[i], "-specs") || parts[i] == "-wrapper" ||
               startsWith(parts[i], "@") || parts[i] == "-aux-info" ||
               parts[i] == "-gen-decls" ||
               parts[i] == "-print-objc-runtime-info" ||
               parts[i] == "--param" || parts[i] == "-include" ||
               parts[i] == "-imacros" || parts[i] == "-A" || parts[i] == "-C" ||
               parts[i] == "-CC" || parts[i] == "-P" ||
               parts[i] == "-traditional" || parts[i] == "-traditional-cpp" ||
               parts[i] == "-trigraphs" || parts[i] == "-remap" ||
               parts[i] == "-H" || startsWith(parts[i], "-d") ||
               parts[i] == "-Xpreprocessor" ||
               parts[i] == "-no-integrated-cpp" || parts[i] == "-Xassembler" ||
               parts[i] == "-T" || parts[i] == "-e" ||
               startsWith(parts[i], "--entry") || parts[i] == "-u" ||
               parts[i] == "-z" || parts[i] == "-iquote" ||
               parts[i] == "-isystem" || parts[i] == "-idirafter" ||
               parts[i] == "-I-" || parts[i] == "-iprefix" ||
               parts[i] == "-iwithprefix" || parts[i] == "-iwithprefixbefore" ||
               parts[i] == "-isysroot" || parts[i] == "-imultilib" ||
               parts[i] == "-nostdinc" || parts[i] == "-nostdinc++" ||
               startsWith(parts[i], "-iplugindir") ||
               startsWith(parts[i], "-B") ||
               parts[i] == "-no-canonical-prefixes" ||
               startsWith(parts[i], "--sysroot") ||
               parts[i] == "--no-sysroot-suffix") {
      fmt::print("Unhandled argument type: \"{}\"\n", parts[i]);
      abort();

    } else if (parts[i] == "-o") {
      if (i + 1 == parts.size()) {
        fmt::print("No argument after '-o'\n");
        abort();
      }
      gcc_command->output = parts[++i];
    } else if (startsWith(parts[i], ">") || parts[i] == "2>&1") {
      // ignore output redirect
    } else {
      gcc_command->inputs.push_back(parts[i]);
    }
  }
  return move(gcc_command);
}

shared_ptr<ArCommand> process_ar_command(const vector<string>& parts) {
  auto ar_command = make_shared<ArCommand>();

  if (parts.size() < 4 || parts[1] != "cr") {
    fmt::print(
        "Only form of `ar` command suported is `ar cr <inputs...> <output>\n");
    abort();
  }
  ar_command->output = parts[2];
  for (int i = 3; i < parts.size(); i++) {
    ar_command->inputs.push_back(parts[i]);
  }

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
  vector<shared_ptr<GccCommand>> gcc_compile_commands;
  vector<shared_ptr<GccCommand>> gcc_link_commands;
  vector<shared_ptr<ArCommand>> ar_commands;
  for (auto command : commands) {
    auto parts = split_string_into_parts(command);
    if (parts.size() > 0) {
      if (parts[0] == "gcc" || parts[0] == "g++") {
        auto c = process_gcc_command(parts);
        if (c->command == GccCommand::COMPILE) {
          gcc_compile_commands.push_back(c);
        } else if (c->command == GccCommand::LINK) {
          gcc_link_commands.push_back(c);
        } else {
          fmt::print("Unsupported or unknown gcc/g++ command type.");
          abort();
        }
      } else if (parts[0] == "ar") {
        ar_commands.push_back(process_ar_command(parts));
      } else {
        fmt::print("Skipping unrecognized command {}\n", parts[0]);
      }
    } else {
      // skip.
      fmt::print("Skipping empty line {}\n", line);
    }
    line++;
  }

  for (auto gcc_command : gcc_compile_commands) {
    fmt::print("gcc build: {} -> {}\n", gcc_command->inputs[0],
               gcc_command->output);
  }

  for (auto gcc_command : gcc_link_commands) {
    fmt::print("gcc link: [{}] -> {}\n", gcc_command->inputs,
               gcc_command->output);
  }

  for (auto ar_command : ar_commands) {
    fmt::print("ar archive: [{}] -> {}\n", ar_command->inputs,
               ar_command->output);
  }

  // TODO: print a suggested dependency tree.

  input_stream.close();

  return 0;
}
