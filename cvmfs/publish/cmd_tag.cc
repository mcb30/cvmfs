/**
 * This file is part of the CernVM File System.
 */

#include "cvmfs_config.h"
#include "cmd_tag.h"

#include "logging.h"
#include "manifest.h"
#include "options.h"
#include "publish/except.h"
#include "publish/repository.h"
#include "publish/settings.h"
#include "repository_tag.h"

namespace publish {

int CmdTag::Main(const Options &options) {
  SettingsBuilder builder;
  std::string fqrn = options.plain_args()[0].value_str;
  bool needs_managed = true;
  UniquePtr<SettingsPublisher> settings;
  settings = builder.CreateSettingsPublisher(fqrn, needs_managed);

  UniquePtr<Publisher> publisher;
  publisher = new Publisher(*settings);

  // Identify tag to be added, if any
  std::vector<history::History::Tag> add_tags;
  if (options.Has("add")) {
    const std::string name = options.GetString("add");
    const std::string channel = options.GetStringDefault("channel", "");
    const std::string description = options.GetStringDefault("message", "");
    const RepositoryTag tag_info(name, channel, description);
    const std::string branch; // TODO - no argument seems to correspond to this
    shash::Any hash;
    if (options.Has("hash")) {
      std::string hash_string = options.GetString("hash");
      hash = shash::MkFromHexPtr(shash::HexPtr(hash_string),
                                 shash::kSuffixCatalog);
      if (hash.IsNull()) {
        throw EPublish("invalid hash: " + hash_string, EPublish::kFailInput);
      }
    }
    history::History::Tag tag = publisher->MakeTag(tag_info, branch, hash);
    add_tags.push_back(tag);
  }

  // Identify tag to be removed, if any
  std::vector<std::string> rm_tags;
  if (options.Has("remove")) {
    rm_tags.push_back(options.GetString("remove"));
  }

  // Edit tags, if applicable
  if (!(add_tags.empty() && rm_tags.empty())) {
    publisher->Transaction();
    publisher->EditTags(add_tags, rm_tags);
    publisher->Publish();
  }

  // Identify tag to be inspected, if any
  std::vector<std::string> inspect_tags;
  if (options.Has("inspect")) {
    inspect_tags.push_back(options.GetString("inspect"));
  }

  

  LogCvmfs(kLogCvmfs, kLogStdout, "***** %s",
           publisher->manifest()->ExportString().c_str());

  return 0;
}

}  // namespace publish
