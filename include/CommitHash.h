#ifndef COMMITHASH
#define COMMITHASH

#include <string>

class CommitHash {
 private:
  static unsigned int curCommitCounter;
  int myHash;

 public:
  CommitHash();
  CommitHash(const std::string& hash);
  std::string toString() const;
  static CommitHash getLatestGeneratedHash();
  static void setSeed(const std::string& lastHash);
};

#endif
