# GYO Gateway infrastructure

This Go module supplies framed TCP connections with caller-selected deadlines
and error returns, UDP framing, authenticated peer sessions and HTTP
server defaults. It has no game imports, room allocation, world state, gameplay
rules or game protocol messages. Its tests use synthetic byte payloads and peers.

A product supplies its own executable and routes in a Go module of its own, which
depends on this one through an explicit local Go module replacement. There is
deliberately no executable here that imports or enumerates products.

Run `go test ./...` from this directory. Adding or removing a product does not
require editing this module or its tests.
