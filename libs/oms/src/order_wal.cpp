#include "veloz/oms/order_wal.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <kj/debug.h>

namespace veloz::oms {

namespace {

// Get current timestamp in nanoseconds
int64_t get_timestamp_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// Simple CRC32 implementation (polynomial 0xEDB88320)
uint32_t crc32(kj::ArrayPtr<const kj::byte> data) {
  uint32_t crc = 0xFFFFFFFF;
  for (auto byte : data) {
    crc ^= static_cast<uint8_t>(byte);
    for (int i = 0; i < 8; ++i) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xEDB88320;
      } else {
        crc >>= 1;
      }
    }
  }
  return ~crc;
}

// Write a length-prefixed string to buffer
size_t write_string(kj::ArrayPtr<kj::byte> buffer, kj::StringPtr str) {
  uint32_t len = static_cast<uint32_t>(str.size());
  std::memcpy(buffer.begin(), &len, sizeof(len));
  std::memcpy(buffer.begin() + sizeof(len), str.begin(), len);
  return sizeof(len) + len;
}

// Read a length-prefixed string from buffer
kj::String read_string(kj::ArrayPtr<const kj::byte> buffer, size_t& offset, size_t max_size) {
  if (offset + sizeof(uint32_t) > max_size) {
    return kj::heapString("");
  }
  uint32_t len;
  std::memcpy(&len, buffer.begin() + offset, sizeof(len));
  offset += sizeof(len);

  if (offset + len > max_size) {
    return kj::heapString("");
  }
  auto result = kj::heapString(reinterpret_cast<const char*>(buffer.begin() + offset), len);
  offset += len;
  return result;
}

// Write a double to buffer
size_t write_double(kj::ArrayPtr<kj::byte> buffer, double value) {
  std::memcpy(buffer.begin(), &value, sizeof(value));
  return sizeof(value);
}

// Read a double from buffer
double read_double(kj::ArrayPtr<const kj::byte> buffer, size_t& offset, size_t max_size) {
  if (offset + sizeof(double) > max_size) {
    return 0.0;
  }
  double value;
  std::memcpy(&value, buffer.begin() + offset, sizeof(value));
  offset += sizeof(value);
  return value;
}

// Write an int64 to buffer
size_t write_int64(kj::ArrayPtr<kj::byte> buffer, int64_t value) {
  std::memcpy(buffer.begin(), &value, sizeof(value));
  return sizeof(value);
}

// Read an int64 from buffer
int64_t read_int64(kj::ArrayPtr<const kj::byte> buffer, size_t& offset, size_t max_size) {
  if (offset + sizeof(int64_t) > max_size) {
    return 0;
  }
  int64_t value;
  std::memcpy(&value, buffer.begin() + offset, sizeof(value));
  offset += sizeof(value);
  return value;
}

// Write a uint8 to buffer
size_t write_uint8(kj::ArrayPtr<kj::byte> buffer, uint8_t value) {
  buffer[0] = static_cast<kj::byte>(value);
  return 1;
}

// Read a uint8 from buffer
uint8_t read_uint8(kj::ArrayPtr<const kj::byte> buffer, size_t& offset, size_t max_size) {
  if (offset >= max_size) {
    return 0;
  }
  uint8_t value = static_cast<uint8_t>(buffer[offset]);
  offset += 1;
  return value;
}

} // namespace

OrderWal::OrderWal(const kj::Directory& directory, WalConfig config)
    : config_(kj::mv(config)), directory_(directory) {
  // Find existing WAL files and determine starting sequence
  auto files = list_wal_files();
  if (files.size() > 0) {
    // Parse the last file to get the highest sequence
    const auto& lastFile = files.back();
    KJ_IF_SOME(seq, parse_filename(lastFile)) {
      auto lock = state_.lockExclusive();
      lock->sequence = seq;
      lock->stats.current_sequence = seq;
    }
  }

  // Open current WAL file
  open_current_file();

  KJ_LOG(INFO, "OrderWal initialized", config_.directory, state_.lockShared()->sequence);
}

OrderWal::~OrderWal() {
  close_current_file();
}

void OrderWal::open_current_file() {
  auto lock = state_.lockExclusive();
  open_current_file_locked(*lock);
}

void OrderWal::open_current_file_locked(State& state) {
  kj::String filename = generate_filename(state.sequence);

  // Try to open existing file or create new one
  auto path = kj::Path::parse(filename);
  auto maybeFile = directory_.tryOpenFile(path, kj::WriteMode::CREATE | kj::WriteMode::MODIFY);

  KJ_IF_SOME(file, maybeFile) {
    current_file_ = kj::mv(file);
    // Get current file size
    KJ_IF_SOME(fileRef, current_file_) {
      auto metadata = fileRef->stat();
      current_file_size_ = metadata.size;
    }
    current_file_start_sequence_ = state.sequence;
  }
  else {
    KJ_LOG(ERROR, "Failed to open WAL file", filename);
    state.healthy = false;
  }
}

void OrderWal::close_current_file() {
  KJ_IF_SOME(file, current_file_) {
    if (config_.sync_on_write) {
      file->sync();
    }
  }
  current_file_ = kj::none;
  current_file_size_ = 0;
}

bool OrderWal::needs_rotation() const {
  return current_file_size_ >= config_.max_file_size;
}

kj::String OrderWal::generate_filename(uint64_t sequence) const {
  // Format: prefix_NNNNNNNNNNNNNNNN.wal (16-digit hex sequence)
  // Manually format as 16-character hex string with leading zeros
  char hex_buf[17];
  snprintf(hex_buf, sizeof(hex_buf), "%016llx", static_cast<unsigned long long>(sequence));
  return kj::str(config_.file_prefix, "_", hex_buf, ".wal");
}

kj::Maybe<uint64_t> OrderWal::parse_filename(kj::StringPtr filename) const {
  // Expected format: prefix_NNNNNNNNNNNNNNNN.wal
  auto prefix = kj::str(config_.file_prefix, "_");
  if (!filename.startsWith(prefix) || !filename.endsWith(".wal"_kj)) {
    return kj::none;
  }

  auto hexPart = filename.slice(prefix.size(), filename.size() - 4);
  if (hexPart.size() != 16) {
    return kj::none;
  }

  // Parse hex string
  uint64_t result = 0;
  for (char c : hexPart) {
    result <<= 4;
    if (c >= '0' && c <= '9') {
      result |= (c - '0');
    } else if (c >= 'a' && c <= 'f') {
      result |= (c - 'a' + 10);
    } else if (c >= 'A' && c <= 'F') {
      result |= (c - 'A' + 10);
    } else {
      return kj::none;
    }
  }
  return result;
}

kj::Vector<kj::String> OrderWal::list_wal_files() const {
  kj::Vector<kj::String> result;

  auto entries = directory_.listEntries();
  for (const auto& entry : entries) {
    if (entry.type == kj::FsNode::Type::FILE) {
      auto prefix = kj::str(config_.file_prefix, "_");
      if (entry.name.startsWith(prefix) && entry.name.endsWith(".wal"_kj)) {
        result.add(kj::heapString(entry.name));
      }
    }
  }

  // Sort by name (which sorts by sequence due to hex format)
  // std::sort - KJ Vector lacks iterator compatibility for std::sort
  std::sort(result.begin(), result.end(),
            [](const kj::String& a, const kj::String& b) { return a < b; });

  return result;
}

uint32_t OrderWal::calculate_checksum(kj::ArrayPtr<const kj::byte> data) const {
  return crc32(data);
}

uint64_t OrderWal::write_entry(WalEntryType type, kj::ArrayPtr<const kj::byte> payload) {
  auto lock = state_.lockExclusive();

  if (!lock->healthy) {
    KJ_LOG(WARNING, "WAL is not healthy, skipping write");
    return lock->sequence;
  }

  // Check for rotation
  if (needs_rotation()) {
    close_current_file();
    open_current_file_locked(*lock);
    lock->stats.rotations++;
  }

  KJ_IF_SOME(file, current_file_) {
    // Prepare header
    WalEntryHeader header;
    header.magic = WalEntryHeader::MAGIC;
    header.version = WalEntryHeader::VERSION;
    header.sequence = ++lock->sequence;
    header.timestamp_ns = get_timestamp_ns();
    header.type = type;
    std::memset(header.reserved, 0, sizeof(header.reserved));
    header.payload_size = static_cast<uint32_t>(payload.size());
    header.checksum = 0; // Will be calculated

    // Calculate checksum over payload only (simpler and more reliable)
    header.checksum = calculate_checksum(payload);

    // Write header
    auto fullHeaderBytes = kj::arrayPtr(reinterpret_cast<const kj::byte*>(&header), sizeof(header));
    file->write(current_file_size_, fullHeaderBytes);
    current_file_size_ += sizeof(header);

    // Write payload
    if (payload.size() > 0) {
      file->write(current_file_size_, payload);
      current_file_size_ += payload.size();
    }

    // Sync if configured
    if (config_.sync_on_write) {
      file->sync();
    }

    // Update stats
    lock->stats.entries_written++;
    lock->stats.bytes_written += sizeof(header) + payload.size();
    lock->stats.current_sequence = lock->sequence;
    lock->entries_since_checkpoint++;

    return lock->sequence;
  }

  KJ_LOG(ERROR, "No WAL file open for writing");
  lock->healthy = false;
  return lock->sequence;
}

kj::Array<kj::byte>
OrderWal::serialize_order_request(const veloz::exec::PlaceOrderRequest& request) const {
  // Calculate size
  size_t size = 0;
  size += sizeof(uint32_t) + request.client_order_id.size(); // client_order_id
  size += sizeof(uint32_t) + request.symbol.value.size();    // symbol
  size += 1;                                                 // side
  size += 1;                                                 // type
  size += 1;                                                 // tif
  size += sizeof(double);                                    // qty
  size += 1;                                                 // has_price
  size += sizeof(double);                                    // price (if present)

  auto buffer = kj::heapArray<kj::byte>(size);
  size_t offset = 0;

  auto write_buffer = buffer.slice(offset, buffer.size());
  offset += write_string(write_buffer, request.client_order_id);
  write_buffer = buffer.slice(offset, buffer.size());
  offset += write_string(write_buffer, kj::StringPtr(request.symbol.value));
  write_buffer = buffer.slice(offset, buffer.size());
  offset += write_uint8(write_buffer, static_cast<uint8_t>(request.side));
  write_buffer = buffer.slice(offset, buffer.size());
  offset += write_uint8(write_buffer, static_cast<uint8_t>(request.type));
  write_buffer = buffer.slice(offset, buffer.size());
  offset += write_uint8(write_buffer, static_cast<uint8_t>(request.tif));
  write_buffer = buffer.slice(offset, buffer.size());
  offset += write_double(write_buffer, request.qty);

  KJ_IF_SOME(price, request.price) {
    write_buffer = buffer.slice(offset, buffer.size());
    offset += write_uint8(write_buffer, 1);
    write_buffer = buffer.slice(offset, buffer.size());
    offset += write_double(write_buffer, price);
  }
  else {
    write_buffer = buffer.slice(offset, buffer.size());
    offset += write_uint8(write_buffer, 0);
    write_buffer = buffer.slice(offset, buffer.size());
    offset += write_double(write_buffer, 0.0);
  }

  return buffer;
}

kj::Array<kj::byte> OrderWal::serialize_order_update(kj::StringPtr client_order_id,
                                                     kj::StringPtr venue_order_id,
                                                     kj::StringPtr status, kj::StringPtr reason,
                                                     int64_t ts_ns) const {
  size_t size = 0;
  size += sizeof(uint32_t) + client_order_id.size();
  size += sizeof(uint32_t) + venue_order_id.size();
  size += sizeof(uint32_t) + status.size();
  size += sizeof(uint32_t) + reason.size();
  size += sizeof(int64_t);

  auto buffer = kj::heapArray<kj::byte>(size);
  size_t offset = 0;

  auto write_buffer = buffer.slice(offset, buffer.size());
  offset += write_string(write_buffer, client_order_id);
  write_buffer = buffer.slice(offset, buffer.size());
  offset += write_string(write_buffer, venue_order_id);
  write_buffer = buffer.slice(offset, buffer.size());
  offset += write_string(write_buffer, status);
  write_buffer = buffer.slice(offset, buffer.size());
  offset += write_string(write_buffer, reason);
  write_buffer = buffer.slice(offset, buffer.size());
  offset += write_int64(write_buffer, ts_ns);

  return buffer;
}

kj::Array<kj::byte> OrderWal::serialize_order_fill(kj::StringPtr client_order_id,
                                                   kj::StringPtr symbol, double qty, double price,
                                                   int64_t ts_ns) const {
  size_t size = 0;
  size += sizeof(uint32_t) + client_order_id.size();
  size += sizeof(uint32_t) + symbol.size();
  size += sizeof(double);  // qty
  size += sizeof(double);  // price
  size += sizeof(int64_t); // ts_ns

  auto buffer = kj::heapArray<kj::byte>(size);
  size_t offset = 0;

  auto write_buffer = buffer.slice(offset, buffer.size());
  offset += write_string(write_buffer, client_order_id);
  write_buffer = buffer.slice(offset, buffer.size());
  offset += write_string(write_buffer, symbol);
  write_buffer = buffer.slice(offset, buffer.size());
  offset += write_double(write_buffer, qty);
  write_buffer = buffer.slice(offset, buffer.size());
  offset += write_double(write_buffer, price);
  write_buffer = buffer.slice(offset, buffer.size());
  offset += write_int64(write_buffer, ts_ns);

  return buffer;
}

kj::Array<kj::byte> OrderWal::serialize_order_cancel(kj::StringPtr client_order_id,
                                                     kj::StringPtr reason, int64_t ts_ns) const {
  size_t size = 0;
  size += sizeof(uint32_t) + client_order_id.size();
  size += sizeof(uint32_t) + reason.size();
  size += sizeof(int64_t);

  auto buffer = kj::heapArray<kj::byte>(size);
  size_t offset = 0;

  auto write_buffer = buffer.slice(offset, buffer.size());
  offset += write_string(write_buffer, client_order_id);
  write_buffer = buffer.slice(offset, buffer.size());
  offset += write_string(write_buffer, reason);
  write_buffer = buffer.slice(offset, buffer.size());
  offset += write_int64(write_buffer, ts_ns);

  return buffer;
}

kj::Array<kj::byte> OrderWal::serialize_checkpoint(const OrderStore& store) const {
  auto orders = store.list();

  // Calculate total size
  size_t size = sizeof(uint32_t); // order count
  for (const auto& order : orders) {
    size += sizeof(uint32_t) + order.client_order_id.size();
    size += sizeof(uint32_t) + order.symbol.size();
    size += sizeof(uint32_t) + order.side.size();
    size += 1;              // has_order_qty
    size += sizeof(double); // order_qty
    size += 1;              // has_limit_price
    size += sizeof(double); // limit_price
    size += sizeof(double); // executed_qty
    size += sizeof(double); // avg_price
    size += sizeof(uint32_t) + order.venue_order_id.size();
    size += sizeof(uint32_t) + order.status.size();
    size += sizeof(uint32_t) + order.reason.size();
    size += sizeof(int64_t); // last_ts_ns
    size += sizeof(int64_t); // created_ts_ns
  }

  auto buffer = kj::heapArray<kj::byte>(size);
  size_t offset = 0;

  // Write order count
  uint32_t count = static_cast<uint32_t>(orders.size());
  std::memcpy(buffer.begin() + offset, &count, sizeof(count));
  offset += sizeof(count);

  // Write each order
  for (const auto& order : orders) {
    auto write_buffer = buffer.slice(offset, buffer.size());
    offset += write_string(write_buffer, order.client_order_id);
    write_buffer = buffer.slice(offset, buffer.size());
    offset += write_string(write_buffer, order.symbol);
    write_buffer = buffer.slice(offset, buffer.size());
    offset += write_string(write_buffer, order.side);

    KJ_IF_SOME(qty, order.order_qty) {
      write_buffer = buffer.slice(offset, buffer.size());
      offset += write_uint8(write_buffer, 1);
      write_buffer = buffer.slice(offset, buffer.size());
      offset += write_double(write_buffer, qty);
    }
    else {
      write_buffer = buffer.slice(offset, buffer.size());
      offset += write_uint8(write_buffer, 0);
      write_buffer = buffer.slice(offset, buffer.size());
      offset += write_double(write_buffer, 0.0);
    }

    KJ_IF_SOME(price, order.limit_price) {
      write_buffer = buffer.slice(offset, buffer.size());
      offset += write_uint8(write_buffer, 1);
      write_buffer = buffer.slice(offset, buffer.size());
      offset += write_double(write_buffer, price);
    }
    else {
      write_buffer = buffer.slice(offset, buffer.size());
      offset += write_uint8(write_buffer, 0);
      write_buffer = buffer.slice(offset, buffer.size());
      offset += write_double(write_buffer, 0.0);
    }

    write_buffer = buffer.slice(offset, buffer.size());
    offset += write_double(write_buffer, order.executed_qty);
    write_buffer = buffer.slice(offset, buffer.size());
    offset += write_double(write_buffer, order.avg_price);
    write_buffer = buffer.slice(offset, buffer.size());
    offset += write_string(write_buffer, order.venue_order_id);
    write_buffer = buffer.slice(offset, buffer.size());
    offset += write_string(write_buffer, order.status);
    write_buffer = buffer.slice(offset, buffer.size());
    offset += write_string(write_buffer, order.reason);
    write_buffer = buffer.slice(offset, buffer.size());
    offset += write_int64(write_buffer, order.last_ts_ns);
    write_buffer = buffer.slice(offset, buffer.size());
    offset += write_int64(write_buffer, order.created_ts_ns);
  }

  return buffer;
}

void OrderWal::deserialize_order_new(kj::ArrayPtr<const kj::byte> payload,
                                     OrderStore& store) const {
  size_t offset = 0;
  size_t max_size = payload.size();

  auto client_order_id = read_string(payload, offset, max_size);
  auto symbol_str = read_string(payload, offset, max_size);
  auto side = static_cast<veloz::exec::OrderSide>(read_uint8(payload, offset, max_size));
  auto type = static_cast<veloz::exec::OrderType>(read_uint8(payload, offset, max_size));
  auto tif = static_cast<veloz::exec::TimeInForce>(read_uint8(payload, offset, max_size));
  auto qty = read_double(payload, offset, max_size);
  auto has_price = read_uint8(payload, offset, max_size);
  auto price = read_double(payload, offset, max_size);

  if (client_order_id.size() == 0) {
    return;
  }
  if (store.get(client_order_id) != kj::none) {
    KJ_LOG(WARNING, "Skipping duplicate OrderNew during WAL replay", client_order_id);
    return;
  }

  veloz::exec::PlaceOrderRequest request;
  request.client_order_id = kj::mv(client_order_id);
  request.symbol = veloz::common::SymbolId(kj::mv(symbol_str));
  request.side = side;
  request.type = type;
  request.tif = tif;
  request.qty = qty;
  if (has_price) {
    request.price = price;
  }

  store.note_order_params(request);
}

void OrderWal::deserialize_order_update(kj::ArrayPtr<const kj::byte> payload,
                                        OrderStore& store) const {
  size_t offset = 0;
  size_t max_size = payload.size();

  auto client_order_id = read_string(payload, offset, max_size);
  auto venue_order_id = read_string(payload, offset, max_size);
  auto status = read_string(payload, offset, max_size);
  auto reason = read_string(payload, offset, max_size);
  auto ts_ns = read_int64(payload, offset, max_size);

  if (client_order_id.size() == 0) {
    return;
  }
  KJ_IF_SOME(existing, store.get(client_order_id)) {
    if (ts_ns > 0 && existing.last_ts_ns > 0 && ts_ns <= existing.last_ts_ns) {
      KJ_LOG(WARNING, "Skipping out-of-order OrderUpdate during WAL replay", client_order_id,
             ts_ns, existing.last_ts_ns);
      return;
    }
  }
  else {
    KJ_LOG(WARNING, "OrderUpdate for unknown order during WAL replay", client_order_id);
  }

  store.apply_order_update(client_order_id, ""_kj, ""_kj, venue_order_id, status, reason, ts_ns);
}

void OrderWal::deserialize_order_fill(kj::ArrayPtr<const kj::byte> payload,
                                      OrderStore& store) const {
  size_t offset = 0;
  size_t max_size = payload.size();

  auto client_order_id = read_string(payload, offset, max_size);
  auto symbol = read_string(payload, offset, max_size);
  auto qty = read_double(payload, offset, max_size);
  auto price = read_double(payload, offset, max_size);
  auto ts_ns = read_int64(payload, offset, max_size);

  if (client_order_id.size() == 0) {
    return;
  }
  KJ_IF_SOME(existing, store.get(client_order_id)) {
    if (ts_ns > 0 && existing.last_ts_ns > 0 && ts_ns <= existing.last_ts_ns) {
      KJ_LOG(WARNING, "Skipping out-of-order OrderFill during WAL replay", client_order_id, ts_ns,
             existing.last_ts_ns);
      return;
    }
  }
  else {
    KJ_LOG(WARNING, "OrderFill for unknown order during WAL replay", client_order_id);
  }

  store.apply_fill(client_order_id, symbol, qty, price, ts_ns);
}

void OrderWal::deserialize_order_cancel(kj::ArrayPtr<const kj::byte> payload,
                                       OrderStore& store) const {
  size_t offset = 0;
  size_t max_size = payload.size();

  auto client_order_id = read_string(payload, offset, max_size);
  auto reason = read_string(payload, offset, max_size);
  auto ts_ns = read_int64(payload, offset, max_size);

  if (client_order_id.size() == 0) {
    return;
  }
  KJ_IF_SOME(existing, store.get(client_order_id)) {
    if (ts_ns > 0 && existing.last_ts_ns > 0 && ts_ns <= existing.last_ts_ns) {
      KJ_LOG(WARNING, "Skipping out-of-order OrderCancel during WAL replay", client_order_id,
             ts_ns, existing.last_ts_ns);
      return;
    }
  }
  else {
    KJ_LOG(WARNING, "OrderCancel for unknown order during WAL replay", client_order_id);
  }

  store.apply_order_update(client_order_id, ""_kj, ""_kj, ""_kj, "CANCELED"_kj, reason, ts_ns);
}

void OrderWal::deserialize_checkpoint(kj::ArrayPtr<const kj::byte> payload,
                                      OrderStore& store) const {
  // Clear existing state and restore from checkpoint
  store.clear();

  size_t offset = 0;
  size_t max_size = payload.size();

  if (offset + sizeof(uint32_t) > max_size) {
    return;
  }

  uint32_t count;
  std::memcpy(&count, payload.begin() + offset, sizeof(count));
  offset += sizeof(count);

  for (uint32_t i = 0; i < count && offset < max_size; ++i) {
    auto client_order_id = read_string(payload, offset, max_size);
    auto symbol = read_string(payload, offset, max_size);
    auto side = read_string(payload, offset, max_size);
    auto has_order_qty = read_uint8(payload, offset, max_size);
    auto order_qty = read_double(payload, offset, max_size);
    auto has_limit_price = read_uint8(payload, offset, max_size);
    auto limit_price = read_double(payload, offset, max_size);
    auto executed_qty = read_double(payload, offset, max_size);
    auto avg_price = read_double(payload, offset, max_size);
    auto venue_order_id = read_string(payload, offset, max_size);
    auto status = read_string(payload, offset, max_size);
    auto reason = read_string(payload, offset, max_size);
    auto last_ts_ns = read_int64(payload, offset, max_size);
    auto created_ts_ns = read_int64(payload, offset, max_size);

    // Create order request to populate store
    veloz::exec::PlaceOrderRequest request;
    request.client_order_id = kj::heapString(client_order_id);
    request.symbol = veloz::common::SymbolId(kj::mv(symbol));
    request.side = (side == "SELL"_kj) ? veloz::exec::OrderSide::Sell : veloz::exec::OrderSide::Buy;
    request.qty = has_order_qty ? order_qty : 0.0;
    if (has_limit_price) {
      request.price = limit_price;
    }

    store.note_order_params(request);

    // Apply updates
    store.apply_order_update(client_order_id, symbol, side, venue_order_id, status, reason,
                             last_ts_ns);

    // Apply fills if any
    if (executed_qty > 0.0) {
      store.apply_fill(client_order_id, symbol, executed_qty, avg_price, last_ts_ns);
    }
  }
}

uint64_t OrderWal::log_order_new(const veloz::exec::PlaceOrderRequest& request) {
  auto payload = serialize_order_request(request);
  return write_entry(WalEntryType::OrderNew, payload);
}

uint64_t OrderWal::log_order_update(kj::StringPtr client_order_id, kj::StringPtr venue_order_id,
                                    kj::StringPtr status, kj::StringPtr reason, int64_t ts_ns) {
  auto payload = serialize_order_update(client_order_id, venue_order_id, status, reason, ts_ns);
  return write_entry(WalEntryType::OrderUpdate, payload);
}

uint64_t OrderWal::log_order_fill(kj::StringPtr client_order_id, kj::StringPtr symbol, double qty,
                                  double price, int64_t ts_ns) {
  auto payload = serialize_order_fill(client_order_id, symbol, qty, price, ts_ns);
  return write_entry(WalEntryType::OrderFill, payload);
}

uint64_t OrderWal::log_order_cancel(kj::StringPtr client_order_id, kj::StringPtr reason,
                                    int64_t ts_ns) {
  auto payload = serialize_order_cancel(client_order_id, reason, ts_ns);
  return write_entry(WalEntryType::OrderCancel, payload);
}

uint64_t OrderWal::write_checkpoint(const OrderStore& store) {
  auto payload = serialize_checkpoint(store);
  auto seq = write_entry(WalEntryType::Checkpoint, payload);

  auto lock = state_.lockExclusive();
  lock->entries_since_checkpoint = 0;
  lock->stats.checkpoints++;

  return seq;
}

void OrderWal::replay(WalReplayCallback callback) {
  auto files = list_wal_files();
  uint64_t last_sequence = 0;

  for (const auto& filename : files) {
    auto path = kj::Path::parse(filename);
    auto maybeFile = directory_.tryOpenFile(path);

    KJ_IF_SOME(file, maybeFile) {
      auto metadata = file->stat();
      auto data = file->readAllBytes();

      size_t offset = 0;
      while (offset + sizeof(WalEntryHeader) <= data.size()) {
        WalEntryHeader header;
        std::memcpy(&header, data.begin() + offset, sizeof(header));

        // Validate header
        if (header.magic != WalEntryHeader::MAGIC) {
          KJ_LOG(WARNING, "Invalid WAL entry magic at offset", offset);
          auto lock = state_.lockExclusive();
          lock->stats.corrupted_entries++;
          break;
        }

        if (header.version != WalEntryHeader::VERSION) {
          KJ_LOG(WARNING, "Unsupported WAL version", header.version);
          auto lock = state_.lockExclusive();
          lock->stats.corrupted_entries++;
          break;
        }

        // Check if we have enough data for payload
        if (offset + sizeof(header) + header.payload_size > data.size()) {
          KJ_LOG(WARNING, "Truncated WAL entry at offset", offset);
          auto lock = state_.lockExclusive();
          lock->stats.corrupted_entries++;
          break;
        }

        // Verify checksum over payload only
        auto payloadBytes =
            kj::arrayPtr(data.begin() + offset + sizeof(header), header.payload_size);

        if (calculate_checksum(payloadBytes) != header.checksum) {
          KJ_LOG(WARNING, "WAL entry checksum mismatch at offset", offset);
          auto lock = state_.lockExclusive();
          lock->stats.corrupted_entries++;
          offset += sizeof(header) + header.payload_size;
          continue;
        }

        if (header.sequence <= last_sequence) {
          KJ_LOG(WARNING, "Skipping duplicate/out-of-order WAL entry", header.sequence,
                 last_sequence);
          offset += sizeof(header) + header.payload_size;
          continue;
        }
        if (last_sequence > 0 && header.sequence > last_sequence + 1) {
          KJ_LOG(WARNING, "WAL sequence gap detected", last_sequence, header.sequence);
        }

        // Call callback with entry
        callback(header.type, payloadBytes);

        // Update stats
        {
          auto lock = state_.lockExclusive();
          lock->stats.entries_replayed++;
          lock->stats.bytes_replayed += sizeof(header) + header.payload_size;
          lock->sequence = header.sequence;
          lock->stats.current_sequence = header.sequence;
        }

        last_sequence = header.sequence;
        offset += sizeof(header) + header.payload_size;
      }
    }
  }
}

void OrderWal::replay_into(OrderStore& store) {
  replay([this, &store](WalEntryType type, kj::ArrayPtr<const kj::byte> payload) {
    switch (type) {
    case WalEntryType::OrderNew:
      deserialize_order_new(payload, store);
      break;
    case WalEntryType::OrderUpdate:
      deserialize_order_update(payload, store);
      break;
    case WalEntryType::OrderFill:
      deserialize_order_fill(payload, store);
      break;
    case WalEntryType::OrderCancel:
      deserialize_order_cancel(payload, store);
      break;
    case WalEntryType::Checkpoint:
      deserialize_checkpoint(payload, store);
      break;
    case WalEntryType::Rotation:
      // Rotation markers are informational only
      break;
    }
  });
}

void OrderWal::sync() {
  KJ_IF_SOME(file, current_file_) {
    file->sync();
  }
}

void OrderWal::rotate() {
  // Write rotation marker
  write_entry(WalEntryType::Rotation, kj::ArrayPtr<const kj::byte>());

  // Close current file and open new one
  close_current_file();
  open_current_file();

  auto lock = state_.lockExclusive();
  lock->stats.rotations++;
}

WalStats OrderWal::stats() const {
  return state_.lockShared()->stats;
}

uint64_t OrderWal::current_sequence() const {
  return state_.lockShared()->sequence;
}

bool OrderWal::is_healthy() const {
  return state_.lockShared()->healthy;
}

void OrderWal::cleanup_old_files() {
  auto files = list_wal_files();

  if (files.size() <= config_.max_files) {
    return;
  }

  // Remove oldest files
  size_t to_remove = files.size() - config_.max_files;
  for (size_t i = 0; i < to_remove; ++i) {
    auto path = kj::Path::parse(files[i]);
    if (directory_.tryRemove(path)) {
      KJ_LOG(INFO, "Removed old WAL file", files[i]);
    }
  }
}

} // namespace veloz::oms
