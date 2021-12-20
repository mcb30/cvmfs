/**
 * This file is part of the CernVM File System.
 */

#include "cvmfs_config.h"
#include "cmd_transaction.h"

#include <errno.h>

#include <ctime>
#include <string>
#include <vector>

#include "logging.h"
#include "publish/cmd_util.h"
#include "publish/except.h"
#include "publish/repository.h"
#include "publish/settings.h"
#include "util/posix.h"
#include "util/string.h"
#include "whitelist.h"

namespace publish {

int CmdTransaction::Main(const Options &options) {
  // Repository name and lease path are submitted as a single argument
  // for historical reasons
  std::string fqrn;
  std::string lease_path;
  if (!options.plain_args().empty()) {
    std::vector<std::string> tokens =
      SplitString(options.plain_args()[0].value_str, '/', 2);
    fqrn = tokens[0];
    if (tokens.size() == 2)
      lease_path = MakeCanonicalPath(tokens[1]);
  }

  SettingsBuilder builder;
  UniquePtr<SettingsPublisher> settings;
  settings = builder.CreateSettingsPublisher(fqrn, true /* needs_managed */);
  if (settings->transaction().in_enter_session()) {
    throw EPublish(
      "opening a transaction is unsupported within the ephemeral "
      "writable shell",
      EPublish::kFailInvocation);
  }
  if (options.Has("retry-timeout")) {
    settings->GetTransaction()->SetTimeout(options.GetInt("retry-timeout"));
  }
  if (options.Has("template-from")) {
    if (!options.Has("template-to"))
      throw EPublish("invalid parameter combination for templates");
    settings->GetTransaction()->SetTemplate(
      options.GetString("template-from"), options.GetString("template-to"));
  }
  if (options.Has("template")) {
    if (options.Has("template-from") || options.Has("template-to"))
      throw EPublish("invalid parameter combination for templates");
    std::string templ = options.GetString("template");
    std::vector<std::string> tokens = SplitString(templ, '=');
    if (tokens.size() != 2)
      throw EPublish("invalid syntax for --template parameter: " + templ);
    settings->GetTransaction()->SetTemplate(tokens[0], tokens[1]);
  }

  if (!SwitchCredentials(settings->owner_uid(), settings->owner_gid(),
                         false /* temporarily */))
  {
    throw EPublish("No write permission to repository",
                   EPublish::kFailPermission);
  }
  FileSystemInfo fs_info = GetFileSystemInfo("/cvmfs");
  if (fs_info.type == kFsTypeAutofs)
    throw EPublish("Autofs on /cvmfs has to be disabled");

  settings->GetTransaction()->SetLeasePath(lease_path);


  UniquePtr<Publisher> publisher;
  publisher = new Publisher(*settings);
  if (publisher->whitelist()->IsExpired()) {
    throw EPublish("Repository whitelist for $name is expired",
                   EPublish::kFailWhitelistExpired);
  }

  double whitelist_valid_s =
    difftime(publisher->whitelist()->expires(), time(NULL));
  if (whitelist_valid_s < (12 * 60 * 60)) {
    LogCvmfs(kLogCvmfs, kLogStdout,
      "Warning: Repository whitelist stays valid for less than 12 hours!");
  }

  int rvi = CallServerHook("transaction_before_hook", fqrn);
  if (rvi != 0) {
    LogCvmfs(kLogCvmfs, kLogStderr | kLogSyslogErr,
             "transaction hook failed, not opening a transaction");
    return rvi;
  }

  publisher->Transaction();

  publisher->session()->SetKeepAlive(true);

  rvi = CallServerHook("transaction_after_hook", fqrn);
  if (rvi != 0) {
    LogCvmfs(kLogCvmfs, kLogStderr | kLogSyslogErr,
             "post transaction hook failed");
    return rvi;
  }

  return 0;
}

}  // namespace publish
