# Collision: the static broadphase

Owner: Engine (`engine/collision` → `GYO::Collision`; `engine/collision/StaticAabbGrid.hpp`).
Plan and record: [big map](plans/big-map/README.md), batch v8-arch-big-map-engine-grid (survey: one moving body's
step over 20,691 boxes costs about 2.2 ms with a linear scan, about half of a 60 Hz tick for four).

This page covers the static broadphase only. The exact queries (`RaycastAabb`, `SweepVerticalCapsuleAgainstAabb`,
`OverlapVerticalCapsuleAabb` and the rest), their numeric semantics and their single 1e-7 tolerance are stated in
`engine/collision/Collision.hpp`; the module's place is in the [module table](../architecture.md).

**Status (2026-11-06): added by v8-arch-big-map-engine-grid (PR #161).** No product uses it yet; object_fps_pvp is planned to
replace its linear scans with it (v8-arch-big-map-product-arena-v3).

## What it is

`StaticAabbGrid` is a uniform grid over a fixed set of axis-aligned boxes. It is built once from a span of boxes and
a cell length and never changes. It only decides which boxes a query tests; every box it returns or uses is tested
with the exact queries above.

## Contract: equivalence

Every query returns, bit for bit, what a linear scan of all boxes in index order 0, 1, …, n − 1 returns:

| Query | The linear scan it equals |
|---|---|
| `Overlapping(b)` | every index `i` whose box intersects `b` (closed: touching counts), increasing |
| `RaycastNearest(ray, d, tie)` | `h = RaycastAabb(ray, d, box i)`; keep `(h, i)` when there is none yet, or `h < best` (`LowestIndex`) / `h <= best` (`HighestIndex`) |
| `SweepVerticalCapsuleNearest(c, δ, tie)` | the same on the fraction of `SweepVerticalCapsuleAgainstAabb(c, δ, box i)` |
| `SweepVerticalCapsuleAll(c, δ)` | every `(SweepVerticalCapsuleAgainstAabb(c, δ, box i), i)` that has a value, increasing `i` |
| `OverlapVerticalCapsuleAll(c)` | every `(OverlapVerticalCapsuleAabb(c, box i), i)` that has a value, increasing `i` |

- **Ties are the caller's choice.** Consumers break ties differently today (one keeps the later of equal hits,
  another the earlier), so the tie rule is a parameter, not a policy of this module.
- **`…All` keep the consumer's policy outside.** A consumer that wants "any contact with an upward normal", "the
  deepest penetration" or a filtered nearest runs its own loop over the `…All` result, which arrives in index order
  as its scan did. The grid owns the conservative candidate region, so no consumer needs to know how far a rounded
  sweep can reach.
- **The cell length is a hint.** The grid uses at most 2^22 cells; a cell length that would make more is doubled
  until it fits. Results never depend on the length in use (`CellLength()`).
- **Preconditions.** Every box valid (`IsValid`), the cell length finite and positive, fewer than 2^32 boxes, fewer
  than 2^32 registrations (each box counted once per cell it covers; about 1,024 boxes that each cover all 2^22 cells
  reach the limit), and query inputs as for the exact queries; anything else is a Programmer Error (`GYO_ASSERT`).

### How it stays exact

- A coordinate's cell is `floor((x − origin) / cell)` in double, where `origin` is the minimum corner of all boxes.
  The function is monotonic, so two closed intervals that intersect always have cell ranges that intersect; each
  box is registered in every cell its closed extent covers, and `Overlapping` needs no margin.
- Capsule and ray queries collect candidates from bounds inflated by `1e-3 + 1e-5 × the largest coordinate
  magnitude`. The inflation is needed, not only a safeguard: the exact sweep accepts a root up to 1e-12 of the path
  beyond its end and clamps it, so a box just past the end still reports a contact (a test keeps that case); the
  inflation also covers double rounding. Extra candidates cost time only.
- A ray is walked in pieces one cell long, from its entry into the hull of all boxes; once a hit is known, pieces
  that start beyond it (plus the margin) are skipped, since any equal or nearer hit lies in an earlier piece.
- The tie rule is applied as "smaller value, then lower or higher index", which is what the scans produce whatever
  order the candidates are tested in.

## Design Invariants

A change against any item below is an Architecture Delta that needs the user's agreement, however small
(AGENTS.md section 3). So is changing this section.

1. **Equivalent.** Every broadphase query returns, bit for bit, the result of the linear scan of all boxes in index
   order with the stated tie rule; the cell length changes speed only. Guard: `tests/common/collision/
   StaticAabbGridTests.cpp` (200 random sets and the edge cases compared with the scans; mutants `grid-*` in
   `docs/architecture/plans/big-map/scripts/engine_grid_mutations.json`).
2. **Static.** The grid is built once and never changes; queries are const and keep no state between calls. Guard:
   the public header (no mutating member) and review.
3. **No new dependency.** `GYO::Collision` links `GYO::Math` and, privately, `GYO::Base` only. Guard: the
   configure-time check `gyo_expect_collision_links` (`tests/common/collision/CMakeLists.txt`; mutant
   `grid-links-voxel-mesh`).

### Replacement Criteria

Stop patching and let the user decide between patching and rebuilding when any of these holds:

1. Something that collides moves (doors, destructible pieces): a dynamic structure, not this grid.
2. Static collision needs a shape other than an axis-aligned box.
3. The cells are measured to be very uneven (a few cells hold most boxes) or box sizes differ widely, so queries
   slow down: a hierarchical grid (Ericson, Real-Time Collision Detection §7.2, not yet read) first, then a tree.
