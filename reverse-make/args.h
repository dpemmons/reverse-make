#ifndef REVERSE_MAKE_ARGS_H__
#define REVERSE_MAKE_ARGS_H__

#include <string>
#include <variant>

class Args {
 public:
  ~Args() {}
  static std::variant<Args, int> ParseArgs(int argc, const char* argv[]);

  const std::string& getInpuFilename() const { return filename_; }

 private:
  Args() {}
  std::string filename_;
};

#endif  // REVERSE_MAKE_ARGS_H__
