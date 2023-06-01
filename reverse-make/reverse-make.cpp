#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
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

/**
 * Splits the given string into substrings at newline characters ('\n') that are
 * not escaped by backslashes.
 *
 * This function iterates over each character in the input string, checking if
 * it's a newline character. If it is, and if it is not preceded by a backslash,
 * it's considered as a split point. A sequence of two backslashes before a
 * newline does not count as an escape sequence (i.e., the newline will still be
 * considered as a split point).
 *
 * @param str The string to be split. This string can contain newline characters
 * and backslashes.
 *
 * @return A std::vector of std::string, each containing a substring of 'str'.
 * These substrings are formed by splitting 'str' at each unescaped newline
 * character. If 'str' does not contain any unescaped newline characters, the
 * returned vector will contain one element that is equal to 'str'.
 *
 * @note The escape character (backslash) itself is also escaped. That is, a
 * backslash followed by a non-newline character will result in two backslashes
 * in the resulting string. Also, if 'str' ends with a backslash, that backslash
 * will be duplicated in the last string of the returned vector.
 *
 * Example:
 * split_unescaped_newlines("hello\\nworld\\n") returns {"hello\\nworld\\n"}
 * split_unescaped_newlines("hello\nworld\n") returns {"hello", "world", ""}
 * split_unescaped_newlines("hello\\world") returns {"hello\\world"}
 */
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

/**
 * Splits a given string into multiple parts based on spaces, respecting quoted
 * substrings and escape sequences.
 *
 * This function parses an input string that may contain quoted and/or escaped
 * characters. It divides the string into parts along space characters (' '),
 * except when the space character is within a quoted substring or is escaped by
 * a backslash. A sequence of two backslashes before a space does not count as
 * an escape sequence (i.e., the space will still be considered as a split
 * point). At the end, outer quotes from each part of the string are removed if
 * present.
 *
 * @param str The string to be split. This string can contain space characters,
 * quotation marks and backslashes.
 *
 * @return A std::vector of std::string, each containing a part of 'str'. These
 * parts are formed by splitting 'str' at each space character that is not
 * within quotes and not escaped. If 'str' does not contain any such space
 * characters, the returned vector will contain one element that is equal to
 * 'str' (minus the outer quotes, if they are present).
 *
 * @note The escape character (backslash) itself is also escaped. That is, a
 * backslash followed by a non-space, non-quote character will result in two
 * backslashes in the resulting string. Also, if 'str' ends with a backslash,
 * that backslash will be duplicated in the last string of the returned vector.
 *
 * Example:
 * split_string_into_parts("hello world") returns {"hello", "world"}
 * split_string_into_parts("\"hello world\"") returns {"hello world"}
 * split_string_into_parts("hello\\ world") returns {"hello\\ world"}
 * split_string_into_parts("\"hello\\\" world\"") returns {"hello\" world"}
 */
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

/**
 * Checks if a given string starts with a specific prefix.
 *
 * This function compares the start of the input string 'str' with the provided
 * prefix string. It returns true if the first characters of 'str' are equal to
 * 'prefix', and false otherwise.
 *
 * @param str The string to be checked. This is the string that may or may not
 * start with 'prefix'.
 *
 * @param prefix The prefix string. This function checks if 'str' starts with
 * this string.
 *
 * @return A bool value indicating whether 'str' starts with 'prefix'. If
 * 'prefix' is an empty string, the function returns true.
 *
 * Example:
 * startsWith("hello world", "hello") returns true
 * startsWith("hello world", "world") returns false
 */
bool startsWith(const string& str, const string& prefix) {
  return str.size() >= prefix.size() &&
         str.compare(0, prefix.size(), prefix) == 0;
}

/**
 * Processes a vector of strings containing parts of a GCC or G++ command, and
 * constructs a GccCommand object from them.
 *
 * This function identifies and handles several types of command line options,
 * including (but not limited to):
 * - "-c", "-S", and "-E" to determine the command type;
 * - "-D", "-I", and "-f" to handle definitions, include directories, and some
 * options;
 * - "-W", "-m", "-O", and "-g" to handle warnings, target options,
 * optimizations, and debug options;
 * - "-L", "-l", and "-o" to handle linker options.
 *
 * Unhandled or unrecognised options cause the function to abort and print an
 * error message.
 *
 * @param parts A vector of strings containing parts of a GCC or G++ command.
 * This is usually obtained by splitting the command line at each space
 * character.
 *
 * @return A shared_ptr to a GccCommand object that represents the given
 * command.
 *
 * @note This function assumes that 'parts' is non-empty and that the first
 * element of 'parts' is "gcc" or "g++". It does not check for this, and the
 * behaviour is undefined if this is not the case.
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
      auto this_part = parts[i];
      auto next_part = parts[++i];
      gcc_command->linkopts.insert(fmt::format("{} {}", this_part, next_part));
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
      auto this_part = parts[i];
      auto next_part = parts[++i];
      gcc_command->linkopts.insert(fmt::format("{} {}", this_part, next_part));
    } else if (parts[i] == "-l") {
      if (i + 1 == parts.size()) {
        fmt::print("No argument after '-l'\n");
        abort();
      }
      // pass the whole thing.
      auto this_part = parts[i];
      auto next_part = parts[++i];
      gcc_command->linkopts.insert(fmt::format("{} {}", this_part, next_part));
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
  return std::move(gcc_command);
}

/**
 * Processes a vector of strings containing parts of an 'ar' command, and
 * constructs an ArCommand object from them.
 *
 * This function only supports the form 'ar cr <inputs...> <output>', where
 * '<inputs...>' is one or more input files, and '<output>' is the output file.
 * Other forms of 'ar' commands are not supported and will cause the function to
 * abort and print an error message.
 *
 * @param parts A vector of strings containing parts of an 'ar' command. This is
 * usually obtained by splitting the command line at each space character.
 *
 * @return A shared_ptr to an ArCommand object that represents the given
 * command.
 *
 * @note This function assumes that 'parts' is of the form 'ar cr <inputs...>
 * <output>'. It does not check for this, and the behaviour is undefined if this
 * is not the case.
 */
shared_ptr<ArCommand> process_ar_command(const vector<string>& parts) {
  auto ar_command = make_shared<ArCommand>();

  if (parts.size() < 4 || (parts[1] != "cr" && parts[1] != "rc")) {
    fmt::print(
        "Only form of `ar` command suported is `ar cr|rc <inputs...> "
        "<output>\n");
    abort();
  }
  ar_command->output = parts[2];
  for (int i = 3; i < parts.size(); i++) {
    ar_command->inputs.push_back(parts[i]);
  }

  return std::move(ar_command);
}

/**
 * Iterates through unused inputs from a compilation/linking command, groups
 * them based on the command's flags and prints the groups.
 *
 * The function works in the following steps:
 * 1. For each unused input, it finds its corresponding GCC command and marks
 * the input as used.
 * 2. It then iterates through the other unused inputs to find ones that match
 * the original input's flags.
 * 3. For each match found, it adds the match to a group and marks it as used.
 * 4. After iterating through all unused inputs, it prints each group along with
 * the representative command flags.
 *
 * @note The function assumes that 'unused_dependencies' and
 * 'gcc_compile_commands' have been filled correctly and that
 * 'unused_dependencies' contains all input files that have not been processed
 * yet. The behavior is undefined if this is not the case.
 */
void find_deps(map<string, bool>& unused_dependencies,
               const map<string, shared_ptr<GccCommand>>& gcc_compile_commands,
               const map<string, shared_ptr<GccCommand>>& gcc_link_commands,
               const map<string, shared_ptr<ArCommand>>& ar_commands) {
  // For each dependency...
  struct Group {
    vector<string> sources;
    shared_ptr<GccCommand> example_gcc_command;
  };
  vector<Group> match_groups;
  for (auto& unused_dependency : unused_dependencies) {
    if (unused_dependency.second) {
      continue;
    }

    auto maybe_input = gcc_compile_commands.find(unused_dependency.first);
    if (maybe_input == gcc_compile_commands.end()) {
      // not a source input.
      continue;
    }
    auto input = maybe_input->second;

    // mark it used.
    unused_dependency.second = true;

    // find other dependencies that have the same flags as this one.
    // save off the *input*
    Group group;
    group.sources.push_back(input->inputs[0]);
    group.example_gcc_command = input;

    // Now find others that match it.
    for (auto& maybe_matched_unused_dependency : unused_dependencies) {
      // skip used potential matches.
      if (maybe_matched_unused_dependency.second) {
        continue;
      }

      if (auto maybe_matched_input =
              gcc_compile_commands.find(maybe_matched_unused_dependency.first);
          maybe_matched_input != gcc_compile_commands.end()) {
        if (input->FlagsMatch(*maybe_matched_input->second)) {
          // Match!
          if (maybe_matched_input->second->inputs.size() != 1) {
            fmt::print(
                "Expected matching compile target {} to have one input!\n",
                maybe_matched_input->first);
            abort();
          }

          group.sources.push_back(maybe_matched_input->second->inputs[0]);
          // mark it used
          maybe_matched_unused_dependency.second = true;
        }
      } else if (auto maybe_matched_input = gcc_link_commands.find(
                     maybe_matched_unused_dependency.first);
                 maybe_matched_input != gcc_link_commands.end()) {
        // Link depdency. Mark it used and skip.
        maybe_matched_unused_dependency.second = true;
      } else if (auto maybe_matched_input =
                     ar_commands.find(maybe_matched_unused_dependency.first);
                 maybe_matched_input != ar_commands.end()) {
        // Link depdency. Mark it used and skip.
        maybe_matched_unused_dependency.second = true;
      } else {
        fmt::print("Compilation command for dependency \"{}\" not found.\n",
                   maybe_matched_unused_dependency.first);
        abort();
      }
    }

    match_groups.push_back(group);
  }

  fmt::print("  Found the following group(s) of matching source dependencies:\n");
  int group_num = 0;
  for (auto group : match_groups) {
    if (group.sources.size() == 0) {
      fmt::print("  Group sources is empty!\n");
      continue;
    }

    fmt::print("    Group {} depending on {} source dependencies: {}\n", group_num,
               group.sources.size(), group.sources);
    fmt::print("    Compiled with the following flags:\n");
    auto representative_input = group.example_gcc_command;
    fmt::print("      compiler: {}\n",
               representative_input->CompilerAsString());
    fmt::print("      command: {}\n", representative_input->CommandAsString());
    fmt::print("      defines: {}\n", representative_input->defines);
    fmt::print("      includes: {}\n", representative_input->includes);
    fmt::print("      cflags: {}\n", representative_input->cflags);
    fmt::print("      warns: {}\n", representative_input->warns);
    fmt::print("      target_opts: {}\n", representative_input->target_opts);
    fmt::print("      optimizations: {}\n",
               representative_input->optimizations);
    fmt::print("      debug: {}\n", representative_input->debug);
    fmt::print("      linkopts: {}\n", representative_input->linkopts);
    fmt::print("      link_search_dirs: {}\n",
               representative_input->link_search_dirs);
    fmt::print("      link_libs: {}\n", representative_input->link_libs);

    group_num++;
  }
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
  int line = 1;
  map<string, shared_ptr<GccCommand>> gcc_compile_commands;
  map<string, shared_ptr<GccCommand>> gcc_link_commands;
  map<string, shared_ptr<ArCommand>> ar_commands;
  for (auto command : commands) {
    auto parts = split_string_into_parts(command);
    if (parts.size() > 0) {
      if (parts[0] == "gcc" || parts[0] == "g++") {
        auto c = process_gcc_command(parts);
        if (c->command == GccCommand::COMPILE) {
          gcc_compile_commands.insert(pair(c->output, c));
        } else if (c->command == GccCommand::LINK) {
          gcc_link_commands.insert(pair(c->output, c));
        } else {
          fmt::print("Unsupported or unknown gcc/g++ command type.");
          abort();
        }
      } else if (parts[0] == "ar") {
        auto c = process_ar_command(parts);
        ar_commands.insert(pair(c->output, c));
      } else {
        fmt::print("Skipping unrecognized command \"{}\" on line {}.\n",
                   parts[0], line);
      }
    }
    line++;
  }

  // For each ar link target...
  for (auto ar_command : ar_commands) {
    fmt::print("----------------------------------------------------\n");
    fmt::print("ar archive target: {} has {} dependencies: {}.\n",
               ar_command.second->output, ar_command.second->inputs.size(),
               ar_command.second->inputs);
    fmt::print("----------------------------------------------------\n");

    // Save away all dependencies as keys. We'll use keep track of which
    // dependencies haven't yet been matched to others with the same flags.
    map<string, bool> unused_dependencies;
    for (auto input : ar_command.second->inputs) {
      unused_dependencies.insert(pair(input, false));
    }
    // consumes unused_dependencies.
    find_deps(unused_dependencies, gcc_compile_commands, gcc_link_commands,
              ar_commands);
  }

  // For each gcc link target...
  for (auto gcc_command : gcc_link_commands) {
    fmt::print("----------------------------------------------------\n");
    fmt::print("gcc link target: {} has {} dependencies: {}\n",
               gcc_command.second->output, gcc_command.second->inputs.size(),
               gcc_command.second->inputs);
    fmt::print("----------------------------------------------------\n");

    fmt::print("  Linked with the following flags:\n");
    fmt::print("    linkopts: {}\n", gcc_command.second->linkopts);
    fmt::print("    link_search_dirs: {}\n",
               gcc_command.second->link_search_dirs);
    fmt::print("    link_libs: {}\n", gcc_command.second->link_libs);

    // Save away all dependencies as keys. We'll use keep track of which
    // dependencies haven't yet been matched to others with the same flags.
    map<string, bool> unused_dependencies;
    for (auto input : gcc_command.second->inputs) {
      unused_dependencies.insert(pair(input, false));
    }
    // consumes unused_dependencies.
    find_deps(unused_dependencies, gcc_compile_commands, gcc_link_commands,
              ar_commands);
  }

  input_stream.close();

  return 0;
}
