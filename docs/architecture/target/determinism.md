# Determinism under the kernel

> Status: target, not implemented

Updated 2026-11-11. Owner: architecture (architect session). Source: `docs/architecture/plans/kernel/spec.zh-Hant.md` §G (the four contracts of a (c) zone) and §G.1 (zone rules); the levels and zones themselves are defined by the current contract `docs/architecture/determinism.md` (§1, §2), which this page does not change. English because the file it will merge into is English.
This page records only what the user has decided; spec items still marked "待使用者" are not here.

## 1. Zones are declared contracts, checked at startup

Every System, component, resource and event declares `zone ∈ {C, B}`; there is no default (spec S-6). The Kernel does not decide zones, it checks the declarations against each other (rules Z1 to Z8, spec §G.1): a zone=C System's whole access set (reads, writes, membership filters, events, deferred writes, run conditions, set reads) lies in zone=C data; zone=C data is written only by zone=C Systems or hooks, with the single exception of ingress Systems writing `role = Input` data in the ingress stage; zone=C Systems have no effects except the write-only `Diagnostics` sink (`DiagnosticsWriter<T>`, spec S-8 v0.3: one-way (c) to (b), not World data, no conflict edges), no `Local`, no change filters, no `ExclusiveWorld`; `role = Input` data is `rollback = No`. A violation is a startup error, not a runtime check.

## 2. Where (c) code may take time from (user decision 2026-11-11, spec K.3 item 11)

A zone=C System may read time from exactly two sources:

1. **The World's domain tick** (`tick[domain]`, advanced only by the first step of that domain's clock schedule; inside the rollback snapshot).
2. **Recorded boundary inputs**: zone=C, `role = Input` resources and columns written by the Host while the World is `Ready` or by an ingress System in the ingress stage, and therefore replayable from a record. Today's shapes: `HostClockSample` (the Host samples the clock once per wake-up and writes it), and the arrival time carried with every inbound message (`Link.inbound = {arrivalNs, bytes}`, written into `InputRing[tick]` by ingress).

A zone=C System reading a clock itself stays forbidden: wall-clock resources such as `FrameTime` are `external`, zone=B, and rule Z1 keeps (c) away from them; `<chrono>` in a (c) source file is caught by the source-file scan (spec J-17). Writing the domain tick or a boundary resource from any System other than ingress is a startup error (spec J-39).

This resolves the conflict between the concept document (`mechanism.zh-Hant.md` §3.3, formerly "time comes only from the tick") and the current contract's DI 4 ("the tick, or a clock injected"): the mechanism text was amended; DI 4 stands. The host-judgement zone (current contract §2) needs arrival timing and stays (c); its inputs are now data in the ring rather than a clock callback into the tick.

## 3. Randomness

Random state is a per-entity zone=C component with `rollback = Yes` (entity-model E-10's `Brain.rngState`), seeded by the product and included in the digest. The Kernel provides no random source; a single shared resource was rejected because replaying one entity's order change would disturb every other entity's sequence (spec appendix X20).

## 4. The four contracts of a (c) zone (spec §G.2)

Serialised execution is necessary, not sufficient (PR #177: the predicted core is serialised today and still shows 1 to 4 ULP differences). A (c) zone rests on one contract in each of four places:

| Place | Contract | Guards |
|---|---|---|
| World | iteration ascending by key; references invalid after a sync point; structural changes only through the deferred queue during a run; queue applied in the fixed order; allocator monotonic and serialised by conflict analysis; no hash or address order | spec J-2, J-3, J-4, J-8, J-9, J-31, J-32 |
| Executor | one Plan per schedule, (c) ambiguity is a startup error; Serial by default, Parallel only with a bit-identical guard; time from §2 above; randomness from §3 | spec J-20, J-22, J-27, J-39 |
| Ingress | external messages enter only through `Link`; only ingress Systems write `InputRing[tick]`; arrival time is data; (c) Systems never touch `Link`; in replay the ring is read-only | spec J-40, J-41, J-42 |
| Math / build | current contract DI 3 (no contraction, no FMA or ISA intrinsics, no fast-math, exact libm only, Math's own transcendental functions after E1); the Kernel cannot check this, it only emits the list of (c) source files for the scan | current contract E0 to E5; spec J-17 |

## Design Invariants (candidates; become binding once approved)

1. **Zones are declared, never defaulted, and checked at startup** (rules Z1 to Z8). Guards: spec J-14, J-38 to J-41.
2. **(c) time comes only from the domain tick and recorded boundary inputs**; no (c) System reads a clock; no System other than ingress writes the tick or a boundary resource. Guards: spec J-39, J-17.
3. **(c) randomness is per-entity rollback state seeded by the product**; the Kernel has no random source. Guards: spec J-9 scan (`<random>`), product digest.

### Replacement Criteria (candidates)

- LIM-2: v1 is Serial only; see `limits.zh-Hant.md`.
- If a (c) rule can be stated only with time that is neither the tick nor a recorded input, the input model is wrong: add the recorded input, do not open a clock read.
