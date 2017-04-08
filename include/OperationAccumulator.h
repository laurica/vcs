#ifndef OPERATIONACCUMULATOR
#define OPERATIONACCUMULATOR

#include <string>
#include <vector>

class OperationAccumulator {
  bool projectInit;
  bool projectInitializedThisRun;
  std::string projectName;
  std::vector<std::string> trackedFiles;

  void outputTrackedFiles() const;
  bool outputBasicInfo() const;
  bool alreadyTracked(const std::string& fileName) const;
  
public:
  OperationAccumulator();
  void addFile(const std::string& fileName);
  void initializeProject(const std::string& projectName);
  void saveState() const;
  bool initialize();
};

#endif
