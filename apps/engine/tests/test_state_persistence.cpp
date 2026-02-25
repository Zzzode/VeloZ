#include "veloz/engine/state_persistence.h"

#include <kj/debug.h>
#include <kj/filesystem.h>
#include <kj/test.h>

namespace veloz::engine {

KJ_TEST("StatePersistence: Save and Load Snapshot") {
  StatePersistenceConfig config;
  // Use a unique directory for testing
  config.snapshot_dir = kj::Path({"test_snapshots_persistence"});
  config.max_snapshots = 5;

  // Use a local scope for persistence to ensure it releases files
  {
    StatePersistence persistence(kj::mv(config));

    // Initialize (creates dir)
    KJ_EXPECT(persistence.initialize());

    // Create dummy snapshot
    StateSnapshot snapshot;
    snapshot.meta.version = 1;
    snapshot.meta.timestamp_ns = 123456789;
    snapshot.meta.sequence_num = 1;

    Balance b;
    b.asset = kj::str("BTC");
    b.free = 1.0;
    b.locked = 0.5;
    snapshot.balances.add(kj::mv(b));

    // Save
    KJ_EXPECT(persistence.save_snapshot(snapshot));

    // Load
    auto loaded = persistence.load_latest_snapshot();
    KJ_IF_SOME(s, loaded) {
      KJ_EXPECT(s.meta.sequence_num == 1);
      KJ_EXPECT(s.balances.size() == 1);
      KJ_EXPECT(s.balances[0].asset == "BTC");
      KJ_EXPECT(s.balances[0].free == 1.0);
    }
    else {
      KJ_FAIL_EXPECT("Failed to load snapshot");
    }
  }

  // Cleanup
  auto fs = kj::newDiskFilesystem();
  kj::Path dirPath({"test_snapshots_persistence"});

  if (fs->getCurrent().exists(dirPath)) {
    // Open as writable to remove files
    auto dir = fs->getCurrent().openSubdir(dirPath, kj::WriteMode::MODIFY);
    for (const auto& name : dir->listNames()) {
      dir->remove(kj::Path(name));
    }
  }

  // Remove the directory itself
  if (fs->getCurrent().exists(dirPath)) {
    fs->getCurrent().remove(dirPath);
  }
}

} // namespace veloz::engine
