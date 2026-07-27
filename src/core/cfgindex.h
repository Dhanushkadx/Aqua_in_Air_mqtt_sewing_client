#ifndef _CFGINDEX_H
#define _CFGINDEX_H

#include <Arduino.h>

// ── CfgIndex file ──────────────────────────────────────────────────────────────
// Tracks which version of each server-pushed config block this device holds.
// PrimeHive versioned 64 schedule blocks plus four irrigation tables; the sewing
// client has no schedule concept, so the slot space is just the named blocks
// below. Add a slot here AND its key in kSlotKeys[] in cfgindex.cpp to extend it.
#define CFGINDEX_FILE       "/cfgindex.bin"

enum {
    CFGSLOT_MACHINE_CONFIG = 0,  // device<->machine binding + tenant context
    CFGSLOT_ACTIVE_MANIFEST,     // the active production job (PrimeFlow manifest)
    CFGINDEX_SLOTS               // total version-store slots
};

// Alias kept so the fetch state machine in wifi_mqtt.cpp — which sizes its
// stale-key array from this name — needs no per-product edit.
#define CFGINDEX_MAX_BLOCKS CFGINDEX_SLOTS

// ── Block-apply callback ────────────────────────────────────────────────────
// The core versions and fetches blocks but is deliberately domain-agnostic — it
// does not know what a "manifest" is. When a fetched block arrives, cfgindex_save
// calls this handler (if registered) with the block key and the raw attr/res
// JSON, and the product decides what to do with it. Keeping this a callback (vs
// hardcoding a dispatch) is what lets cfgindex.cpp live unchanged in the shared
// core across products.
//
// The handler runs on the MQTT task (Task1). It must be FAST and non-blocking —
// hand the payload to the owning task, do not parse or apply inline.
typedef void (*cfgindex_apply_fn)(const char* key, const char* json);
void cfgindex_set_apply_handler(cfgindex_apply_fn fn);

// ── Public API ─────────────────────────────────────────────────────────────────

// Initialise LittleFS, load stored cfgIndex versions, and create the mutex.
// Must be called once in setup() before any other cfgindex_* call.
void config_init();

// Process an incoming cfgIndex payload from the server.
// Compares each block's server version against the locally stored version.
// Stale block keys (server newer than local) are written into stale_keys[][16]
// and marked "pending" internally so cfgindex_save() accepts them on arrival.
// Returns the number of stale blocks found.
// Expected JSON: {"cfgIndex":{"machine":{"ver":3,"updatedTs":...},...}}
int cfgindex_process(const char* json, char stale_keys[][16], uint8_t max_keys);

// Returns true if this block key was marked pending by cfgindex_process().
bool cfgindex_is_pending(const char* block_key);

// Write the key of the first pending block into key_out (must be >= 16 bytes).
// Returns true if a pending block was found, false if all blocks are up to date.
// Used by the fetch state machine to know what to request next.
bool cfgindex_next_pending(char* key_out);

// Clear the pending flag for a block without saving it — used when a fetch times
// out and we give up on that block so the state machine can move on.
void cfgindex_clear_pending(const char* block_key);

// Build a JSON string of all locally stored block versions for publishing to
// attr/pub after a sync completes, so the backend knows the device is current.
// Format: {"machine":{"ver":4},"thresholds":{"ver":2}}
// buf must be at least 1024 bytes.
void cfgindex_get_local_json(char* buf, size_t len);

// Apply a fetched cfgIndex payload for `key` and advance its local version.
// Currently a logged no-op apply — version sync is live but no slot has a
// consumer wired yet. Returns true when the version was advanced.
bool cfgindex_save(const char* key, const char* json);

// Zero the stored local version(s) so the next sync re-fetches from the server.
// key = a specific slot key zeroes just that slot; key = nullptr or "all" zeroes
// every slot. Persists the change. Version-only — does NOT touch config data.
// Returns the number of slots affected, or -1 on error / unknown key. The caller
// (reset_cfgindex RPC) re-arms the fetch state machine afterwards.
int cfgindex_reset(const char* key);

// Factory reset: wipe the persisted cfgIndex so the next boot re-syncs the whole
// config from the server. PRESERVES /wifi.json (credentials + portal flag) so
// the device stays reachable. Returns the number of files removed. The
// factory_reset RPC calls this and then reboots.
int config_factory_reset();

#endif // _CFGINDEX_H
