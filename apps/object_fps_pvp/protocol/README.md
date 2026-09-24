# Object FPS PvP contracts

`client_v1.proto` describes remote Client ↔ Gateway payloads.
`runtime_v1.proto` describes local ObjectFPS Adapter ↔ C++ IPC Host payloads.
They are separate versioned contracts and contain no shared imported game schema.
The Adapter maps between their generated types; Match receives product C++ values.

UDP uses the shared 24-byte big-endian transport header: `GYOP` magic (4),
client protocol version (2), opaque message type (2), session ID (8), packet
sequence (4), payload byte count (2), channel (2). Version 1 requires channel 0.
Maximum datagram length including the header is 1200 bytes. Product message kinds
are HELLO=1, WELCOME=2, INPUT=3, SNAPSHOT=4, ERROR=5. There are no ACK fields or
reliable channels in this MVP. Hello authenticates the lobby-issued token before
the peer endpoint is bound. Welcome confirms the runtime accepted the player;
it contains no world snapshot.

TCP IPC frames are a 4-byte big-endian payload length followed by one serialized
RuntimeEnvelope. Length must be 1–65536 bytes. Runtime sends Ready first. Ready
advertises the loaded arena ID/version, 60 authority ticks/second, a snapshot every
3 completed ticks, and capacity 2. The envelope version is independent of the UDP
header's client version. Only Join/Leave/Input travel to Runtime; only
JoinResult/WorldSnapshot/RuntimeError follow Ready in the reverse direction.

Angles are radians. Continuous input axes are in [-1, 1], pitch is in [-pi/2,
pi/2], and every float must be finite. InputSequence is positive and strictly
increasing for a player; ClientTick is an independent unsynchronized sampling
counter. Neither one is an authoritative simulation clock. Input contains no
position or outcome claims. Snapshot is the full active player set, with owning
absolute transforms and the last input sequence accepted by the authority.

Generated Go files are checked in under `clientv1/` and `runtimev1/`. Regenerate
both schemas with the same pinned protoc provided by the C++ build and
`protoc-gen-go` from the product's pinned `google.golang.org/protobuf` module.
Use `--proto_path=protocol --go_out=. --go_opt=module=gyo.local/object_fps_pvp`
from the product module root. C++ generated files belong to the build tree.
Do not edit generated bindings or add application rules to either codec.
