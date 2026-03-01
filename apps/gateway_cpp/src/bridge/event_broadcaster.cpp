#include "event_broadcaster.h"

#include <atomic>
#include <chrono>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/exception.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway::bridge {

// ============================================================================
// SseSubscription Implementation
// ============================================================================

SseSubscription::SseSubscription(uint64_t start_id) {
  last_id_.store(start_id, std::memory_order_release);
}

SseSubscription::~SseSubscription() noexcept {
  close();
}

kj::Promise<kj::Maybe<SseEvent>> SseSubscription::next_event() {
  // Check if already closed first
  if (closed_.load(std::memory_order_acquire)) {
    // Already closed, return a promise resolved to none
    return kj::Promise<kj::Maybe<SseEvent>>(kj::none);
  }

  auto lock = state_.lockExclusive();
  if (lock->pending_head < lock->pending.size()) {
    auto event = kj::mv(lock->pending[lock->pending_head]);
    ++lock->pending_head;
    if (lock->pending_head == lock->pending.size()) {
      lock->pending.clear();
      lock->pending_head = 0;
    }
    last_id_.store(event.id, std::memory_order_release);
    kj::Maybe<SseEvent> maybe_event = kj::mv(event);
    return kj::Promise<kj::Maybe<SseEvent>>(kj::mv(maybe_event));
  }

  auto paf = kj::newPromiseAndCrossThreadFulfiller<kj::Maybe<SseEvent>>();
  lock->fulfiller = kj::mv(paf.fulfiller);

  return kj::mv(paf.promise);
}

uint64_t SseSubscription::last_id() const {
  return last_id_.load(std::memory_order_acquire);
}

bool SseSubscription::is_closed() const {
  return closed_.load(std::memory_order_acquire);
}

void SseSubscription::close() {
  if (closed_.exchange(true, std::memory_order_acq_rel)) {
    // Already closed
    return;
  }
  fulfill_none(); // Signal end of stream
}

void SseSubscription::fulfill_event(SseEvent event) {
  if (closed_.load(std::memory_order_acquire)) {
    return;
  }

  auto lock = state_.lockExclusive();
  KJ_IF_SOME(fulfiller, lock->fulfiller) {
    // Update last_id
    last_id_.store(event.id, std::memory_order_release);

    // Fulfill the promise with the event
    fulfiller->fulfill(kj::mv(event));
    lock->fulfiller = kj::none;
    return;
  }

  lock->pending.add(kj::mv(event));
}

void SseSubscription::fulfill_none() {
  auto lock = state_.lockExclusive();
  lock->pending.clear();
  lock->pending_head = 0;
  KJ_IF_SOME(fulfiller, lock->fulfiller) {
    fulfiller->fulfill(kj::none);
    lock->fulfiller = kj::none;
  }

  // Signal end of stream - remove from broadcaster
  if (broadcaster_ != nullptr) {
    broadcaster_->remove_subscription(this);
    broadcaster_ = nullptr;
  }
}

// ============================================================================
// EventBroadcaster Implementation
// ============================================================================

EventBroadcaster::EventBroadcaster(const EventBroadcasterConfig& config) : config_(config) {
  // Reserve space in history
  auto history_lock = history_.lockExclusive();
  history_lock->reserve(config_.history_size);
}

EventBroadcaster::~EventBroadcaster() {
  // Close all subscriptions
  auto subs_lock = subscriptions_.lockExclusive();

  for (auto* sub : *subs_lock) {
    if (sub != nullptr) {
      sub->broadcaster_ = nullptr; // Prevent double-remove
      sub->close();
    }
  }
  subs_lock->clear();
}

kj::Own<SseSubscription> EventBroadcaster::subscribe(uint64_t last_id) {
  // Create subscription
  auto subscription = kj::refcounted<SseSubscription>(last_id);
  auto* sub_ptr = subscription.get();

  // Set back-reference
  sub_ptr->broadcaster_ = this;

  auto subs_lock = subscriptions_.lockExclusive();

  // Check for max subscriptions
  if (subs_lock->size() >= config_.max_subscriptions) {
    sub_ptr->broadcaster_ = nullptr;
    KJ_FAIL_REQUIRE("Maximum subscriptions reached");
  }

  subs_lock->add(sub_ptr);
  stats_.total_subscriptions.fetch_add(1, std::memory_order_relaxed);

  return kj::mv(subscription);
}

void EventBroadcaster::broadcast(SseEvent event) {
  // Assign an ID if not already set
  if (event.id == 0) {
    event.id = next_id_.fetch_add(1, std::memory_order_acq_rel);
  }

  // Set timestamp if not already set
  if (event.timestamp_ns == 0) {
    auto now = std::chrono::steady_clock::now();
    event.timestamp_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
  }

  // Deliver to all subscriptions first (before moving to history)
  deliver_event(event);

  // Add to history (moves the event)
  add_to_history(kj::mv(event));

  stats_.events_broadcast.fetch_add(1, std::memory_order_relaxed);
}

void EventBroadcaster::broadcast_batch(kj::Vector<SseEvent> events) {
  if (events.empty()) {
    return;
  }

  // Assign IDs and timestamps to all events
  for (auto& event : events) {
    if (event.id == 0) {
      event.id = next_id_.fetch_add(1, std::memory_order_acq_rel);
    }
    if (event.timestamp_ns == 0) {
      auto now = std::chrono::steady_clock::now();
      event.timestamp_ns =
          std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    }
  }

  // Add all to history
  for (const auto& event : events) {
    add_to_history(SseEvent(event.id, event.type, event.timestamp_ns, kj::str(event.data)));
  }

  // Deliver events
  deliver_batch(events.asPtr());

  stats_.events_broadcast.fetch_add(events.size(), std::memory_order_relaxed);
}

void EventBroadcaster::remove_subscription(SseSubscription* sub) {
  if (sub == nullptr) {
    return;
  }

  auto subs_lock = subscriptions_.lockExclusive();

  // Find and remove using swap-and-pop pattern
  size_t write_idx = 0;
  for (size_t read_idx = 0; read_idx < subs_lock->size(); ++read_idx) {
    if ((*subs_lock)[read_idx] != sub) {
      if (write_idx != read_idx) {
        (*subs_lock)[write_idx] = (*subs_lock)[read_idx];
      }
      ++write_idx;
    }
  }

  // Remove trailing elements
  while (subs_lock->size() > write_idx) {
    subs_lock->removeLast();
  }
}

void EventBroadcaster::deliver_event(const SseEvent& event) {
  auto subs_lock = subscriptions_.lockExclusive();

  // Deliver to all active subscriptions
  // Use swap-and-pop to remove closed subscriptions
  size_t write_idx = 0;
  for (size_t read_idx = 0; read_idx < subs_lock->size(); ++read_idx) {
    auto* sub = (*subs_lock)[read_idx];
    if (sub != nullptr && !sub->is_closed()) {
      sub->fulfill_event(SseEvent(event.id, event.type, event.timestamp_ns, kj::str(event.data)));
      if (write_idx != read_idx) {
        (*subs_lock)[write_idx] = (*subs_lock)[read_idx];
      }
      ++write_idx;
    }
  }

  // Remove closed subscriptions
  while (subs_lock->size() > write_idx) {
    subs_lock->removeLast();
  }
}

void EventBroadcaster::deliver_batch(kj::ArrayPtr<const SseEvent> events) {
  auto subs_lock = subscriptions_.lockExclusive();

  // Deliver all events to all active subscriptions
  size_t write_idx = 0;
  for (size_t read_idx = 0; read_idx < subs_lock->size(); ++read_idx) {
    auto* sub = (*subs_lock)[read_idx];
    if (sub != nullptr && !sub->is_closed()) {
      for (const auto& event : events) {
        sub->fulfill_event(SseEvent(event.id, event.type, event.timestamp_ns, kj::str(event.data)));
      }
      if (write_idx != read_idx) {
        (*subs_lock)[write_idx] = (*subs_lock)[read_idx];
      }
      ++write_idx;
    }
  }

  // Remove closed subscriptions
  while (subs_lock->size() > write_idx) {
    subs_lock->removeLast();
  }
}

void EventBroadcaster::add_to_history(SseEvent event) {
  auto history_lock = history_.lockExclusive();

  // Add new entry
  HistoryEntry entry(event.id, kj::mv(event));
  history_lock->add(kj::mv(entry));

  // Manage circular buffer - remove oldest entries if over limit
  // kj::Vector doesn't have remove(index), so use swap-and-pop approach
  while (history_lock->size() > config_.history_size) {
    // Shift all elements left by one position (remove first element)
    for (size_t i = 0; i < history_lock->size() - 1; ++i) {
      (*history_lock)[i] = kj::mv((*history_lock)[i + 1]);
    }
    history_lock->removeLast();
  }
}

uint64_t EventBroadcaster::current_id() const {
  return next_id_.load(std::memory_order_acquire) - 1;
}

kj::Vector<SseEvent> EventBroadcaster::get_history(uint64_t last_id) const {
  kj::Vector<SseEvent> result;

  auto history_lock = history_.lockShared();

  for (const auto& entry : *history_lock) {
    if (entry.id > last_id) {
      result.add(SseEvent(entry.event.id, entry.event.type, entry.event.timestamp_ns,
                          kj::str(entry.event.data)));
    }
  }

  return result;
}

size_t EventBroadcaster::subscription_count() const {
  auto subs_lock = subscriptions_.lockShared();
  return subs_lock->size();
}

EventBroadcaster::Stats EventBroadcaster::get_stats() const {
  Stats stats;
  stats.events_broadcast = stats_.events_broadcast.load(std::memory_order_relaxed);
  stats.active_subscriptions = subscription_count();
  stats.total_subscriptions = stats_.total_subscriptions.load(std::memory_order_relaxed);

  auto history_lock = history_.lockShared();
  stats.events_in_history = history_lock->size();
  stats.queue_size = 0; // Not using a queue currently

  return stats;
}

} // namespace veloz::gateway::bridge
