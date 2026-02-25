/**
 * VeloZ Configuration Migration System
 *
 * Handles migration of user configuration between versions.
 * Each migration is a function that transforms config from one version to the next.
 */

const fs = require('fs');
const path = require('path');
const semver = require('semver');

// Migration registry
const migrations = new Map();

/**
 * Register a migration
 * @param {string} fromVersion - Source version
 * @param {string} toVersion - Target version
 * @param {Function} migrateFn - Migration function
 * @param {Function} rollbackFn - Rollback function
 * @param {Function} validateFn - Validation function
 */
function registerMigration(fromVersion, toVersion, migrateFn, rollbackFn, validateFn) {
  const key = `${fromVersion}->${toVersion}`;
  migrations.set(key, {
    fromVersion,
    toVersion,
    migrate: migrateFn,
    rollback: rollbackFn,
    validate: validateFn,
  });
}

/**
 * Get migration path between two versions
 * @param {string} fromVersion - Current version
 * @param {string} toVersion - Target version
 * @returns {Array} Array of migration objects in order
 */
function getMigrationPath(fromVersion, toVersion) {
  const path = [];
  let current = fromVersion;

  // Find all migrations that can take us from fromVersion to toVersion
  const sortedMigrations = Array.from(migrations.values()).sort((a, b) =>
    semver.compare(a.fromVersion, b.fromVersion)
  );

  while (semver.lt(current, toVersion)) {
    const nextMigration = sortedMigrations.find(
      (m) => semver.eq(m.fromVersion, current) && semver.lte(m.toVersion, toVersion)
    );

    if (!nextMigration) {
      throw new Error(`No migration path from ${current} to ${toVersion}`);
    }

    path.push(nextMigration);
    current = nextMigration.toVersion;
  }

  return path;
}

/**
 * Backup configuration
 * @param {string} configPath - Path to config file
 * @param {string} backupDir - Backup directory
 * @param {string} version - Current version
 */
function backupConfig(configPath, backupDir, version) {
  if (!fs.existsSync(configPath)) {
    return null;
  }

  const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
  const backupPath = path.join(backupDir, `config-${version}-${timestamp}.json`);

  // Ensure backup directory exists
  if (!fs.existsSync(backupDir)) {
    fs.mkdirSync(backupDir, { recursive: true });
  }

  // Copy config to backup
  fs.copyFileSync(configPath, backupPath);

  // Also backup strategies directory if it exists
  const strategiesDir = path.join(path.dirname(configPath), 'strategies');
  if (fs.existsSync(strategiesDir)) {
    const strategiesBackup = path.join(backupDir, `strategies-${version}-${timestamp}`);
    copyDirectory(strategiesDir, strategiesBackup);
  }

  return backupPath;
}

/**
 * Copy directory recursively
 */
function copyDirectory(src, dest) {
  if (!fs.existsSync(dest)) {
    fs.mkdirSync(dest, { recursive: true });
  }

  const entries = fs.readdirSync(src, { withFileTypes: true });

  for (const entry of entries) {
    const srcPath = path.join(src, entry.name);
    const destPath = path.join(dest, entry.name);

    if (entry.isDirectory()) {
      copyDirectory(srcPath, destPath);
    } else {
      fs.copyFileSync(srcPath, destPath);
    }
  }
}

/**
 * Restore configuration from backup
 * @param {string} backupPath - Path to backup file
 * @param {string} configPath - Path to restore to
 */
function restoreConfig(backupPath, configPath) {
  if (!fs.existsSync(backupPath)) {
    throw new Error(`Backup not found: ${backupPath}`);
  }

  fs.copyFileSync(backupPath, configPath);
}

/**
 * Run migrations
 * @param {Object} config - Current configuration
 * @param {string} fromVersion - Current version
 * @param {string} toVersion - Target version
 * @returns {Object} Migrated configuration
 */
function runMigrations(config, fromVersion, toVersion) {
  if (semver.eq(fromVersion, toVersion)) {
    return config;
  }

  if (semver.gt(fromVersion, toVersion)) {
    // Downgrade - run rollbacks
    return runRollbacks(config, fromVersion, toVersion);
  }

  const migrationPath = getMigrationPath(fromVersion, toVersion);
  let currentConfig = { ...config };

  for (const migration of migrationPath) {
    console.log(`Migrating from ${migration.fromVersion} to ${migration.toVersion}`);

    try {
      currentConfig = migration.migrate(currentConfig);

      // Validate migrated config
      const validation = migration.validate(currentConfig);
      if (!validation.valid) {
        throw new Error(`Validation failed: ${validation.errors.join(', ')}`);
      }
    } catch (error) {
      console.error(`Migration failed: ${error.message}`);
      throw error;
    }
  }

  // Update version in config
  currentConfig._version = toVersion;

  return currentConfig;
}

/**
 * Run rollbacks (for downgrade)
 * @param {Object} config - Current configuration
 * @param {string} fromVersion - Current version
 * @param {string} toVersion - Target version
 * @returns {Object} Rolled back configuration
 */
function runRollbacks(config, fromVersion, toVersion) {
  // Get reverse migration path
  const migrationPath = getMigrationPath(toVersion, fromVersion).reverse();
  let currentConfig = { ...config };

  for (const migration of migrationPath) {
    console.log(`Rolling back from ${migration.toVersion} to ${migration.fromVersion}`);

    try {
      currentConfig = migration.rollback(currentConfig);
    } catch (error) {
      console.error(`Rollback failed: ${error.message}`);
      throw error;
    }
  }

  currentConfig._version = toVersion;

  return currentConfig;
}

/**
 * List available backups
 * @param {string} backupDir - Backup directory
 * @returns {Array} List of backup files with metadata
 */
function listBackups(backupDir) {
  if (!fs.existsSync(backupDir)) {
    return [];
  }

  const files = fs.readdirSync(backupDir);
  const backups = [];

  for (const file of files) {
    if (file.startsWith('config-') && file.endsWith('.json')) {
      const match = file.match(/config-(.+?)-(\d{4}-\d{2}-\d{2}T.+?)\.json/);
      if (match) {
        const filePath = path.join(backupDir, file);
        const stats = fs.statSync(filePath);
        backups.push({
          file,
          path: filePath,
          version: match[1],
          timestamp: match[2].replace(/-/g, ':').replace('T', ' '),
          size: stats.size,
          created: stats.birthtime,
        });
      }
    }
  }

  return backups.sort((a, b) => b.created - a.created);
}

/**
 * Clean old backups (keep last N)
 * @param {string} backupDir - Backup directory
 * @param {number} keepCount - Number of backups to keep
 */
function cleanOldBackups(backupDir, keepCount = 5) {
  const backups = listBackups(backupDir);

  if (backups.length <= keepCount) {
    return;
  }

  const toDelete = backups.slice(keepCount);
  for (const backup of toDelete) {
    fs.unlinkSync(backup.path);
    console.log(`Deleted old backup: ${backup.file}`);
  }
}

// ============================================================================
// Migration Definitions
// ============================================================================

// Migration: 1.0.0 -> 1.1.0
registerMigration(
  '1.0.0',
  '1.1.0',
  // Migrate
  (config) => {
    return {
      ...config,
      // Add new charting configuration
      charting: {
        defaultTimeframe: '1h',
        indicators: [],
        theme: 'dark',
      },
      // Transform risk settings
      risk: {
        ...config.risk,
        dailyLossLimit: config.risk?.maxLoss ?? 0.1,
        positionSizeLimit: config.risk?.maxPositionSize ?? 0.25,
      },
      // Add notification settings
      notifications: {
        enabled: true,
        sound: true,
        desktop: true,
        email: false,
      },
    };
  },
  // Rollback
  (config) => {
    const { charting, notifications, ...rest } = config;
    return {
      ...rest,
      risk: {
        maxLoss: config.risk?.dailyLossLimit ?? 0.1,
        maxPositionSize: config.risk?.positionSizeLimit ?? 0.25,
      },
    };
  },
  // Validate
  (config) => {
    const errors = [];

    if (!config.charting) {
      errors.push('Missing charting configuration');
    }
    if (!config.charting?.defaultTimeframe) {
      errors.push('Missing charting.defaultTimeframe');
    }
    if (typeof config.risk?.dailyLossLimit !== 'number') {
      errors.push('Invalid risk.dailyLossLimit');
    }

    return { valid: errors.length === 0, errors };
  }
);

// Migration: 1.1.0 -> 1.2.0
registerMigration(
  '1.1.0',
  '1.2.0',
  // Migrate
  (config) => {
    return {
      ...config,
      // Add strategy marketplace settings
      marketplace: {
        enabled: true,
        autoUpdate: true,
        trustedPublishers: [],
      },
      // Add backup settings
      backup: {
        autoBackup: true,
        backupInterval: 'daily',
        maxBackups: 10,
      },
      // Migrate exchange settings to new format
      exchanges: (config.exchanges || []).map((exchange) => ({
        ...exchange,
        rateLimit: exchange.rateLimit ?? { requests: 10, interval: 1000 },
        testnet: exchange.testnet ?? false,
      })),
    };
  },
  // Rollback
  (config) => {
    const { marketplace, backup, ...rest } = config;
    return {
      ...rest,
      exchanges: (config.exchanges || []).map((exchange) => {
        const { rateLimit, testnet, ...exchangeRest } = exchange;
        return exchangeRest;
      }),
    };
  },
  // Validate
  (config) => {
    const errors = [];

    if (!config.marketplace) {
      errors.push('Missing marketplace configuration');
    }
    if (!config.backup) {
      errors.push('Missing backup configuration');
    }
    if (!Array.isArray(config.exchanges)) {
      errors.push('Invalid exchanges configuration');
    }

    return { valid: errors.length === 0, errors };
  }
);

module.exports = {
  registerMigration,
  getMigrationPath,
  backupConfig,
  restoreConfig,
  runMigrations,
  runRollbacks,
  listBackups,
  cleanOldBackups,
  migrations,
};
