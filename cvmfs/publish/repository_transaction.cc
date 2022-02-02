/**
 * This file is part of the CernVM File System.
 */

#include "cvmfs_config.h"
#include "publish/repository.h"

#include <string>

#include "backoff.h"
#include "catalog_mgr_ro.h"
#include "catalog_mgr_rw.h"
#include "directory_entry.h"
#include "logging.h"
#include "manifest.h"
#include "publish/except.h"
#include "publish/repository_util.h"
#include "publish/settings.h"
#include "util/exception.h"
#include "util/pointer.h"
#include "util/posix.h"

namespace publish {


void Publisher::OpenTransaction() {
  // Check that no other transaction is in progress
  if (in_transaction_.IsSet()) {
    throw EPublish("another transaction is already open",
                   EPublish::kFailTransactionState);
  }

  // Set transaction flag
  in_transaction_.Set();

  // Prevent read-only mountpoint from changing for the duration of
  // the transaction (e.g. due to upstream changes if we are
  // publishing via a gateway)
  if (managed_node_.IsValid()) {
    shash::Any hash = managed_node_->GetMountedRootHash();
    managed_node->SetRootHash(hash);
  }

  // Make union mountpoint writable
  if (managed_node_.IsValid())
    managed_node->Unlock();

  LogCvmfs(kLogCvmfs, llvl_ | kLogDebug | kLogSyslog,
           "(%s) opened transaction", settings_.fqrn().c_str());
}


void Publisher::CloseTransaction() {
  // Do nothing if transaction has already been closed
  if (!in_transaction_.IsSet())
    return;

  // Make union mountpoint read-only
  if (managed_node_.IsValid())
    managed_node->Lock();

  // Allow read-only mountpoint to track upstream changes
  if (managed_node_.IsValid())
    managed_node->ClearRootHash();

  // Clear transaction flag
  in_transaction_.Clear();
}


void Publisher::TransactionRetry() {
  if (managed_node_.IsValid()) {
    int rvi = managed_node_->Check(false /* is_quiet */);
    if (rvi != 0) throw EPublish("cannot establish writable mountpoint");
  }

  BackoffThrottle throttle(500, 5000, 10000);
  // Negative timeouts (i.e.: no retry) will result in a deadline that has
  // already passed and thus has the correct effect
  uint64_t deadline = platform_monotonic_time() +
                      settings_.transaction().GetTimeoutS();
  if (settings_.transaction().GetTimeoutS() == 0)
    deadline = uint64_t(-1);

  while (true) {
    try {
      TransactionImpl();
      break;
    } catch (const publish::EPublish& e) {
      if (e.failure() != EPublish::kFailTransactionState) {
        session_->Drop();
        in_transaction_.Clear();
      }

      if ((e.failure() == EPublish::kFailTransactionState) ||
          (e.failure() == EPublish::kFailLeaseBusy))
      {
        if (platform_monotonic_time() > deadline)
          throw;

        LogCvmfs(kLogCvmfs, kLogStdout, "repository busy, retrying");
        throttle.Throttle();
        continue;
      }

      throw;
    }  // try-catch
  }  // while (true)

  if (managed_node_.IsValid())
    managed_node_->Unlock();
}


void Publisher::TransactionImpl() {
  if (in_transaction_.IsSet()) {
    throw EPublish("another transaction is already open",
                   EPublish::kFailTransactionState);
  }

  InitSpoolArea();

  // On error, Transaction() will release the transaction lock and drop
  // the session
  in_transaction_.Set();
  session_->Acquire();

  // We might have a valid lease for a non-existing path. Nevertheless, we run
  // run into problems when merging catalogs later, so for the time being we
  // disallow transactions on non-existing paths.
  if (!settings_.transaction().lease_path().empty()) {
    std::string path = GetParentPath(
      "/" + settings_.transaction().lease_path());
    catalog::SimpleCatalogManager *catalog_mgr = GetSimpleCatalogManager();
    catalog::DirectoryEntry dirent;
    bool retval = catalog_mgr->LookupPath(path, catalog::kLookupSole, &dirent);
    if (!retval) {
      throw EPublish("cannot open transaction on non-existing path " + path,
                     EPublish::kFailLeaseNoEntry);
    }
    if (!dirent.IsDirectory()) {
      throw EPublish(
        "cannot open transaction on " + path + ", which is not a directory",
        EPublish::kFailLeaseNoDir);
    }
  }

  ConstructSpoolers();

  // Lock read-only mountpoint to current catalog revision
  //if (managed_node_.IsValid())
  //managed_node_->SetRootHash();

  if (settings_.transaction().HasTemplate()) {
    LogCvmfs(kLogCvmfs, llvl_ | kLogStdout | kLogNoLinebreak,
             "CernVM-FS: cloning template %s --> %s ... ",
             settings_.transaction().template_from().c_str(),
             settings_.transaction().template_to().c_str());
    ConstructSyncManagers();

    try {
      catalog_mgr_->CloneTree(settings_.transaction().template_from(),
                              settings_.transaction().template_to());
    } catch (const ECvmfsException &e) {
      std::string panic_msg = e.what();
      in_transaction_.Clear();
      // TODO(aandvalenzuela): release session token (gateway publishing)
      throw publish::EPublish("cannot clone directory tree. " + panic_msg,
                              publish::EPublish::kFailInput);
    }

    Sync();
    SendTalkCommand(settings_.transaction().spool_area().readonly_talk_socket(),
      "chroot " + settings_.transaction().base_hash().ToString() + "\n");
    LogCvmfs(kLogCvmfs, llvl_ | kLogStdout, "[done]");
    // TODO(jblomer): fix-me
    // PushReflog();
  }
}

}  // namespace publish
