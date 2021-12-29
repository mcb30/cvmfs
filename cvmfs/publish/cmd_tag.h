/**
 * This file is part of the CernVM File System.
 */

#ifndef CVMFS_PUBLISH_CMD_TAG_H_
#define CVMFS_PUBLISH_CMD_TAG_H_

#include <string>

#include "publish/command.h"

namespace publish {

class CmdTag : public Command {
 public:
  virtual std::string GetName() const { return "tag"; }
  virtual std::string GetBrief() const {
    return "Create and manage named snapshots";
  }
  virtual std::string GetDescription() const {
    return "Create, delete, or list named snapshots of a repository. "
      "Named snapshots can be used to identify historical contents of "
      "the repository and may be accessed via the .cvmfs/snapshots "
      "virtual directory.";
  }
  virtual std::string GetUsage() const {
    return "[options] <repository name / URL>";
  }
  virtual ParameterList GetParams() const {
    ParameterList p;
    p.push_back(Parameter::Optional("add", 'a', "name",
      "Create new tag <name>"));
    p.push_back(Parameter::Optional("channel", 'c', "channel",
      "Specify channel for new tag"));
    p.push_back(Parameter::Optional("message", 'm', "message",
      "Specify descriptive message for new tag"));
    p.push_back(Parameter::Optional("hash", 'h', "hash",
      "Specify hash of newly created tag"));
    p.push_back(Parameter::Optional("remove", 'r', "name",
      "Remove tag <name>"));
    p.push_back(Parameter::Optional("inspect", 'i', "name",
      "Inspect tag <name>"));
    p.push_back(Parameter::Switch("branches", 'b',
      "List branch hierarchy"));
    p.push_back(Parameter::Switch("list", 'l',
      "List tags"));
    p.push_back(Parameter::Switch("force", 'f',
      "Do not ask for confirmation"));
    p.push_back(Parameter::Switch("machine-readable", 'x',
      "Produce machine readable output"));
    return p;
  }
  virtual unsigned GetMinPlainArgs() const { return 1; }
  virtual unsigned GetMaxPlainArgs() const { return 1; }

  virtual int Main(const Options &options);
};  // class CmdTag

}  // namespace publish

#endif  // CVMFS_PUBLISH_CMD_TAG_H_
