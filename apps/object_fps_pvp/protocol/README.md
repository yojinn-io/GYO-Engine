# Object FPS PvP contracts

Production Client, Gateway and Runtime use protocol v5 and reject v1–v4.
The product-owned [v5 contract](../../../docs/object_fps_pvp/protocol-v5.zh-Hant.md)
is the single source for gameplay, field validation, identity, timing and delivery
rules. The [batch plan](../../../docs/object_fps_pvp/plans/v5/README.md) tracks
implementation and acceptance; the prior [v4 baseline](../../../docs/object_fps_pvp/plans/v4/STABLE_BASELINE.md)
does not certify v5.

`client_v5.proto` defines remote Client ↔ Gateway payloads.
`runtime_v5.proto` defines local ObjectFPS Adapter ↔ C++ IPC Host payloads.
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
`PlayerState.epoch_start_wait_us` is an optional Host timing observation: the wait
from receiving an epoch's first window to the tick that executed sequence 1. Adapters
keep its presence and reject values above 1,000,000; only the Client's start-phase
alignment reads it.

The historical `ShotRequest`, `ShotDecision`, `ShotRejection` and `ActionBatch.shots`
names now cover both Shot and Reload. `ActionKind` and `LifeState` have an invalid
zero sentinel; adapters reject unknown values. Shot aim fields use proto3 optional
presence: both are required for Shot and both absent for Reload. C++ domain enums
are explicitly mapped, since their numeric values differ from the wire enums.

Generated Go bindings are checked in under `clientv5/` and `runtimev5/`. Regenerate
both with the pinned protoc from the C++ build and `protoc-gen-go` from the product's
pinned `google.golang.org/protobuf` module, from the product module root:

```sh
protoc --proto_path=protocol --go_out=. --go_opt=module=gyo.local/object_fps_pvp \
  protocol/client_v5.proto protocol/runtime_v5.proto
```

C++ generated files belong to the build tree. Do not hand-edit generated bindings
or add application rules to either codec. Product Go tests cover field validation,
maximum datagrams, version rejection, lifecycle isolation and action delivery.
