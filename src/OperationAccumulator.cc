#include <assert.h>
#include <iostream>
#include <sstream>

#include "DiffInterface.h"
#include "FileParser.h"
#include "FileSystemInterface.h"
#include "FileWriter.h"
#include "OperationAccumulator.h"

using namespace std;

OperationAccumulator::OperationAccumulator() :
  projectInit(false), initialCommitPerformed(false), curCommit(NULL) {
  fileNames[FileName::ADDED_FILES] = ".kil/.addedFiles.txt";
  fileNames[FileName::BASIC_INFO] = ".kil/.basicInfo.txt";
  fileNames[FileName::BRANCH_LIST] = ".kil/.branches.txt";
  fileNames[FileName::COMMIT_DIR] = ".kil/.commits";
  fileNames[FileName::MAIN_DIR] = ".kil";
  fileNames[FileName::TRACKED_FILES] = ".kil/.trackedFiles.txt";
  fileNames[FileName::TREE_FILE] = ".kil/.tree.txt";
}

OperationAccumulator::~OperationAccumulator() {
  delete curCommit;
}

void OperationAccumulator::initializeProject(const std::string& fileName) {
  projectInit = true;
  projectName = fileName;
  
  curBranch = "Master";
  tree.initialize(curBranch);
  branches.insert("Master");
}

bool OperationAccumulator::alreadyTracked(const string& fileName) const {
  for (string file : trackedFiles) {
    if (fileName == file) {
      return true;
    }
  }

  for (string file : addedFiles) {
    if (fileName == file) {
      return true;
    }
  }

  return false;
}

bool OperationAccumulator::addFile(const string& fileName) {  
  // Do we already have this file
  if (!alreadyTracked(fileName)) {
    addedFiles.push_back(fileName);
    return true;
  }

  return false;
}

static void outputVectorInfoToFile(const char * fileName,
				   const vector<string>& lines) {
  ofstream outputStream;
  outputStream.open(fileName, fstream::out);
  
  for (string line : lines) {
    outputStream << line << "\n";
  }
  
  outputStream << flush;
  outputStream.close();
}

void OperationAccumulator::outputTrackedFiles() const {
  outputVectorInfoToFile(fileNames.at(FileName::TRACKED_FILES), trackedFiles);
}

void OperationAccumulator::outputAddedFiles() const {
  outputVectorInfoToFile(fileNames.at(FileName::ADDED_FILES), addedFiles);
}

bool OperationAccumulator::outputBasicInfo() const {
  if (FileSystemInterface::createDirectory(fileNames.at(FileName::MAIN_DIR))
      == -1) {
    cout << "Could not initialize project!\n";
    return false;
  }
  
  string basicProjectInfoFileName = fileNames.at(FileName::BASIC_INFO);
  
  ofstream outputStream;
  outputStream.open(basicProjectInfoFileName, fstream::out);

  outputStream << "projName=" << projectName << "\n";
  outputStream << "curBranch=" << curBranch << "\n";
  outputStream << "initialCommit=" <<
    (initialCommitPerformed ? "true" : "false")  << "\n";
  if (initialCommitPerformed) {
    // curCommit represents the current commit we're on whereas lastHash
    // represents the last commit created
    // These differ if you create a commit on one branch, then check out a
    // different branch
    outputStream << "curCommit=" << curCommit->toString() << "\n";
    outputStream << "lastHash=" <<
      CommitHash::getLatestGeneratedHash().toString() << "\n";
  }
  
  outputStream << flush;
  outputStream.close();

  return true;
}

void OperationAccumulator::outputTree() const {
  vector<string> lines;
  tree.getPrintableTree(lines);
  outputVectorInfoToFile(fileNames.at(FileName::TREE_FILE), lines);
}

void OperationAccumulator::outputBranches() const {
  const string branchFileName = fileNames.at(FileName::BRANCH_LIST);

  ofstream outputStream;
  outputStream.open(branchFileName, fstream::out);

  for (const string& branch : branches) {
    outputStream << branch << "\n";
  }

  outputStream.flush();
  outputStream.close();
}

bool OperationAccumulator::readAddedAndTrackedFiles() {
  // read in from the trackedFiles file
  if (!FileSystemInterface::fileExists(fileNames.at(FileName::TRACKED_FILES)) ||
      !FileSystemInterface::fileExists(fileNames.at(FileName::ADDED_FILES))) {
    return false;
  }

  vector<string> lines;
  FileParser::readFile(fileNames.at(FileName::TRACKED_FILES), lines);
  for (string fileName : lines) {
    trackedFiles.push_back(fileName);
  }

  lines.clear();
  FileParser::readFile(fileNames.at(FileName::ADDED_FILES), lines);
  for (string fileName : lines) {
    addedFiles.push_back(fileName);
  }

  return true;
}

bool OperationAccumulator::readBasicInfo() {
  size_t MIN_NUMBER_LINES = 3;
  size_t PROJ_NAME_INDEX = 0;
  size_t CUR_BRANCH_INDEX = 1;
  size_t INITIAL_COMMIT_INDEX = 2;
  size_t CUR_COMMIT_INDEX = 3;
  size_t LAST_HASH_INDEX = 4;
  size_t NUM_LINES_WITH_INIT_COMMIT = 5;
  
  // make sure that all of the information that needs to be there, is there
  if (!(FileSystemInterface::fileExists(fileNames.at(FileName::BASIC_INFO)))) {
    return false;
  }

  vector<string> lines;
  FileParser::readFile(fileNames.at(FileName::BASIC_INFO), lines);
  
  if (lines.size() < MIN_NUMBER_LINES) {
    return false;
  }
  
  if (lines[PROJ_NAME_INDEX].find("projName=") != 0 ||
      lines[PROJ_NAME_INDEX].substr(9).length() == 0 ||
      lines[CUR_BRANCH_INDEX].find("curBranch=") != 0 ||
      lines[CUR_BRANCH_INDEX].substr(10).length() == 0 ||
      lines[INITIAL_COMMIT_INDEX].find("initialCommit=") != 0) {
    return false;
  }
  
  string initialCommitString = lines[INITIAL_COMMIT_INDEX].substr(14);
  
  if (initialCommitString != "true" && initialCommitString != "false") {
    return false;
  }

  projectName = lines[PROJ_NAME_INDEX].substr(9);
  curBranch = lines[CUR_BRANCH_INDEX].substr(10);
  
  initialCommitPerformed = initialCommitString == "true" ? true : false;

  if (initialCommitPerformed) {
    if (lines.size() != NUM_LINES_WITH_INIT_COMMIT ||
	lines[CUR_COMMIT_INDEX].find("curCommit=") != 0 ||
	lines[CUR_COMMIT_INDEX].substr(10).length() == 0 ||
	lines[LAST_HASH_INDEX].find("lastHash=") != 0 ||
	lines[LAST_HASH_INDEX].substr(9).length() == 0) {
      return false;
    }
    delete curCommit;
    curCommit = new CommitHash(lines[CUR_COMMIT_INDEX].substr(10));
    CommitHash::setSeed(lines[LAST_HASH_INDEX].substr(9));
  }

  return true;
}

bool OperationAccumulator::readTree() {
  if (!(FileSystemInterface::fileExists(fileNames.at(FileName::TREE_FILE)))) {
    return false;
  }

  vector<string> lines;
  FileParser::readFile(fileNames.at(FileName::TREE_FILE), lines);
  
  return tree.initializeTree(lines, curBranch, curCommit == NULL ?
			     CommitHash::getNullHash() :
			     curCommit->toString());
}

bool OperationAccumulator::readInBranches() {
  if (!(FileSystemInterface::fileExists(fileNames.
					  at(FileName::BRANCH_LIST)))) {
    return false;
  }

  vector<string> lines;
  FileParser::readFile(fileNames.at(FileName::BRANCH_LIST), lines);

  for (const string& branchLine : lines) {
    branches.insert(branchLine);
  }

  return true;
}

bool OperationAccumulator::initialize() {
  // try and see if the .kil directory is created
  if (!(FileSystemInterface::fileExists(fileNames.at(FileName::MAIN_DIR)))) {
    return true;
  }

  projectInit = true;
  
  const string error = "Error! KIL information tampered with or missing!";
  
  if (!readBasicInfo() || !readTree() || !readAddedAndTrackedFiles() ||
      !readInBranches()) {
     cout << error << endl;
     return false;
  }

  return true;
}

void OperationAccumulator::saveState() const {
  if (!projectInit) {
    return;
  }


  if (!outputBasicInfo()) {
    return;
  }

  // TODO:
  // Check if any of these have been changes before blindly rewriting the info
  outputTrackedFiles();
  outputAddedFiles();
  outputTree();
  outputBranches();
}

bool OperationAccumulator::isInitialized() const {
  return projectInit;
}

string OperationAccumulator::getCurBranchName() const {
  return curBranch;
}

void OperationAccumulator::calculateRemovalsAndDiffs(
    vector<string>& removedFiles,
    vector<pair<string, FileDiff> >& diffs) const {
  for (const string& trackedFile : trackedFiles) {
    if (FileSystemInterface::fileExists(trackedFile.c_str())) {
      // calculate the location of the previous diff
      // path to version of file in previous commit will be commithash/filepath
      string previousCommitFileName =
	FileSystemInterface::appendPath(fileNames.at(COMMIT_DIR),
					curCommit->toString());
      previousCommitFileName =
	FileSystemInterface::appendPath(previousCommitFileName, trackedFile);
      FileDiff diff =
	DiffInterface::calculateFileDiff(
	    previousCommitFileName.c_str(), trackedFile.c_str());
      if (!diff.isEmptyDiff()) {
	diffs.push_back(make_pair(trackedFile, diff));
      }
    } else {
      removedFiles.push_back(trackedFile);
    }
  }
}

void OperationAccumulator::createNewCommitDirectory(
    const string& newCommitDirectoryPath) const {
  if (!initialCommitPerformed) {
      vector<string> directories;
      FileSystemInterface::parseDirectoryStructure(newCommitDirectoryPath,
						   directories);
      FileSystemInterface::createDirectories("", directories);
    }
    
    // Create new directory with hash
    FileSystemInterface::createDirectory(newCommitDirectoryPath.c_str());
}

string OperationAccumulator::calculateFileLocationForHash(
    const CommitHash& hash) const {
  string path =
    FileSystemInterface::appendPath(fileNames.at(COMMIT_DIR), hash.toString());
  path = FileSystemInterface::appendPath(path, hash.toString().c_str());
  path = path + ".txt";
  return path;
}

void OperationAccumulator::updateParentCommit(
    const CommitHash& childHash) const {
  size_t MIN_LINES_INFO = 8;
  size_t CHILD_COMMIT_LINE = 4;
  
  const string path = calculateFileLocationForHash(*curCommit);
  vector<string> fileContents;
  FileParser::readFile(path.c_str(), fileContents);
  
  assert(fileContents.size() >= MIN_LINES_INFO);
  assert(fileContents[CHILD_COMMIT_LINE].find("childCommits=[") == 0);
  assert(fileContents[CHILD_COMMIT_LINE].
	 at(fileContents[CHILD_COMMIT_LINE].length() - 1) == ']');

  string listOfChildren = fileContents[CHILD_COMMIT_LINE].substr(14);
  listOfChildren = listOfChildren.substr(0, listOfChildren.size() - 1);
  
  if (listOfChildren.length() == 0) {
    // this is the first child we're adding
    fileContents[CHILD_COMMIT_LINE] = "childCommits=[" +
      childHash.toString() + "]";
  } else {
    fileContents[CHILD_COMMIT_LINE] = "childCommits=[" + listOfChildren + "," +
      childHash.toString() + "]";
  }

  FileWriter::writeFile(path.c_str(), fileContents);
}

void OperationAccumulator::writeBasicCommitInfo(
    ofstream& output, const string& newCommitFileName, const CommitHash& hash,
    const string& commitMessage) const {
  output.open((newCommitFileName + ".txt").c_str(), fstream::out);
  output << "commitHash=" << hash.toString() << "\n";
  output << "commitMessage=\"" << commitMessage << "\"\n";
  output << "branch=" << curBranch << "\n";
  output << "parentCommit=";
  if (initialCommitPerformed) {
    output << curCommit->toString();
  } else {
    output << "ROOT";
  }
  output << "\n";
  output << "childCommits=[]\n";
}

static vector<string>::const_iterator vectorContains(
    const vector<string>& myVector, string element) {
  for (vector<string>::const_iterator it = myVector.begin();
       it != myVector.end(); ++it) {
    if ((*it) == element) {
      return it;
    }
  }

  return myVector.end();
}

void OperationAccumulator::removeDeletedFilesFromLists(const vector<string>&
						       removedFiles) {
  for (const string& removedFile : removedFiles) {
    vector<string>::const_iterator location =
      vectorContains(trackedFiles, removedFile);
    if (location != trackedFiles.end()) {
      trackedFiles.erase(location);
    }

    location = vectorContains(addedFiles, removedFile);
    if (location != addedFiles.end()) {
      addedFiles.erase(location);
    }
  }
}

void OperationAccumulator::writeOutAddedFiles(
    ofstream& output, const vector<string>& addedFiles,
    const string& newCommitDirectoryPath) const {
  output << "addedFiles [" << addedFiles.size() << "]\n";
  for (string addedFile : addedFiles) {
      cout << "Created file " << addedFile << endl;
      output << addedFile << "\n";
  }

  for (const string& newFile : addedFiles) {
      vector<string> directories;
      FileSystemInterface::parseDirectoryStructure(newFile, directories);
      FileSystemInterface::createDirectories(newCommitDirectoryPath, directories);
      vector<string> fileLines;
      FileParser::readFile(newFile.c_str(), fileLines);
      FileWriter::writeFile(
          FileSystemInterface::appendPath(newCommitDirectoryPath, newFile).c_str(),
	                                  fileLines);
  }
}

void OperationAccumulator::writeOutCommit(
    const string& commitMessage, const vector<string>& addedFiles,
    const vector<string>& removedFiles,
    const vector<pair<string, FileDiff> >& diffs) {
  CommitHash * hash = new CommitHash();

  string newCommitDirectoryPath =
    FileSystemInterface::appendPath(fileNames.at(COMMIT_DIR),
				    hash->toString().c_str());
  
  createNewCommitDirectory(newCommitDirectoryPath);
  
  ofstream output;
  string newCommitFileName =
    FileSystemInterface::appendPath(newCommitDirectoryPath,
				    hash->toString().c_str());
  writeBasicCommitInfo(output, newCommitFileName, *hash, commitMessage);
  
  writeOutAddedFiles(output, addedFiles, newCommitDirectoryPath);
  
  // now write out the removed files
  output << "removedFiles [" << removedFiles.size() << "]\n";
  for (string removedFile : removedFiles) {
    cout << "Removed file " << removedFile << endl;
    output << removedFile << "\n";
  }

  // Now remove the removed files from our added/tracked file lists
  removeDeletedFilesFromLists(removedFiles);
  
  // write out which files have diffs
  output << "diffs [" << diffs.size() << "]\n";
  
  // now write out the diffs
  for (const pair<string, FileDiff>& diffInfo : diffs) {
    cout << "Updating file " << diffInfo.first << " with " <<
      diffInfo.second.getNumInsertions() << " insertions and " <<
      diffInfo.second.getNumDeletions() << " deletions" << endl;
    vector<string> directories;
    FileSystemInterface::parseDirectoryStructure(diffInfo.first, directories);
    FileSystemInterface::createDirectories(newCommitDirectoryPath,
					   directories);
    diffInfo.second.
      print(FileSystemInterface::appendPath(newCommitDirectoryPath,
					    diffInfo.first));
  }

  output.flush();
  output.close();

  if (initialCommitPerformed) {
    // update the parent commit
    updateParentCommit(*hash);
  }

  delete curCommit;
  curCommit = hash;
}

void OperationAccumulator::getAddedFiles(vector<string>&
					 verifiedAddedFiles) const {
  // Make sure that files that were added haven't been deleted in the time since
  // they've been added
  for (const string& addedFile : addedFiles) {
    if (FileSystemInterface::fileExists(addedFile.c_str())) {
      verifiedAddedFiles.push_back(addedFile);
    }
  }
}

bool OperationAccumulator::commit(const string& commitMessage,
				  const bool addFlag) {  
  vector<string> verifiedAddedFiles;
  
  if (addFlag) {
    getAddedFiles(verifiedAddedFiles);
  }

  // Now found out which file have been deleted, and gets the diffs for the
  // files that have been changed
  vector<string> removedFiles;
  vector<pair<string, FileDiff> > diffs;
  calculateRemovalsAndDiffs(removedFiles, diffs);

  if (verifiedAddedFiles.size() == 0 && removedFiles.size() == 0 &&
      diffs.size() == 0) {
    return false;
  }

  // We're going to output this info now
  writeOutCommit(commitMessage, verifiedAddedFiles, removedFiles, diffs);

  // Update internal state
  if (addFlag) {
    for (string addedFile : verifiedAddedFiles) {
      trackedFiles.push_back(addedFile);
    }
    addedFiles.clear();
  }

  initialCommitPerformed = true;

  // curCommit has now been updated
  tree.addCommit(*curCommit);
  
  return true;
}

void OperationAccumulator::getStatus() const {
  // Print all the added files
  vector<string> verifiedAddedFiles;
  getAddedFiles(verifiedAddedFiles);

  vector<string> removedFiles;
  vector<pair<string, FileDiff> > diffs;
  calculateRemovalsAndDiffs(removedFiles, diffs);

  if (verifiedAddedFiles.size() == 0 && removedFiles.size() == 0 &&
      diffs.size() == 0) {
    cout << "No new changes to be commited!" << endl;
  } else {
    cout << "Changes to be committed:" << endl;

    for (const string& addedFileName : verifiedAddedFiles) {
      cout << "new file: " << addedFileName << endl;
    }

    for (const pair<string, FileDiff>& diff : diffs) {
      cout << "modified: " << diff.first << endl;
    }
    
    for (const string& removedFile : removedFiles) {
      cout << "deleted: " << removedFile << endl;
    }
  }
}

void OperationAccumulator::createNewBranch(const string& newBranchName) {
  if (!cleanState()) {
    cout << "Please commit changes before checking out new branch!" << endl;
    return;
  }
  
  // Tell our tree there is a new branch
  curBranch = newBranchName;
  tree.registerNewBranch(newBranchName);
  branches.insert(newBranchName);
}

bool OperationAccumulator::filesHaveBeenAdded() const {
  for (const string& addedFile : addedFiles) {
    if (FileSystemInterface::fileExists(addedFile.c_str())) {
      return true;
    }
  }

  return false;
}

bool OperationAccumulator::filesHaveBeenRemovedOrModified() const {
  for (const string& trackedFile : trackedFiles) {
    if (FileSystemInterface::fileExists(trackedFile.c_str())) {
      string previousCommitFileName =
	FileSystemInterface::appendPath(fileNames.at(COMMIT_DIR),
					curCommit->toString());
      previousCommitFileName =
	FileSystemInterface::appendPath(previousCommitFileName, trackedFile);
      if (!FileParser::compareFiles(previousCommitFileName.c_str(),
				   trackedFile.c_str())) {
	return true;
      }
    } else {
      return true;
    }
  }

  return false;
}

bool OperationAccumulator::cleanState() const {
  if (filesHaveBeenAdded() || filesHaveBeenRemovedOrModified()) {
    return false;
  }
  
  return true;
}

void OperationAccumulator::switchBranch(const string& branchName) {
  // Check if the branch exists!
  if (branches.count(branchName) == 0) {
    cout << "No branch named " << branchName << " found!" << endl;
    return;
  }
  
  // Check there are no uncommitted changes!
  if (!cleanState()) {
    cout << "Please commit changes before checking out branch!" << endl;
    return;
  }

  // If we're here, we can actually switch branches!
}
