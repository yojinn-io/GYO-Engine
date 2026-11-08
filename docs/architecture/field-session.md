# Field sessions: the common data every real-machine session collects

Owner: Architecture (this page); each product owns the items it adds. Record: [big-map HANDOFF](plans/big-map/HANDOFF.md)
(decision `v8-arch-field-session-common`, 2026-11-08). First use: the `object_fps_pvp` lag-l2 + K8 session of 2026-11-09
([session manual](../object_fps_pvp/plans/v8/v8-product-hitbox-lag-l2-session.md)).

## Why a common list

A session with real players on real machines is the most realistic environment the project has, and it is rare (people,
machines, a place, an evening). The user's rule (2026-11-08): every such session **collects data**, not only the one case
its batch is about, and the collection is a collaboration: architecture writes the items every session needs whatever the
product, the product adds the items its batch cares about, the two lists are declared together before the session, and
the data is read by both afterwards. Handing the session to one line and forgetting it is not the process.

What the common items buy: numbers that architecture decisions wait for (tick cost, load time, memory, frame time, wire
sizes at real density) arrive from every session as a by-product, with the same names, so sessions can be compared.

## The common items (every session, every product)

Collected per machine and per process; one line per second for the periodic ones (per tick or per frame is too much
for an hour), in the product's JSONL or log-line families ([log contract](../object_fps_pvp/log-contract.zh-Hant.md) for
`object_fps_pvp`), ending with the family's `trace_end` line so dropped lines are counted.

| # | Item | What exactly | Why architecture needs it |
|---|---|---|---|
| C1 | Session facts (`marks.txt` or equivalent) | commit and `build_metadata.json` of every binary, SHA-256 of the binaries, machine (OS, CPU, GPU, RAM), role (server, gateway, client N), addresses, participants, content ids (map, arena/scene), the configuration in force (artificial delay, diagnostics flags), start and end marks with wall clock | nothing below can be compared across sessions without it |
| C2 | Clocks | each process writes its steady-clock origin against wall clock at start and once a minute | aligns machines; every cross-machine measurement depends on it |
| C3 | Server tick | per second: tick duration p50, p95, max; ticks that overran; players present | the fixed-tick budget (`architecture.md`, fixed simulation time) is the first thing a bigger map or more entities break |
| C4 | Wire per peer | per second: bytes in and out, datagrams sent, received, dropped, out of order or late; snapshot size p50, p95, max; RTT and jitter where measured | per-recipient budget (1200 B), interest decisions, and the counts reconciliation (sent, received, dropped) that §14 asks for |
| C5 | Client frame | per second: frame time p50, p95, max; draw count and triangles when the renderer reports them; prediction corrections (count, largest) | culling, LOD and region-mesh decisions; the user's declared p95 |
| C6 | Load and memory | content load time to ready; peak resident memory at ready and at exit; file sizes loaded | load-time and binary-cache replacement criteria, Match memory at scale |
| C7 | Faults | assertion text and exit code of any process that ends abnormally; Trace `dropped` counts; disconnects and reconnects with the reason the program gives | a session that crashed is still data, if the fault is recorded |
| C8 | The kit | one archive per machine: binaries from one commit, configuration, the scripts, nothing fetched at the venue; every script run once before delivery; the end-of-session step packs the logs into one folder per machine for the user to send back | the user runs it and sends logs; no manual collection |

Items a program cannot yet record are listed in the declaration as gaps, not silently left out; a gap recorded once is a
candidate for the product's or Engine's next batch (Trace and Log are Engine's, record kinds are the product's).

## The product's items

The product's declaration adds its own items under its own ids, next to the common list: the judgments its batch declared
(for `object_fps_pvp` on 2026-11-09: lag-l2's rewind range R, K8, the movement diagnostics of pm-detect), with their
thresholds declared before the session (AGENTS.md §14.1). Everything else recorded is exploratory and is marked so.

## The process

1. Before a session the product session writes the combined list (common + its own) into its declaration and the kit
   manual; architecture checks the common half is present and the names match this page.
2. The user approves the declaration and receives the kit with a one-page contents list.
3. After the session the user sends the log folders back. The product analyses its items; architecture reads the common
   items and records what they change (a replacement criterion met, a budget exceeded, nothing) in the relevant HANDOFF.
4. Evidence stays under the main checkout's `build/evidence/<batch>/session-<date>/` with the SHA-256 list; the documents
   record the numbers and where the folder is.

## Design Invariants

A change against any item below is an Architecture Delta that needs the user's agreement; so is changing this section.

1. **Common first, product second.** Every field session collects C1–C8 (or records which of them it cannot yet collect)
   before any product item is added. Guard: review of the session declaration against this page.
2. **Recorded, not interpreted, at the venue.** The kit records; judgments are made afterwards from the files, by the
   declared rules. Guard: the kit scripts contain no judgment; review.

### Replacement Criteria

1. A product needs a common item in a different shape (another period, another statistic) and the shape cannot serve both:
   split the item into product-owned variants and keep the comparable core here.
2. Two products collect the same product item: it moves to this list.
