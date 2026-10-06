# Object FPS PvP contracts

Production Client, Gateway and Runtime use protocol version 6 (pv6) and reject
every other version. The product-owned [pv6 contract](../../../docs/object_fps_pvp/protocol-v6.zh-Hant.md)
states what pv6 changes; every rule it does not restate stays as written in the
[pv5 contract](../../../docs/object_fps_pvp/protocol-v5.zh-Hant.md). Together they are
the single source for gameplay, field validation, identity, timing and delivery rules.
The [v6 plan](../../../docs/object_fps_pvp/plans/v6/README.md) tracks implementation and
acceptance. The version value has one definition per owner and language: C++
`wire::ProtocolVersion`, Go `adapter.ProtocolVersion`, and the acceptance expected
values; `object_fps_pvp.protocol_version` checks that they agree.

`client_v6.proto` defines remote Client ↔ Gateway payloads.
`runtime_v6.proto` defines local ObjectFPS Adapter ↔ C++ IPC Host payloads.
These independent schemas import no shared gameplay schema. The product Adapter
maps generated types; Match receives product C++ values. No shared Engine or
Gateway codec owns game rules.

UDP retains the shared 24-byte framing header and 1200-byte total packet limit.
Runtime IPC retains length-prefixed TCP frames. Ready supplies Match-owned combat
and movement rules; Gateway forwards them in Welcome. Snapshot movement and
combat records carry matching life generations. Movement windows are discarded
at an authoritative life/epoch transition, while the immutable action ledger and
ACK/retirement cursors survive until Leave/disconnect. Nonzero stale/future-life
actions reach Match for a terminal decision.
`PlayerState.movement_slack_sequence` and `movement_slack_us` are an optional Host
timing observation, present together or not at all: the smallest movement slack since
the previous published snapshot (executing tick minus a command's first receipt, or
negative for a command that arrived after its sequence was substituted) and its
sequence. Adapters keep their presence, reject |slack| above 1,000,000 us and a
sequence outside 1..last_resolved_command; only the Client's phase tracking reads
them (field 16, the former `epoch_start_wait_us`, is reserved).
`CombatState.last_damage_tick`, `damage_count` and `last_attacker_id` are the
presentation-only hit record of the current life: all zero before the first hit, written
by the Match when a legal Shot hits the player, kept while dead and reset on respawn.
Adapters require all zero or all set, a tick inside the current life state and the
snapshot, an attacker other than the player, a count no larger than maximum_hp, and a
dead player whose last hit is its death tick. Nothing in the authority reads them.
`Ready.arena_digest` and `Welcome.arena_digest` (fixed64) carry the Match's arena content
digest: FNV-1a 64 over the parsed arena in a canonical byte order, zero meaning missing.
The Gateway rejects a zero digest at readiness and forwards it verbatim into Welcome,
the HTTP join reply and the room list; it never computes or compares it. The Client
compares it on both join paths and fails with `arena_identity_mismatch` or
`arena_content_mismatch`.
`PlayerState.connection_quality_failures` counts the failed 10-second connection-quality
windows in a row (0-2); `PlayerInput.observed_authority_tick` is the latest snapshot
tick the Client applied, and a merged window carries the largest. The Match evicts
after three failed windows and sends the runtime-only `PlayerEvicted`; the Gateway
clears the player like a Leave and sends the Client an `Error` with code
`evicted_high_latency` or `evicted_unstable_input`.

The historical `ShotRequest`, `ShotDecision`, `ShotRejection` and `ActionBatch.shots`
names now cover both Shot and Reload. `ActionKind` and `LifeState` have an invalid
zero sentinel; adapters reject unknown values. Shot aim fields use proto3 optional
presence: both are required for Shot and both absent for Reload. C++ domain enums
are explicitly mapped, since their numeric values differ from the wire enums.

Generated Go bindings are checked in under `clientv6/` and `runtimev6/`. Regenerate
both with the pinned protoc from the C++ build and `protoc-gen-go` from the product's
pinned `google.golang.org/protobuf` module, from the product module root:

```sh
protoc --proto_path=protocol --go_out=. --go_opt=module=gyo.local/object_fps_pvp \
  protocol/client_v6.proto protocol/runtime_v6.proto
```

C++ generated files belong to the build tree. Do not hand-edit generated bindings
or add application rules to either codec. Product Go tests cover field validation,
maximum datagrams, version rejection, lifecycle isolation and action delivery.
