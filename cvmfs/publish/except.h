/**
 * This file is part of the CernVM File System.
 */

#ifndef CVMFS_PUBLISH_EXCEPT_H_
#define CVMFS_PUBLISH_EXCEPT_H_

#include <stdexcept>
#include <string>

namespace publish {

class EPublish : public std::runtime_error {
 public:
  /**
   * Well-known exceptions that are usually caught and handled
   */
  enum EFailures {
    kFailUnspecified = 0,
    kFailInput,               // Invalid input
    kFailInvocation,          // Invalid command line options
    kFailPermission,          // Not owner of the repository
    kFailTransactionState,    // The txn was expected to be in the other state
    kFailGatewayKey,          // cannot access the gateway secret key
    kFailLeaseHttp,           // cannot connect to the gateway HTTP endpoint
    kFailLeaseBody,           // corrupted session token
    kFailLeaseBusy,           // another active lease blocks the path
    kFailLeaseNoEntry,        // the lease path does not exist
    kFailLeaseNoDir,          // the lease path is no a directory
    kFailRepositoryNotFound,  // the repository was not found on the machine
    kFailRepositoryType,      // the stratum type (0, 1) does not match
    kFailLayoutRevision,      // unsupported layout revision, migrate required
    kFailWhitelistExpired,    //
    kFailMissingDependency,   // a program or service was not found

    kFailNumEntries
  };

  explicit EPublish(const std::string& what, EFailures f = kFailUnspecified)
    : std::runtime_error(what + "\n\nStacktrace:\n" + GetStacktrace())
    , failure_(f)
    , msg_(what)
  {}

  virtual ~EPublish() throw();

  std::string name() const {
    static const char *names[kFailNumEntries + 1] = {
      "Unspecified error",
      "Invalid input",
      "Invocation error",
      "Permission error",
      "Transaction state incorrect",
      "Gateway key error",
      "Cannot connect to gateway",
      "Corrupt session token",
      "Lease path is busy",
      "Lease path does not exist",
      "Lease path is not a directory",
      "Repository missing",
      "Repository stratum incorrect",
      "Unsupported layout revision",
      "Whitelist expired",
      "Missing dependency",
      ""
    };
    return names[failure_];
  }

  int retval() const {
    static const int retvals[kFailNumEntries + 1] = {
      EINVAL,
      EINVAL,
      EINVAL,
      EPERM,
      EEXIST,
      EPERM,
      EHOSTUNREACH,
      EPERM,
      EBUSY,
      ENOENT,
      ENOTDIR,
      ENOENT,
      ENOTTY,
      ENOTSUP,
      ESTALE,
      ENOENT,
      0
    };
    return retvals[failure_];
  }

  EFailures failure() const { return failure_; }
  std::string msg() const { return msg_; }

 private:
  EFailures failure_;
  std::string msg_;

  /**
   * Maximum number of frames in the stack trace
   */
  static const unsigned kMaxBacktrace = 64;
  static std::string GetStacktrace();
};

}  // namespace publish

#endif  // CVMFS_PUBLISH_EXCEPT_H_
