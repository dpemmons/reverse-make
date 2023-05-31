#ifndef REVERSE_MAKE_COMMANDS_H__
#define REVERSE_MAKE_COMMANDS_H__

#include <filesystem>
#include <set>
#include <string>
#include <vector>

using namespace std;

/**
 * Struct that holds the command type, flags, options, and file paths for a GCC
 * or G++ command.
 */
struct GccCommand {
  enum { GCC, GPP } compiler;

  enum Command {
    COMPILE,              // -c
    COMPILE_NO_ASSEMBLE,  // -S
    PREPROCESS_ONLY,      // -E
    LINK
  } command;

  set<string> defines;   // -D
  set<string> includes;  // -I
  set<string> cflags;    // -fsomething, -std

  set<string> warns;          // -W
  set<string> target_opts;    // -m
  set<string> optimizations;  // -O
  set<string> debug;          // -g

  // Link options
  set<string> linkopts;
  set<string> link_search_dirs;  // -Ldir
  set<string> link_libs;         // -Ldir

  vector<filesystem::path> inputs;
  filesystem::path output;

  std::string CompilerAsString() {
    switch (compiler) {
      case GCC:
        return "gcc";
      case GPP:
        return "g++";
      default:
        abort();
    }
  }

  std::string CommandAsString() {
    switch (command) {
      case COMPILE:
        return "COMPILE";
      case COMPILE_NO_ASSEMBLE:
        return "COMPILE_NO_ASSEMBLE";
      case PREPROCESS_ONLY:
        return "PREPROCESS_ONLY";
      case LINK:
        return "LINK";
      default:
        abort();
    }
  }

  bool FlagsMatch(const GccCommand& other) {
    return other.command == command && other.defines == defines &&
           other.includes == includes && other.cflags == cflags &&
           other.warns == warns && other.target_opts == target_opts &&
           other.debug == debug && other.linkopts == linkopts &&
           other.link_search_dirs == link_search_dirs &&
           other.link_libs == link_libs;
  }
};

/**
 * Struct that holds the input and output file paths for an 'ar' command.
 */
struct ArCommand {
  vector<filesystem::path> inputs;
  filesystem::path output;
};

#endif  // REVERSE_MAKE_COMMANDS_H__
