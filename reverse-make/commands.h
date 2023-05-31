#ifndef REVERSE_MAKE_COMMANDS_H__
#define REVERSE_MAKE_COMMANDS_H__

#include <set>
#include <string>
#include <vector>

using namespace std;

struct GccCommand {
  enum { GCC, GPP } compiler;

  enum {
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

  set<string> inputs;
  string output;
};

struct ArCommand {};

#endif  // REVERSE_MAKE_COMMANDS_H__
