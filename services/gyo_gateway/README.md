# GYO Gateway infrastructure

This Go module supplies framed TCP connections with caller-selected deadlines
and error returns, UDP framing, authenticated peer sessions and HTTP
server defaults. It has no game imports, room allocation, world state, gameplay
rules or game protocol messages. Its tests use synthetic byte payloads and peers.

Each session limits its peer's packet rate with a token bucket (120 packets per
second, 120 deep, kept as GCRA on integer time in `session/session.go`). It
measures the peer's pace rather than the reader's, so datagrams that queued in
the socket while the reader stalled are not rejected for arriving together.

A product supplies its own executable and routes in a Go module of its own, which
depends on this one through an explicit local Go module replacement. There is
deliberately no executable here that imports or enumerates products.

Run `go test ./...` from this directory. Adding or removing a product does not
require editing this module or its tests.
