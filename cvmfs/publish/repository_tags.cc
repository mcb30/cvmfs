/**
 * This file is part of the CernVM File System.
 */

#include "cvmfs_config.h"
#include "publish/repository.h"

#include <vector>

#include "catalog_mgr_ro.h"
#include "history_sqlite.h"
#include "publish/except.h"
#include "repository_tag.h"
#include "sanitizer.h"
#include "util/posix.h"

namespace publish {

void Publisher::CheckTagName(const std::string &name) {
  if (name.empty()) throw EPublish("the empty string is not a valid tag name");
  if (name == "trunk")
    throw EPublish("'trunk' is not allowed as a custom tag name");
  if (name == "trunk-previous")
    throw EPublish("'trunk-previous' is not allowed as a custom tag name");
  if (!sanitizer::TagSanitizer().IsValid(name))
    throw EPublish("invalid tag name: " + name);
}


history::History::Tag Publisher::MakeTag(const RepositoryTag &tag_info,
                                         const std::string &branch,
                                         const shash::Any &root_hash) {
  UniquePtr<catalog::SimpleCatalogManager> catalog_mgr;
  catalog_mgr = CreateSimpleCatalogManager(root_hash);
  catalog::Catalog *catalog = catalog_mgr->GetRootCatalog();

  history::History::Tag tag;
  tag.name = tag_info.name();
  tag.root_hash = catalog->hash();
  tag.size = GetFileSize(catalog->database_path());
  tag.revision = catalog->GetRevision();
  tag.timestamp = catalog->GetLastModified();
  tag.branch = branch;
  if (!tag_info.channel().empty()) {
    tag.channel = static_cast<history::History::UpdateChannel>(
      String2Uint64(tag_info.channel()));
  } else {
    tag.channel = history::History::kChannelTrunk;
  }
  tag.description = tag_info.description();
  return tag;
}


void Publisher::EditTags(const std::vector<history::History::Tag> &add_tags,
                         const std::vector<std::string> &rm_tags)
{
  if (!IsInTransaction())
    throw EPublish("cannot edit tags outside transaction");

  for (unsigned i = 0; i < add_tags.size(); ++i) {
    std::string name = add_tags[i].name;
    CheckTagName(name);
    history_->Insert(add_tags[i]);
  }

  for (unsigned i = 0; i < rm_tags.size(); ++i) {
    std::string name = rm_tags[i];
    CheckTagName(name);
    if (history_->Exists(name)) {
      bool retval = history_->Remove(name);
      if (!retval) throw EPublish("cannot remove tag " + name);
    }
  }

  PushHistory();

  // TODO(jblomer): virtual catalog
}

}  // namespace publish
