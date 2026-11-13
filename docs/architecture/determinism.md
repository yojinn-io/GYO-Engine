# Determinism

> **Legacy** (current-state record; `v8-arch-docs-legacy`, 2026-11-12). Design authority is [`docs/architecture/target/`](target/README.md). This document takes only two kinds of updates: the current state of code that still exists, and Design Invariants that tests still guard. Existing code is bound by this document's Design Invariants; new code (from `engine/kernel` on) by target. Target design: [`target/determinism.md`](target/determinism.md); it replaces this document when the migration batch completes (the batch is set by `v8-arch-migration-plan`).
>
> 2026-11-13 (`v8-arch-a6-revision`): sections 1, 2 and 4 (with 4.7a and 4.7b, without the candidate 4.2a) were copied to `target/determinism.md` under the same section numbers; **the authority for those three sections is now the target page** and the copies here are frozen. Sections 0, 3, 5 to 9 and 4.2a, the plan items E0 to E5 and the Design Invariants and Replacement Criteria below stay here: they belong to the determinism line, which is paused (kernel `HANDOFF.md`), and where they go is decided before the closeout batch deletes this file. In section 4.7b, "`target/determinism.md` section 2" now means its section 11 (that page's own sections were renumbered 10 to 13).

Updated 2026-11-11. Owner: Engine (architecture). Written by task card `v8-arch-determinism-levels` (docs only; no code,
flag or golden changed). **Status: proposal.** The `Design Invariants` section below is a candidate; it binds only
after the user approves it. The decisions the user has to make are in section 0. The guard-expansion plan and the
flag fix are in [plans/determinism](plans/determinism/README.zh-Hant.md). The adversarial review of this document and
the replies are in section 9; it changed the boundary (section 2), the evidence (section 5) and the plan.

This contract answers one question for every module: which level of reproducibility it promises, what it pays for
that promise, and what guards it. It does not change any module's contract by itself; where it names a rule a module
already states, that module's document stays the authority and this one points at it.

## 0. Decisions for the user

| # | Decision | Recommendation | What it costs if taken, what it costs if not |
|---|---|---|---|
| **DET-0** | **The predicted core is not (c) today, and the cause is unknown.** The 2026-11-09 field session (section 5) has steps with identical states and inputs on both ends whose `sweep_fraction` differs by 1 to 4 ULP between the Windows x64 Client (MSVC) and the macOS x64 Match (AppleClang), while the positions agree. No libm is on that path. Approve a detection batch (E0 in the plan: reproduce with the collision corpus on each platform, bisect the operation; 1 to 2 days) **before** any (c) guard is widened? | Yes, first. AGENTS §14.3: a divergence that is measured but not explained is recorded, and an observation path is built before a fix or a wider promise. Every (c) statement below is conditional on E0's result. | Skipping E0 and writing (c) into a Design Invariant would approve a promise the code already breaks; E1 would then regenerate goldens that still differ between platforms. |
| **DET-1** | **The middle zone**: authority results that no Client predicts, but that enter the authority digest or decide a match. Today that is the hit decision against an animated target (its hitbox pose comes from clip sampling, which is level (b), section 4.2) and, from the first AI batch, AI decisions (`entity-requirements` 4-6 says "Match-consistent only"). Level (b) or level (c)? | **(c) as the target for everything the authority digest encodes**, reached by one Engine batch after E0 (E1: deterministic `sin`, `cos`, `acos`, `atan2` in Math, 4 to 6 days, estimate), scheduled before the first AI batch so that AI scenarios with angles can be goldens from the start. Until E1 lands, the middle zone is **explicitly (b)**, and every cross-machine comparison of it is same-platform (the existing same-host two-tree rule). | Taking (c): E0 then E1, one golden regeneration under AGENTS §14.4 (the characterization tests that freeze `std::acos`/`std::sin` copies loosen; section 9, S5), and the rule that no boundary code calls platform libm except the exact functions. Staying at (b): a Linux Match incident can only be replayed on Linux (the docker Match exists), cross-platform goldens stay restricted to zero-angle scenarios, and every two-end diagnosis (Client against Match) needs a tolerance, which hides small defects. |
| **DET-2** | Third-party code is **outside the boundary unless proven**: a library may produce a boundary result only when its flags and intrinsics are controlled by GYO's build and a cross-platform golden covers the path (section 6). Accept, knowing that **ufbx is inside today** through the animation content digest (section 4.9) and must either be guarded (E5, a literal-digest test on every L1 platform, half a day) or leave the runtime (pose data baked by a tool, a data contract)? | Accept, with E5 now and the bake-time move as the long-term answer (the Engine does not carry tool duties). | Not accepting means the third-party evaluation (PR #176, D1 and D2) has to carry a determinism proof for every candidate, which none of the surveyed libraries offers without a flag like Jolt's `CROSS_PLATFORM_DETERMINISTIC`. |
| **DET-3** | Order and timing of the guard expansion (plan E0 to E5)? | E0 (detect) → E2 (flag order, configure check, intrinsic scan, canary on arm64) and E5 (content-digest golden) as small Engine batches now → E1 (deterministic trig, goldens) and E3 (sampled pose in the digest, only if DET-1 = (c)) as one batch before the first (c)-zone System is migrated onto the mechanism (feature lines paused 2026-11-11; the AI batch no longer anchors this timing) → E4 absorbed: `plans/kernel/mechanism.zh-Hant.md` §3.1 declares the determinism zone per System as an explicit contract (PR #175 merged 2026-11-11; blueprint demoted to input). | Doing E1 after the AI batch leaves every AI scenario with an angle non-golden until then. Doing it before the queued product work delays that work by the same days. |

Everything below is evidence and definitions for those decisions.

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

## 3. What the boundary is not

- **Not "the Engine"**: Audio and Render are Engine modules outside the boundary; Movement and the digest are product code inside it. The boundary follows the data (what is judged, predicted, contracted or compared at join), as AGENTS §10 asks (directories are not ownership).
- **Not a tolerance**: a comparison with a tolerance is a measurement (a lag-l2 distribution), never a determinism guard. Goldens compare bits.
- **Not a promise about what the Client shows**: prediction tolerates (b). `LocalPlayerPrediction::Reconcile` replays the pending commands from the authority state and turns any displacement above 10^-6 into a correction slewed over `MovementCorrectionSeconds`, snapping only beyond `MovementHardCorrectionDistance` (`LocalPlayerPrediction.cpp:267-333`), so a one-ULP divergence (1.9 × 10^-6 at 30 m) would be a correction too small to see. (c) in the predicted core buys *diagnosis*, not play: without it, every two-end comparison needs a tolerance and cannot tell a small defect from a rounding difference.
- **Not covered by the offline analyzers**: `lag_l2_summary.py`, `lan15_judge.py`, the shot and ingress analyzers read logs and judge counts and distributions; they recompute no simulation. The acceptance probe `hit_aim.hpp` runs the product's hit model on the test host to aim, with a margin, not bit for bit.

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

### 4.2a Animation advance (candidate, pending the determinism line; `v8-arch-determinism-table-fix`)

The audit's zone cross-check (audit section 4.5) flags that the **animation advance that enters the digest's `animations` segment** (today `PlayerAnimation`; under the mechanism an `AnimationState` column written by an `AdvanceAnimation` System) is (c) by rule 1, while section 4.2 lists `GYO::Model` sampling as the middle zone. The two differ: sampling (poses for hit geometry) is the middle zone; the advance of the animation state encoded into the digest is inside. The determinism line verifies this against the digest encoding (`AuthorityDigest.cpp`) before the table changes; until then this entry is a candidate.

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
- **Host clock sample** (today `engine/time` read through `now_()` callbacks; under the mechanism the `HostClockSample` boundary resource, spec G.2): time is an input to the host-judgement zone, sampled once per wake-up and recorded, never read inside (c) (spec K.3 item 11; `target/determinism.md` section 2). Pays: the sample is replayable. Guard today: the digest runner's scripted clock (section 2); target guards (not yet implemented): spec J-39, J-17.


### 4.8 Math (`GYO::Math`, inside wherever called)

- Pays: `-ffp-contract=off` for all GYO code (B1b, 2026-10-03, user decision), `constexpr` only for single-multiplication conversions, run-time data on both sides of every bit comparison (math.md Numeric policy).
- States: `sin`, `cos`, `tan`, `pow`, `acos` are platform libm (math.md:78). The callers inside the boundary are `Quaternion.hpp:67-68` (`MakeQuaternionFromAxisAngle`) and `:93-96` (`Slerp`), `Matrix4.hpp:88-113` (rotations), `PlanarMovement.cpp:17-18`, `ShotQuery.cpp:82-84`, `PlayerAnimation.cpp:172`, `PlayerPose.cpp:363`, and the digest driver's `atan2`/`hypot`. E1 gives Math its own `Sin`, `Cos`, `Acos`, `Atan2` (fixed polynomials built under the same flags, so the same bits everywhere) and switches **every** Math caller, presentation included, because the hitbox must be placed as the renderer draws the body (`PlayerHitbox.hpp:59`).
- Guard: characterization against frozen legacy helpers (same host); the digest goldens (three platforms, zero angle); the UI layout literal golden.

### 4.9 Content-identity digests (product, inside by rule 4)

- `AnimationContentDigest` (`PlayerAnimationDigest.cpp:54-108`): SHA-256 over `presentation.json` values and the character model **as ufbx converts it at run time** on the Match (`match_main.cpp:140`) and on each Client; the Client refuses a `Welcome` whose digest differs (`ClientConnection.hpp:128-131`; protocol-v9 §4.6, decided on B3's "load identical on four platforms"). ufbx is built in `third_party` before the contraction flag (`GyoBuild.cmake:35` vs `:41`) and its Euler-to-quaternion conversion calls the platform's `sin`/`cos` (`ufbx.c:259-263`, `ufbx_euler_to_quat`). This path is therefore a **production cross-platform bit comparison of third-party floating point**, which works today by measurement (B3 H-load) and is guarded only by the manual H6 hash print.
- `ArenaContentDigest` (`Arena.cpp:174-198`): fourteen arena members, decimals parsed by `strtod`, the expanded walls as float bits; compared at join; pinned by `ArenaDigestTests` on every L1 platform.
- Missing: a literal golden of the product animation digest on every L1 platform (E5, half a day); long term, pose data produced by a tool and read through a data contract, so the runtime runs no FBX conversion at all (the Engine does not carry tool duties).

## 5. Evidence

1. **Field session 2026-11-09** (`build/evidence/v8-lag-l2-session-20261109/session/`; analysis in `build/evidence/determinism-levels/`, main checkout, not in git). Match on macOS 26.7.1 x64 (Intel i9, AppleClang local build), three Windows 11 x64 Clients (CI windows-x64 package; the `win3` folder holds the tester who ran the win2 kit, `MAPPING.txt`), all at source revision `24943706`. Joining Match and Client `step` records on (player, life, epoch, sequence) and comparing every recorded field as the `%.17g` text both ends write (`AppendTraceDouble`; `-0` and `+0` are not told apart by the comparison):

   | Client | joined | equal in every field | differing |
   |---|---|---|---|
   | win1 | 80,788 | 80,565 | 223 |
   | win2 | 80,197 | 79,985 | 212 |
   | win3 | 101,716 | 101,578 | 138 |

   Of win1's 223: 170 already differed before the step (a divergence propagating; every one traces to the previous step's result); 51 had equal state before and a different position after. The adversarial check joined those 51 with `match-commands.jsonl`: 37 were `held` (a substituted command; the Match substituted 2,223 commands and took 1,283 first commands late in the session), 14 were `actual` commands at a life change with the Match stationary; their displacement directions differ by multiples of 0.143°, so they are different **inputs**, not different arithmetic. The remaining **2** are the finding of DET-0: equal state before, equal position after, equal input, and `sweep_fraction` differing by 1 ULP ((6,1,1,7481)) and 4 ULP ((5,4,4,241)). win2 has the same kind at 1 to 2 ULP ((3,1,1,865), (3,31,32,939), (3,33,34,4483)), win3 one at 1 ULP in both `sweep_fraction` and `sweep_normal.z` ((4,11,12,5276)). The steps carry no yaw field; headings are inferred from displacement directions, which are not axis-aligned. **What this shows**: Movement with Collision sweeps agrees to the bit on 99.7 % of steps between MSVC/UCRT and AppleClang/Apple libm on x64, including `sinf`/`cosf` of arbitrary yaws, and **does not** agree in a sweep intermediate on a handful of steps, for a reason not yet found. Correction to the card's premise: the Match in that session ran on macOS, not Linux, and "both ends agree" is 99.7 % with the remainder explained at the input level except for those ULP cases.
2. **B3** (dev_log 2026-10-31): pose sampling differs across platforms, load does not (section 4.2).
3. **CI goldens**: `object_fps_pvp.authority_digest` (37 scenarios), `gyo_audio_tests` (recorded hash), `gyo_voxel_mesh` goldens, `ArenaDigestTests`, the UI layout golden, all label `cpu`, pass on the three L1 platforms on every merged PR.
4. **Manual hashes**: `PlayerAnimationDigestTests`, `anim_bottlenecks` and `hitbox_survey` print per-platform hashes for the H6 comparison by a person; nothing fails when they differ.

## 6. Third-party code

Rule (DET-2): third-party code is outside the boundary unless (i) GYO's build sets its contraction mode and forbids its FMA intrinsics (per target, checked at configure time like `gyo_expect_*_links`), and (ii) a cross-platform golden covers the path that uses it. Facts today:

- `GyoBuild.cmake` adds `-ffp-contract=off` **after** `add_subdirectory(third_party)` (line 35 vs 41), so compiled third-party targets (SDL3, SDL_image, SDL_ttf, `gyo_ufbx`, imgui, doctest) build with the compiler default (Clang fuses on arm64). Its comment "applies to the directories added below" is accurate but the consequence was not written anywhere until PR #176 §2.5. The fix is in the plan (E2).
- Header-only code (nlohmann/json through `Base::StrictJson`, doctest headers) is compiled inside GYO's translation units and inherits GYO's flags.
- **ufbx is inside today** through the animation content digest (section 4.9): it violates (i) and has no (ii). E5 supplies (ii); moving the conversion to a tool removes it from the runtime.
- Candidate libraries from PR #176 (Jolt, Recast/Detour, ozz): Jolt needs `CROSS_PLATFORM_DETERMINISTIC` and `USE_FMADD=OFF`, ozz uses `_mm_fmadd_ps` under `__FMA__`, Detour calls libm; none qualifies for the boundary without both (i) and (ii). Their D1/D2 decisions are re-read after DET-1.

## 7. Gaps, in one list

| Gap | Zone | Closed by |
|---|---|---|
| A sweep intermediate differs by 1 to 4 ULP between windows-x64 and macos-x64 with equal inputs; cause unknown | predicted core | E0 (detect and bisect; the corpus two-tree tool on each platform) |
| Non-zero angles never cross-checked across platforms (5 digest scenarios "information only"; the aimed ones also take `atan2`/`hypot` inputs) | predicted core, middle | E1 (deterministic trig incl. `Atan2`, or literal angles in the driver), then flip the 5 scenarios to golden |
| No canary that a build is uncontracted; no scan for FMA intrinsics; flag order excludes compiled third-party | all inside | E2 (the canary can only fail on arm64; the configure-time check is the x64 guard) |
| Animation content digest compares ufbx output across platforms with no CI guard | content identity | E5 now; bake-time pose data later |
| Pose sampling (b); hits against animated targets same-platform only | middle | E3 (only if DET-1 = (c)): a digest scenario with a synthetic two-key clip through `SamplePose`, after E1 |
| VoxelMesh goldens use only exactly representable decimals | contract data | E2 (one golden with `cell_size` 0.1; `ArenaDigestTests` already pins `strtod`) |
| `engine/model` states no level | middle | this document (4.2); a line in `animation-events.md` or a `model.md` pointing here (docs, with E4) |
| Kernel blueprint DI-K5 says "pure function" without a level | — | absorbed 2026-11-11: mechanism §3.1 (zone per System) and §3.3 (four contracts); section 8 wording kept as the definition |

## 8. Wording for the Kernel blueprint (DI-K5)

2026-11-11: the blueprint was demoted to an input of `plans/kernel/mechanism.zh-Hant.md`; the wording below now serves as the definition behind mechanism §3.1 (determinism zone as an explicit System contract) and §3.3 (the four contracts of a (c) zone). Original proposal for the DI-K5 row of `docs/architecture/plans/kernel/blueprint.zh-Hant.md`:

> **DI-K5 step 是純函數、時間只來自 tick、等級由決定性契約指定**：模擬 step＝Step(state, inputs, tick)，可在同一幀重入（回滾）；固定步內不用 f32 delta；變更 tick 用遊戲 tick 標記；時鐘與亂數只以輸入的形式進入 step。決定性邊界內的 step（[determinism.md](../../determinism.md) §2）達到該區所指定的等級：預測核心、host 判斷、契約資料、內容身分為 (c)，中間地帶照 DET-1；邊界外為 (b)。守衛：既有 digest 與重放；回滾重跑的等價測試；邊界內的每個模組有跨平台 golden。

Writing "(c) everywhere inside" into K5 today would state something the code does not do (sections 4.2 and 5); the wording above states the level by zone and points here.

## Design Invariants

Candidate. In force only after the user's approval; until then this section is a proposal and changing it is not a Delta.

1. **The boundary is written down.** The paths inside the determinism boundary are those that meet one of the four rules of section 2 (digest-encoded, Client-predicted, tool-runtime contracted, compared at join). Adding a field to `DigestFields`, predicting a new quantity on the Client, making a new tool-runtime bit contract, or comparing a new digest at join puts that path inside and is an Architecture Delta that names its zone and level here. Guard: review; a change to `DigestFields` or to a join-time digest is a review item that must cite this section.
2. **One level per zone, and the gaps are listed.** Predicted core, host judgement, contract data and content identity are (c); the middle zone is at the level DET-1 fixes; outside is (b). A path may not be held to a lower level than its zone by adding a tolerance. Where a (c) path has no cross-platform guard yet, section 7 lists it with the batch that closes it; a gap not listed there is a defect. Guard: every (c) path has a literal-bit or recorded-digest golden that runs under the `cpu` label on every L1 platform, or an entry in section 7; a (b) path has a same-platform golden.
3. **Inside the boundary, arithmetic is portable.** No contraction (the flag), no FMA or other ISA intrinsics, no `-ffast-math`, no `long double`, no platform libm except the exact functions (`sqrt`, `remainder`, `fmod`, `floor`, `ceil`, `trunc`, `round`, `lround`, `llround`, `rint`, `nearbyint`, `abs`, classification); transcendental functions come from Math's own deterministic implementations (after E1) and until then are a listed gap under rule 2. Decimal text enters through a correctly rounded `strtod`, pinned by a golden with non-binary decimals. Guard: the contraction canary and the intrinsic scan (E2); the `strtod` goldens; review.
4. **Inside the boundary, order and inputs are explicit.** No clock read, no random source other than a seeded, digest-encoded state, no thread, atomic, pointer comparison, or iteration over a hash-ordered container decides a result; time enters as an input (the tick, or a clock injected at construction, as the host's `now_`). Digests of bytes (SHA-256, FNV-1a) are allowed; iteration in hash order is not. (Restates entity-store 8, entity-model E-10, voxel-mesh 7 at the boundary level.) Guard: the module guards named in section 2; the digest runner's scripted clock; review.
5. **Third-party code is outside unless proven** (section 6). Today's one exception, the animation content digest over ufbx output, is listed in section 7 with E5 and the bake-time exit. Guard: configure-time flag check and cross-platform golden for any exception; review.
6. **Goldens compare bits, and change only under §14.4.** A golden is regenerated only with the disclosure AGENTS §14.4 asks for (what loosens, what tightens, where the property is still guarded). Guard: review; `git diff` of golden files and of frozen legacy copies in characterization tests is a review item.

### Replacement Criteria

Stop patching and let the user decide between keeping this boundary and redrawing it when any of these holds:

1. A feature inside the boundary needs a function Math cannot make deterministic at acceptable cost (a transcendental the polynomial approach cannot match within the feature's accuracy, or a library the feature cannot do without) — then either the path moves to the middle zone at (b) with a same-platform replay facility, or the library is brought inside under section 6.
2. A supported platform cannot honour "no contraction" (a toolchain that ignores the flag, or an ISA where the default cannot be turned off) — then (c) shrinks to the platforms that can, and the goldens become per-group.
3. E0 finds that the ULP divergence comes from the compilers themselves (not from a library call or a code path), so that (c) between MSVC and Clang cannot be had for double arithmetic at all — then the predicted core is (b) with per-platform goldens, and this document's zone table is rewritten.
4. The goldens of a (c) path have to be regenerated for a reason other than an intended rule change more than once in a version — the guard is then measuring the toolchain, not the rules.
5. The same tolerance appears in two or more comparison tools over boundary data — the boundary is not delivering (c) and the zone table is wrong.

## 9. Adversarial review

Checker: a read-only subagent (general-purpose, Opus, high; the author ran on Fable 5.1, so the checker is the other model, as the card asked). It was asked to break the boundary, find guards this document missed, check every citation, audit the evidence scripts, and attack the candidate invariants and E1. Its scripts are in the session scratchpad only (not evidence). It reported 6 blockers, 8 shoulds and 7 nits; the author re-verified each against the repository and the evidence before replying.

| # | Finding (checker) | Verified | Reply and change |
|---|---|---|---|
| F1 | L1 has three platforms; macos-x64 has no L1 row since 2026-10-31 (`package_contract.py:18`, `cross-platform.yml:287`, `5ea8fcc0`). The draft said "four" throughout. | Yes | Accepted. Section 1 "Platforms"; every "four" corrected; B3 dated as a four-row measurement; the session's Match platform noted as uncovered by CI. |
| F2 | Two win1 steps with equal input, state and position differ in `sweep_fraction` by 1 and 4 ULP; the classification scripts skipped them (they filtered on position). win2 and win3 have more. The claim "none of the 53 is a rounding difference" is false. | Yes (`intermediate_only_diffs.py`, all three Clients) | Accepted; this is the most important finding of the card. New DET-0 and E0; section 5 rewritten with the numbers; RC 3 added; "51" corrected. |
| F3 | The digest encodes the Client's `renderPosition`, `correctionOffset`, `interpolationAlpha`, phase corrections, and the host's quality and eviction; the draft put interpolation outside and said nothing of the host. Eviction reads a clock, against DI 4. | Yes (`DigestFields` lines 63-66; `MatchRuntimeHost.cpp:603`) | Accepted. Predicted core now includes the digest's prediction record; new zone "Host judgement"; DI 4 reworded: time enters as an input (the host's clock is injected). |
| F4 | `AnimationContentDigest` (ufbx at run time, platform `sin`/`cos`, built before the flag) is compared at join across platforms: a production cross-platform bit comparison of third-party arithmetic; `ArenaContentDigest` likewise. DET-2's "nothing third-party inside today" was false. | Yes (`PlayerAnimationDigest.cpp`, `ClientConnection.hpp:128-131`, protocol-v9 §4.6, `ufbx.c:259-263`) | Accepted. New rule 4 (content identity), section 4.9, DET-2 reworded, E5 added, DI 5 names the exception. |
| F5 | The aimed digest scenarios take `std::atan2`/`std::hypot` inputs from the driver, so E1's "42/42 golden" is unreachable with `Sin`/`Cos`/`Acos` alone. | Yes (`AuthorityDigest.cpp:385-391`) | Accepted. E1 adds `Atan2` (or literal angles in the driver) and names this. |
| F6 | E1 missed `Matrix4` rotations (used by `PlayerBodyPointToWorld` and the hit model), `MakeQuaternionFromAxisAngle`, `PlayerPose.cpp:363`; and "presentation not switched" contradicts `PlayerHitbox.hpp:59`. | Yes | Accepted. Section 4.8 lists every caller; E1 switches all Math callers. |
| S1 | Of the 51, 37 are `held` and 14 are `actual` at life changes; "one side stationary" is 15 not 1; the minimum direction difference is 0.143°; one case is a wall sweep (0.0435 vs 0.0428). | Partly re-verified (the command join was not re-run by the author; the scripts' stationary count was) | Accepted as the checker's result, attributed as such in section 5; the author's classification script is kept as it ran, its limits noted in the evidence README. |
| S2 | win2 and win3 unused. | Yes | Accepted; both compared, table in section 5. |
| S3 | `ArenaDigestTests` already pins `strtod` with non-binary decimals. | Yes | Accepted; sections 2, 4.6, 7 and E2 corrected. |
| S4 | Missing guards: UI layout literal golden (a contraction canary already), the collision corpus tool, the manual hash prints. | Yes | Accepted; listed in sections 2, 4.4, 5. |
| S5 | E1 breaks the characterization tests that freeze `std::acos`/`std::sin` copies; "nothing loosens" was wrong. | Yes (`MathCharacterizationTests.cpp:131-133`, `:309-331`) | Accepted; the plan's E1 carries a §14.4 disclosure with a loosening side. |
| S6 | The canary cannot fail on x64 (no FMA instruction without `/arch:AVX2`); the scan missed `apps/object_fps_pvp/src/Collision`, `engine/model`, the digest driver. | Yes | Accepted; E2 reworded (canary = arm64 guard, configure check = x64 guard; directories added). |
| S7 | "DigestVersion 5 arrives anyway" has no source and conflicts with E-9/G11. | Partly: entity-model §5.4 (line 251) does plan DigestVersion 5 for the first non-player batch; E-9/G11 do require existing goldens unchanged there, so the "regenerate once" argument was wrong. | Reasoning replaced: E1 is the only regeneration; it goes before the AI batch so AI scenarios can be goldens from the start. |
| S8 | DI 1 approved-and-violated (F3, F4), missed `LocalShotGate`; DI 2 violated today (non-zero yaw); DI 3 whitelist missed `llround`, had an undecidable exit, ignored `strtod`; DI 4 vs the host clock and content hashes; audio (a) misquoted. | Yes | Accepted; all six invariants reworded (gap list under DI 2; whitelist extended; `strtod` rule; time as input; hashes of bytes allowed). |
| Nits | Quote belongs to `StaticAabbGrid.cpp:19`; audio.md:35-37 and DI 9; one ULP at 30 is 1.9 × 10^-6; no yaw field in the records; `-0`/`+0` conflation; the join key is correct (first predictions only, no duplicate keys); `hit_aim.hpp` probe. | Yes | All corrected in sections 2, 3, 4.5 and 5. |

Not found by either side: whether the UCRT's `strtod` is correctly rounded on every input (pinned on three CI platforms only); whether any dependency sets FTZ/DAZ on a thread (no `_mm_setcsr`/`FLUSH_ZERO` in the repository or the fetched sources that were read; SDL not read in full); whether a respawn yaw reaches `PlayerAnimation.cpp:172` inside a golden scenario (inferred not, from the teleport reset; not executed).
