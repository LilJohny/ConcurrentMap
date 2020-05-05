#ifndef AKSLAB5_FILEDOESNOTEXISTSEXCEPTION_H
#define AKSLAB5_FILEDOESNOTEXISTSEXCEPTION_H

#include <string>
#include <stdexcept>

struct FileDoesNotExistsException : public std::runtime_error {
 public:
  explicit FileDoesNotExistsException(const std::string &filename) : runtime_error(
      filename + " does not exists!") {}
};

#endif //AKSLAB5_FILEDOESNOTEXISTSEXCEPTION_H
