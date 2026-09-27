# Object FPS PvP contracts

`client_v4.proto` describes remote Client ↔ Gateway payloads.
`runtime_v4.proto` describes local ObjectFPS Adapter ↔ C++ IPC Host payloads.
They are separate versioned contracts and contain no shared imported game schema.
The Adapter maps between their generated types; Match receives product C++ values.

## Protocol v4

The independent Client and Runtime schemas are now v4. All three roles upgrade
as a unit and reject v1–v3. Product rules, identity, time and ownership are defined
in [the v4 contract](../../../docs/object_fps_pvp/protocol-v4.zh-Hant.md).
Batch 03 provides headless action delivery; batch 04 adds GUI firing and Mark23
feedback with short real-presentation regression evidence. Batch 05 provides integrated
movement/fire/HP probes and a retained manual guide. On 2026-09-28, v4 was promoted
to a stable baseline for same-host two-player Linux/X11/Vulkan operation: three
120-second GUI rounds, 60/144 Hz 30-minute headless runs, native window/input
checks and the unchanged product fault matrix passed. Physical LAN, Windows and
lag-compensated shooting are outside this qualification. See the
[baseline and evidence](../../../docs/object_fps_pvp/plans/STABLE_BASELINE.md).

## Wire and preserved movement behavior

UDP uses the shared 24-byte big-endian transport header: `GYOP` magic (4),
client protocol version (2), opaque message type (2), session ID (8), packet
sequence (4), payload byte count (2), channel (2). Version 4 requires channel 0.
Maximum datagram length including the header is 1200 bytes. Product message kinds
are HELLO=1, WELCOME=2, INPUT=3, SNAPSHOT=4, ERROR=5, ACTIONS=6, ACTION_RESULTS=7. There are no transport ACK fields or
reliable channels; snapshots carry per-player command resolution cursors.
Client, Gateway and Match must upgrade together; versions 1, 2 and 3 are rejected.
Hello authenticates the lobby-issued token before the peer endpoint is bound.
Welcome confirms the runtime accepted the player and carries the Match-owned CombatRules; it contains no world snapshot.

TCP IPC frames are a 4-byte big-endian payload length followed by one serialized
RuntimeEnvelope. Length must be 1–65536 bytes. Runtime sends Ready first. Ready
advertises the loaded arena ID/version, 60 authority ticks/second, a snapshot every
completed tick, capacity 2, and Match CombatRules. The envelope version is independent of the UDP
header's client version. Join/Leave/Input/ActionBatch travel to Runtime; JoinResult/WorldSnapshot/ActionResults/RuntimeError follow Ready in the reverse direction.
An unstarted snapshot frame may be replaced by a newer snapshot. A partially
written TCP frame must finish before another frame is selected.

Angles are radians. Input axes are in [-1, 1], yaw magnitude is at most 1e6,
pitch is in [-pi/2, pi/2] (float32 bounds), and every float must be finite.
Each immutable MovementCommand has a positive per-player sequence and represents
exactly 1/60 second. No client time, duration, position or outcome is accepted.
PlayerInput carries a positive `movement_epoch` and a strictly ordered window of
at most 12 unacknowledged commands. The network worker sends the first available
window immediately, then resends the current full window at 60 Hz independently
of rendering. When ACKs clear the window completely, the next nonempty window
starts immediately; a partial ACK does not restart its deadline. It prunes commands as authority ACKs arrive and never sends a
catch-up burst after a worker stall. The 1200-byte limit includes the UDP header.

A new player starts in epoch 1. Only authority advances `movement_epoch`.
Old-epoch packets are inert, and input cannot introduce a future epoch.
An authoritative epoch advance clears old commands, resets its resolution cursor,
and requires a fresh sequence beginning at 1. Client prediction seeds two neutral
commands before current controls; the epoch prevents a delayed old command window
from being mistaken for the new sequence. Unresolved commands are immutable
within their epoch; duplicates never refresh gameplay input lifetime.

After command 1 arrives, authority resolves exactly one sequence each tick.
Missing sequences use the last actually applied input for at most 15 missing
ticks, then neutral axes. Late resolved commands never execute. At most 32 future
commands are buffered per player. Snapshot is the full active player set: position,
`movement_epoch`, and `last_resolved_command` describe the same post-step state,
including substituted steps. `contiguous_pending_commands` counts consecutive
queued commands immediately after the resolution cursor, with range 0–32.
Authority tick remains independent of a player's epoch and sequence.

Match can rotate the movement epoch at the next authority boundary, retaining
pose and identity, with a 60-tick reset cooldown. The 30-step post-execution
contiguous-queue sum triggers backlog recovery at 105. A sum of zero, at least
one substituted step in that same window, and an entirely empty future-command
map identify exhausted lead after interruption and trigger recovery too. An
all-Actual zero-depth window does not reset. Awaiting command 1 never loops
through resets. ACK loss, window saturation or an advanced cursor alone is not
a reset condition. The reason is diagnostic-only, not client-controlled.

The client worker retains the latest state plus up to 64 snapshots stamped when
UDP is actually received. `Drain()` atomically consumes that history together with
its matching state and lifecycle generation. Overflow discards the oldest sample
and reports both a since-drain flag and a generation-scoped cumulative count.
Prediction/presentation consumes these samples without inventing receive times.

The worker independently repeats authenticated Hello once per second after joining;
active Welcome replies maintain transport liveness without submitting gameplay
commands or refreshing the authority's missing-input hold counter. Departure
requires a successful HTTP response containing `left: true`; unconfirmed cleanup
retains credentials, stops UDP, and must complete before another session starts.

Generated Go files are checked in under `clientv4/` and `runtimev4/`. Regenerate
both schemas with the same pinned protoc provided by the C++ build and
`protoc-gen-go` from the product's pinned `google.golang.org/protobuf` module.
Use `--proto_path=protocol --go_out=. --go_opt=module=gyo.local/object_fps_pvp`
from the product module root. C++ generated files belong to the build tree.
Do not edit generated bindings or add application rules to either codec.

The first publishable window of a new movement epoch is formed after its first
legitimate fixed input step: retain the two neutral seed commands and publish
them with the available current commands. An ACK-zero neutral-only seed is not
published on an elapsed-zero frame. This explicit bootstrap rule preserves the
two-command headroom without generating extra steps; subsequent windows and
running-cursor reseeds keep the existing publication rules.

While that neutral-only bootstrap is still unpublished, an Advance that would
produce three or more accumulated fixed steps admits at most one step of elapsed
time, retaining the fractional remainder and reporting the omitted time. Normal
30 FPS two-step production remains intact. This bounds startup loading debt; it
does not change the running command cursor or add another neutral lead command.

## Action delivery

ActionBatch contains at most eight immutable shot requests plus acknowledged_through;
an empty request list is a pure acknowledgement. Runtime carries the player ID
mapped from the authenticated Gateway Session; remote Client does not choose it.
ActionResults contains at most eight immutable terminal decisions and retired_through.
Requests/results retain their ActionId across new packet sequence numbers. Each
player has a 32-ID window above the authoritative retirement floor. Conflicting
content rejects the whole batch, including its ACK. Retired IDs never execute again.

Client requests and ACK-only retries share one at-most-30-Hz deadline. Match IPC
and Gateway result lanes rotate through the complete unacknowledged window in
batches of eight, without using the lossy control or latest-snapshot queue as the
only decision store. A partial TCP frame always finishes before another begins.
Late deadlines skip elapsed slots. The public 120 authenticated packets/second cap
still counts movement, action, Hello and malformed/repeated traffic together.

Client keeps received decisions through a game-thread stall. Drain transfers each
decision once; only contiguous transferred decisions become ACK-eligible. Entries
are reclaimed only after authoritative retirement, keeping pending and unconsumed
state bounded. Snapshot history overflow or movementEpoch changes never erase
this ledger. Leave/disconnect/new Session clears it with the lifecycle generation.

WorldSnapshot carries separate movement and combat collections. CombatState is
latest authority HP and cooldown, not an interpolated or replayed movement field.
CombatRules come from Match Ready through Welcome; Gateway and Client validate
and transport them rather than maintaining another set of weapon defaults.

Owner-local short fault verification is `build/acceptance/object_fps_pvp/action_probe.py`:
it runs real Match/Gateway and the action probe, preserving per-case evidence and
nonzero failure exit status. It pauses packet paths and TCP partial-frame delivery;
it does not claim to saturate a kernel socket buffer. Full commands/results are in
the batch 03 dev_log and handoff.
