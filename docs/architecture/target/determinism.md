# Determinism under the kernel

> Status: target, not implemented

Updated 2026-11-13 (`v8-arch-a6-revision`). Owner: architecture (architect session). English because the file it will replace is English.

This page now carries two things:

- **Sections 1, 2 and 4: the levels, the boundary with its zone table, and the module-by-module table**, copied from the legacy contract `docs/architecture/determinism.md` (its sections 1, 2 and 4, including 4.7a and 4.7b as fixed by `v8-arch-determinism-table-fix`). They keep their legacy section numbers, so "determinism section 2" or "section 4.9" means the same text in both files; from this batch on the authority for these three sections is this page, and the legacy copy is frozen. The text is copied as it stood: it still says "today" about the running code and still names the plan items E0 to E5 and the decisions DET-0 to DET-3 where the tables did.
- **Sections 10 to 13: what the kernel adds** (zones as declared contracts, where (c) code may take time from, randomness, the four contracts). Source: `docs/architecture/plans/kernel/spec.zh-Hant.md` section G. These were sections 1 to 4 of this page before 2026-11-13; only the numbers changed.

**Not on this page, by decision**: legacy sections 0 (DET-0 to DET-3), 3, 5, 6, 7 (the gap list and E0 to E5 as plan items), 8 and 9, and the candidate section 4.2a. They belong to the determinism line, which is paused together with DET-0 to DET-3 (`v8-arch-kernel-pending-paused`); they stay in the legacy document, and where they go is decided before the closeout batch deletes it. A reference in the copied text to a section that is not on this page (0, 3, 5, 6, 7, 9) points at the legacy document. The legacy Design Invariants 1 to 6 remain the current contract (they have guards) and stay in the legacy document until then; the invariants at the end of this page are the kernel's additions.

This page records only what the user has decided.

## 1. Three levels

| Level | Promise | Who needs it in GYO |
|---|---|---|
| **(a) same process** | Given the same inputs, the same results within one process: no clock read, random source, thread order, pointer order, hash order or history dependence. | Everything that is judged or replayed. Every module's Design Invariants already demand it (entity-store 8, voxel-mesh 7, collision 1; audio 8 given each voice's start and stop frames, time entering only through DI 3's time base). |
| **(b) same binary, same platform** | A given binary produces the same bits on every run on that platform, given the same inputs. (a) plus: no uninitialized reads, no dependence on addresses, environment or load order. | Incident replay on the same platform; the same-host two-tree digest comparison (`compare_authority_trees.py`); "Match-consistent only" (entity-requirements 4-6). |
| **(c) cross-platform, bit for bit** | Every supported platform produces the same bits. (b) plus: no contraction, no FMA intrinsics, no platform libm except the exact functions, no `long double`, no x87, integer or single-rounding arithmetic where stated. | One golden file for every CI platform; a Windows Client predicting a Linux Match compared without tolerance; a Linux Match incident replayed on a developer's Mac or Windows; the content-identity digests Client and Match compare at join (section 4.9); the zero-angle authority golden today. |

**Platforms.** The L1 platforms are linux-x64, windows-x64 and macos-arm64 (`build/acceptance/common/package_contract.py:18`, `RELEASE_PLATFORMS`). macos-x64 (the user's Intel Mac, the Match of the 2026-11-09 session) is a local build target with no L1 row since 2026-10-31 (commit `5ea8fcc0`; `cross-platform.yml:287`). "Cross-platform guard" below means those three CI platforms; macos-x64 is covered only by local runs and field sessions. Measurements written before 2026-10-31 (B3) speak of four platforms.

Levels are nested. A promise is only as strong as its guard: a module "designed for (c)" whose goldens are checked on one platform is guarded at (b).

What no level covers: GPU output (driver and hardware dependent; `render.*` tests compare on Lavapipe only), wall-clock scheduling (which tick a packet lands in, which block an audio request lands in), and timing diagnostics (`movementSlack*`, excluded from the digest by its field list).

## 2. The determinism boundary

The boundary is a set of code paths, not a set of directories. A path is **inside** when any of these holds:

1. its result is encoded by an authority digest (`tests/object_fps_pvp/AuthorityDigest.cpp`, field list `DigestFields`), or by a future one;
2. a Client predicts it and the prediction is compared with the authority (today: movement, `StepMovement` shared by `PvpMatch` and `LocalPlayerPrediction`; the shot gate, `LocalShotGate`, integer tick arithmetic, implicitly compared through the Match's acceptance);
3. a tool and the runtime must agree on it bit for bit through a data contract (voxel map expansion, map markers);
4. Client and Match must agree on it bit for bit to join: the **content-identity digests** (`AnimationContentDigest`, `ArenaContentDigest`, section 4.9).

Everything inside is held to one level, chosen by zone:

| Zone | Definition | Level | Today |
|---|---|---|---|
| **Predicted core** | Inside by rule 2, and whatever the digest encodes of the Client's prediction and presentation | **(c)** (conditional on E0) | Movement (`Movement.cpp`, `PlanarMovement.cpp`, Collision sweeps, `StaticAabbGrid`), its arena data (VoxelMesh expansion), entity order, the digest encoding itself; the prediction record of the digest, which includes the Client's presentation values `renderPosition`, `correctionOffset`, `interpolationAlpha` and the phase corrections (`DigestFields`, `AuthorityDigest.cpp:63-66`; `InterpolateLocalPresentation` sweeps with `MoveCharacterBody`, `LocalPlayerPrediction.cpp:461-482`), and the seven `prediction-*` goldens. Non-zero yaw calls `std::sin`/`std::cos` once per step (`PlanarMovement.cpp:17-18`), so the (c) guard covers only zero-angle scenarios (section 4.1). |
| **Host judgement** | Inside by rule 1: the host's quality and eviction decisions (`quality`, `eviction` records; `host-poor-connection` is golden) | **(c)** | Integer and time-point arithmetic over an **injected** clock (`MatchRuntimeHost::Now()` returns `now_()`, `MatchRuntimeHost.cpp:603`); the digest runner injects a scripted clock, so time is an input here, not a read. |
| **Middle zone** | Inside by rule 1 only: judged, not predicted | **DET-1** | Shot direction (`ShotQuery.cpp:82-84`, `sin`/`cos` of yaw and pitch), hit against a sampled pose (`PlayerPose.cpp:510-561` → `Model::SamplePose` → `Math::Slerp`, `acos`/`sin`, `Quaternion.hpp:93-96`; bone placement through `ComposeEulerXYZ` → `Matrix4` rotations, `Matrix4.hpp:88-113`, used by `PlayerBodyPointToWorld`, `PlayerPose.cpp:288-294`, and the hit model, `PlayerHitModel.cpp:40`), animation authority state (in digest since v4; `PlayerAnimation.cpp:172` and `PlayerPose.cpp:363` use `sin`/`cos` of yaw; the rest is `floor`/`fmod`/`round`/`llround`, exact), future AI decisions and their seeded RNG (E-10). |
| **Contract data** | Inside by rule 3 | **(c)** | `GYO::VoxelMesh` expansion and `MapMarkers` units: integers in 1/16 cell, one float conversion, `cell_size` range keeps every result normal (voxel-mesh.md Units, DI 5, DI 7). |
| **Content identity** | Inside by rule 4 | **(c)** required by the protocol today | `AnimationContentDigest` (SHA-256 over the FBX as **ufbx converts it at run time** on each end plus `presentation.json`; `PlayerAnimationDigest.cpp:54-108`; compared at join, `ClientConnection.hpp:128-131`, protocol-v9 §4.6) and `ArenaContentDigest` (JSON decimals and the expanded wall floats, `Arena.cpp:174-198`). Both run third-party conversion (ufbx compiled before the contraction flag; `strtod` from the libc) and are compared across Linux, Windows and arm64 in production. Guard today: `ArenaDigestTests` pins its value with non-binary decimals on every L1 platform; the animation digest has only the per-platform hash print of H6 (manual). |
| **Outside** | Presentation and transport | **(b)** | Pose sampling for rendering (same functions as the middle zone; whatever E1 changes in Math changes here too, which the hitbox requires: `PlayerHitbox.hpp:59`), HUD, `GYO::Audio` output (designed integer, section 4.5), rendering, the wire codec (carries bits, computes nothing on them), timing diagnostics. |

Rules every inside path obeys, with where each is stated and guarded today:

| Rule | Stated in | Guard today |
|---|---|---|
| No floating-point contraction in GYO code | math.md Numeric policy; `GyoBuild.cmake:41-42` (`-ffp-contract=off`; MSVC `/fp:precise`) | Flag only. The UI layout golden (`tests/common/ui/UiLayoutGolden.hpp:14-18`, literal binary32 patterns, "platform independent with FP contraction disabled") is the one existing literal-bit test that would catch contraction on arm64; the characterization tests feed run-time data (math.md) so an accidental contraction would show as a difference against the frozen legacy copies, but only where a legacy pair exists. On x64 no FMA instruction is available without `/arch:AVX2` or `-march`, which the presets do not set, so contraction can only occur on arm64 today. Gap: section 7, E2. |
| No FMA intrinsics, no `-ffast-math`, no `long double` | Not stated anywhere | Review only. Gap: E2 (source scan). |
| No platform libm except the exact functions (`sqrt`, `remainder`, `fmod`, `floor`, `ceil`, `trunc`, `round`, `lround`, `llround`, `rint`, `nearbyint`, `abs`, classification; IEEE 754 requires these correctly rounded or exact) | math.md:78 states the opposite side ("sin, cos, tan, pow, acos depend on the platform's libm") | The digest runner refuses a non-zero angle inside the golden subset (`AuthorityDigest.cpp:134-137`); `WrapRadians` uses `std::remainder` (exact, `Angle.hpp:21`); jumps use `std::sqrt` (`Movement.cpp:84`). Gap: `sin`/`cos`/`acos` in the middle zone and in non-zero-angle movement, `atan2`/`hypot` in the digest driver's aim (`AuthorityDigest.cpp:385-391`), `sin`/`cos` inside ufbx (`ufbx.c:259-263`, platform libm) for the animation digest (E1, E5). |
| Decimal text is converted by a correctly rounded `strtod` (libc, not libm) | Nowhere | `ArenaDigestTests.cpp:17-24` ("the golden value also pins strtod on every CI platform", width 20.1, cell_size 0.7). VoxelMesh goldens use exactly representable values only. |
| Double computation, one rounding to float, where a float result is needed | `Collision.hpp:17-22` ("every query computes in double and rounds its result to float once") | Characterization against frozen legacy helpers (`MathCharacterizationTests.cpp`, same host); `StaticAabbGrid` equivalence to the linear scan (collision.md DI 1; same host); the collision corpus two-tree tool (`tests/common/collision/CMakeLists.txt:5-8`, integer-generated inputs, built for a same-host comparison but portable by construction). Cross-platform only indirectly through the digest goldens that sweep (move-*). **E0 is about this rule**: the field session shows a sweep intermediate differing by 1 to 4 ULP between two x64 platforms. |
| Integer arithmetic where a bit-exact result must survive any platform | audio.md:35-37 (Q16 gains, int64 accumulation, half-up rounding, saturation); voxel-mesh.md Units (1/16-cell integers) | Audio: recorded FNV-1a hash of a one-shot sequence (`CharacterizationTests.cpp:49`), label `cpu`, so run on every L1 platform. VoxelMesh: literal-bit goldens (DI 5), label `cpu`. Both are (c) guards. |
| Total order from ids, never from containers | entity-store.md DI 2, 4, 8; entity-model E-5, E-6 (`EntityId` ascending, ties by id); voxel-mesh.md DI 7 (key order) | `es-row2-*` mutants; G1; rules M8/R4/C5. This is level (a) and costs no floating point. |
| Time and randomness enter as inputs | entity-store.md DI 8; entity-model E-10 (RNG from `(matchSeed, EntityId)`); the host's injected clock | Review; the digest runner's scripted clock; G10 planned for the AI batch. |

## 4. Module by module: what each pays, what guards it, what is missing

### 4.1 Authority digest (product `object_fps_pvp`, inside)

- 42 scenarios, 37 golden (`authority_golden.txt`, 37 non-comment lines). Golden means every yaw and pitch in the scenario is exactly zero, so `sin(0) = 0`, `cos(0) = 1` by C Annex F and no libm value enters. The five non-golden scenarios are `move-turning/*`, `combat-kill-respawn-aimed/product`, `combat-kill-respawn-aimed/contract` and `combat-graze/corridor`.
- Guard (`authority_digest_check.cmake`, test `object_fps_pvp.authority_digest`, labels `cpu;gameplay;pvp`): the optimized and `-O0` runners must agree on the golden subset, and the optimized runner must match the golden file. Run on every L1 platform, so the 37 goldens are a **(c) guard over linux-x64, windows-x64 and macos-arm64**. Differences in the five others are "information only" because Apple clang fuses `sinf`/`cosf` of one angle into `__sincosf_stret`, whose arm64 result can differ by an ULP (comment in the check script).
- What it pays: every golden scenario is axis-aligned; turning, aimed shots and grazes are never cross-checked across platforms. The aimed scenarios take their angles from `std::atan2` and `std::hypot` in the driver (`AuthorityDigest.cpp:385-391`), so even a deterministic `sin`/`cos` would not make them golden unless the driver's aim is deterministic too (E1 includes `Atan2`, or the driver uses literal angles). The hit model in the runner is synthetic (results do not move with clips), so clip sampling is not in the digest at all.
- Missing: non-zero angles (E1), sampled-pose hits (E3, only if DET-1 chooses (c)), a stated definition of the subset in the product contract (entity-model E-9 names the 37 goldens but not why they are 37).

### 4.2 Animation sampling (`GYO::Model` through `Math::Slerp`, middle zone today)

- Level **(b)**, measured: `v8-product-anim-bottlenecks` B3 (dev_log 2026-10-31, when CI still had four rows) found the pose hash identical on every platform across repeats, the load hash identical on all four platforms, and the pose hash in three groups (linux-x64 = macos-arm64, windows-x64, macos-x64). The cause is in sampling (`Slerp`'s `acos`/`sin`), not in the FBX conversion. Hitbox plan R5 and `v8-product-hitbox-golden-policy` followed: hits against animated targets compare same-platform two-tree only.
- Guard: same-platform goldens; `Animation Slerp equals Math Slerp bit for bit` (characterization against a frozen legacy copy that itself calls `std::acos`/`std::sin`, `MathCharacterizationTests.cpp:131-133`, `:1018`).
- The architecture checkup of 2026-11-04 noted that `engine/model` states no determinism promise at all. This document is that statement: **pose sampling is (b) until E1**.

### 4.2a (not moved)

Legacy section 4.2a (animation advance entering the digest's `animations` segment) is a candidate pending the determinism line, which is paused (kernel `HANDOFF.md`, 暫停). It stays in the legacy document and is not part of this page until that line decides it.

### 4.3 Movement (product, predicted core)

- `StepMovement` is one function used by Match and Client. Per step: `WrapRadians` (exact), `sin`/`cos` of yaw once (`PlanarMovement.cpp:17-18`), `Normalize` (sqrt, exact), Collision sweeps (double, one rounding), `sqrt` for the jump.
- Guard: digest goldens (zero yaw, three platforms); the 2026-11-09 field comparison (section 5) for arbitrary headings on windows-x64 against macos-x64, once, not in CI.
- Missing: the cause of the `sweep_fraction` ULP divergence (E0); a CI guard for non-zero yaw (E1 removes the libm dependence; until then `move-turning` stays non-golden).

### 4.4 Collision (`GYO::Collision`, predicted core)

- Pays: double computation with one rounding (`Collision.hpp:17-22`), rounding guards "far below" the single tolerance (`StaticAabbGrid.cpp:18-19`); the broadphase is bit-equal to the scan (DI 1). `std::floor` for cells is exact.
- Guard: same-host (characterization, 200 random sets against the scan, `grid-*` mutants, the corpus two-tree tool); cross-platform only through the digest goldens that sweep.
- Missing: E0 (the corpus tool is the natural instrument: run it on each platform and diff the records); a literal-bit golden of a few sweeps on every L1 platform would make its (c) direct instead of indirect.

### 4.5 Audio (`GYO::Audio`, outside)

- Pays: integer mixing (Q16 gains, int64, half-up, saturation; audio.md:35-37, DI 8), fixed 48 kHz PCM16 (DI 9). This is (c) by construction at no run-time cost, and the recorded one-shot hash (`CharacterizationTests.cpp:49`) checks it on every L1 platform.
- It is outside the boundary: nothing judged depends on audio output. The D0 table of PR #176 lists "float mixing" as what (b) would allow; this document recommends **keeping** the integer design regardless of DET-1, because its cost is zero and its Replacement Criteria (buses, effects) are the place to revisit it, not a determinism decision.

### 4.6 VoxelMesh and MapMarkers (`GYO::VoxelMesh`, contract data)

- Pays: integer units, one conversion whose every step is exact or rounds once, a `cell_size` range that keeps results normal (voxel-mesh.md Units, M4, DI 5). The JSON parse relies on the libc's decimal-to-double conversion being correctly rounded for `cell_size` and `origin`; `ArenaDigestTests` already pins `strtod` with non-binary decimals on every L1 platform through the same nlohmann → `std::strtod` path, so the reliance is guarded, just not at VoxelMesh's own entry (E2 adds one golden with `cell_size` 0.1 there).
- Guard: literal-bit goldens, label `cpu`, every L1 platform: a (c) guard.

### 4.7 EntityStore columns, becoming the World's storage (`GYO::EntityStore` today; inside)

- Pays: strictly ascending columns, order-preserving erase, id-ordered deferred changes (DI 2, 4, 8). This is level (a) ("results do not depend on history or containers") and has no floating point; it is inside because every digest column is encoded in that order (E-9).
- Guard: `es-row2-*` mutants, G1. Nothing missing.
- 2026-11-11 (decision `v8-arch-kernel-delta-approved`, spec K.3 item 7; written by `v8-arch-kernel-spec-v03`): under the mechanism design (`plans/kernel/mechanism.zh-Hant.md`, spec §A), EntityStore's columns become the World's storage. What it pays becomes the four World contracts of mechanism §3.3 (iteration by index; references invalid after a sync point; structural changes only through the deferred queue during iteration; queue applied in order). The guards move with it; this section is then about World.

### 4.7a Content identity and contract-data parsing paths (inside; added 2026-11-11, `v8-arch-determinism-table-fix`)

Two (c) paths the table missed, named by what they are rather than by the library (`target/naming.zh-Hant.md`):
- **Content identity**: the SHA-256 over converted content (`AnimationContentDigest`, `ArenaContentDigest`, section 4.9). Evidence: `engine/base` `Sha256` is the hash; the inputs are the converted bytes. Pays: byte-identical converted content on both ends (section 6 for ufbx). Guard: section 4.9's join-time comparison.
- **Contract-data parsing**: the strict JSON parse that turns `gyo.*` documents into runtime values (section 4.6). Evidence: `engine/base` `StrictJson`; decimals through `strtod`. Pays: the same document yields the same values on every platform. Guard: VoxelMesh and MapMarkers goldens (section 4.6).

### 4.7b Fixed-step accumulator and the host clock sample (added 2026-11-11, `v8-arch-determinism-table-fix`)

- **Fixed-step accumulator** (today `FixedTickRuntime`'s double accumulator; under the mechanism the runner plugin's accumulator, spec G-6; the "B5" below is the spec's revision tag B5, not a section): decides how many FixedUpdate runs happen this wake-up. It is (b): the count is host timing, not simulation state; the domain tick itself is advanced by the clock schedule (spec B5). Today's double accumulator and `deltaSeconds` are described in spec G-6; the product's step length becomes an integer `FixedStep{stepNs}`.
- **Host clock sample** (today `engine/time` read through `now_()` callbacks; under the mechanism the `HostClockSample` boundary resource, spec G.2): time is an input to the host-judgement zone, sampled once per wake-up and recorded, never read inside (c) (spec K.3 item 11; section 11 of this page). Pays: the sample is replayable. Guard today: the digest runner's scripted clock (section 2); target guards (not yet implemented): spec J-39, J-17.


### 4.8 Math (`GYO::Math`, inside wherever called)

- Pays: `-ffp-contract=off` for all GYO code (B1b, 2026-10-03, user decision), `constexpr` only for single-multiplication conversions, run-time data on both sides of every bit comparison (math.md Numeric policy).
- States: `sin`, `cos`, `tan`, `pow`, `acos` are platform libm (math.md:78). The callers inside the boundary are `Quaternion.hpp:67-68` (`MakeQuaternionFromAxisAngle`) and `:93-96` (`Slerp`), `Matrix4.hpp:88-113` (rotations), `PlanarMovement.cpp:17-18`, `ShotQuery.cpp:82-84`, `PlayerAnimation.cpp:172`, `PlayerPose.cpp:363`, and the digest driver's `atan2`/`hypot`. E1 gives Math its own `Sin`, `Cos`, `Acos`, `Atan2` (fixed polynomials built under the same flags, so the same bits everywhere) and switches **every** Math caller, presentation included, because the hitbox must be placed as the renderer draws the body (`PlayerHitbox.hpp:59`).
- Guard: characterization against frozen legacy helpers (same host); the digest goldens (three platforms, zero angle); the UI layout literal golden.

### 4.9 Content-identity digests (product, inside by rule 4)

- `AnimationContentDigest` (`PlayerAnimationDigest.cpp:54-108`): SHA-256 over `presentation.json` values and the character model **as ufbx converts it at run time** on the Match (`match_main.cpp:140`) and on each Client; the Client refuses a `Welcome` whose digest differs (`ClientConnection.hpp:128-131`; protocol-v9 §4.6, decided on B3's "load identical on four platforms"). ufbx is built in `third_party` before the contraction flag (`GyoBuild.cmake:35` vs `:41`) and its Euler-to-quaternion conversion calls the platform's `sin`/`cos` (`ufbx.c:259-263`, `ufbx_euler_to_quat`). This path is therefore a **production cross-platform bit comparison of third-party floating point**, which works today by measurement (B3 H-load) and is guarded only by the manual H6 hash print.
- `ArenaContentDigest` (`Arena.cpp:174-198`): fourteen arena members, decimals parsed by `strtod`, the expanded walls as float bits; compared at join; pinned by `ArenaDigestTests` on every L1 platform.
- Missing: a literal golden of the product animation digest on every L1 platform (E5, half a day); long term, pose data produced by a tool and read through a data contract, so the runtime runs no FBX conversion at all (the Engine does not carry tool duties).

## 10. Zones are declared contracts, checked at startup

Every System, component, resource and event declares `zone ∈ {C, B}`; there is no default (spec S-6). The Kernel does not decide zones, it checks the declarations against each other (rules Z1 to Z8, spec §G.1): a zone=C System's whole access set (reads, writes, membership filters, events, deferred writes, run conditions, set reads) lies in zone=C data; zone=C data is written only by zone=C Systems or hooks, with the single exception of ingress Systems writing `role = Input` data in the ingress stage; zone=C Systems have no effects except the write-only `Diagnostics` sink (`DiagnosticsWriter<T>`, spec S-8 v0.3: one-way (c) to (b), not World data, no conflict edges), no `Local`, no change filters, no `ExclusiveWorld`; `role = Input` data is `rollback = No`. A violation is a startup error, not a runtime check.

## 11. Where (c) code may take time from (user decision 2026-11-11, spec K.3 item 11)

A zone=C System may read time from exactly two sources:

1. **The World's domain tick** (`tick[domain]`, advanced only by the first step of that domain's clock schedule; inside the rollback snapshot).
2. **Recorded boundary inputs**: zone=C, `role = Input` resources and columns written by the Host while the World is `Ready` or by an ingress System in the ingress stage, and therefore replayable from a record. Today's shapes: `HostClockSample` (the Host samples the clock once per wake-up and writes it), and the arrival time carried with every inbound message (`LinkInputInbound = {arrivalNs, bytes}`, written into `InputRing[tick]` by ingress).

A zone=C System reading a clock itself stays forbidden: wall-clock resources such as `FrameTime` are `external`, zone=B, and rule Z1 keeps (c) away from them; `<chrono>` in a (c) source file is caught by the source-file scan (spec J-17). A System other than ingress declaring a write to a boundary resource, or any System declaring a write to the domain tick, is a startup error (spec J-39).

This resolves the conflict between the concept document (`mechanism.zh-Hant.md` §3.3, formerly "time comes only from the tick") and the current contract's DI 4 ("the tick, or a clock injected"): the mechanism text was amended; DI 4 stands. The host-judgement zone (section 2) needs arrival timing and stays (c); its inputs are now data in the ring rather than a clock callback into the tick.

## 12. Randomness

Random state is a per-entity zone=C component with `rollback = Yes` (entity-model E-10's `Brain.rngState`), seeded by the product and included in the digest. The Kernel provides no random source; a single shared resource was rejected because replaying one entity's order change would disturb every other entity's sequence (spec appendix X20).

## 13. The four contracts of a (c) zone (spec §G.2)

Serialised execution is necessary, not sufficient (PR #177: the predicted core is serialised today and still shows 1 to 4 ULP differences). A (c) zone rests on one contract in each of four places:

| Place | Contract | Guards |
|---|---|---|
| World | iteration ascending by key; references invalid after a sync point; structural changes only through the deferred queue during a run; queue applied in the fixed order; allocator monotonic and serialised by conflict analysis; no hash or address order | spec J-2, J-3, J-4, J-8, J-9, J-31, J-32 |
| Executor | one Plan per schedule, (c) ambiguity is a startup error; Serial by default, Parallel only with a bit-identical guard; time from section 11; randomness from section 12 | spec J-20, J-22, J-27, J-39 |
| Ingress | external messages enter only through the Link family of inbound Resources (and assets through `AssetInbox`); only ingress Systems write the role=Input staging (`InputRing[tick]`, `MembershipRequests`, `AssetStaging`); arrival time is data; (c) Systems never touch `Link`; in replay the ring is read-only | spec J-40, J-41, J-42 |
| Math / build | current contract DI 3 (no contraction, no FMA or ISA intrinsics, no fast-math, exact libm only, Math's own transcendental functions after E1); the Kernel cannot check this, it only emits the list of (c) source files for the scan | current contract E0 to E5; spec J-17 |

## Design Invariants (candidates; become binding once approved)

Item numbers are unchanged. An item marked "bound by kernel DI-KS n" has its full text only in that clause of the kernel page; the others are specific to this component.

1. **Zones are declared, never defaulted, and checked at startup**: bound by kernel [DI-KS 8](kernel.zh-Hant.md#di-ks-8): **full text and guards are in that clause; read it before changing this component**.
2. **(c) time comes only from the domain tick and recorded boundary inputs.** The kernel half (a boundary resource is written only by the Host while the World is `Ready` or by an ingress System in the ingress stage; the domain tick is advanced only by the first step of its clock schedule and is written by no System and not by the Host directly; a startup error otherwise) is kernel [DI-KS 8](kernel.zh-Hant.md#di-ks-8), required reading. Specific to this page, binding products and Hosts: no (c) source file reads a clock (`<chrono>`, `engine/time`); the Host samples the clock once per wake-up and records it as `HostClockSample`; the arrival time travels with each inbound message as data. Guards: the source-file scan over the product's (c) lists (spec J-17, `docs/architecture/zone-c-sources.zh-Hant.md`); the digest runner's scripted clock; review.
3. **(c) randomness is per-entity rollback state seeded by the product.** The kernel half (the kernel has no random source) is kernel [DI-KS 15](kernel.zh-Hant.md#di-ks-15), required reading. Specific to this page, binding products: random state is a zone=C component with `rollback = Yes`, seeded by the product and included in its digest; no shared random resource. Guard: the product's digest.

### Replacement Criteria (candidates)

- LIM-2: v1 is Serial only; see `limits.zh-Hant.md`.
- If a (c) rule can be stated only with time that is neither the tick nor a recorded input, the input model is wrong: add the recorded input, do not open a clock read.
