# GYO Math

`GYO::Math` (`engine/math`, namespace `Engine::Math`) is the engine-wide math foundation: value types, pure geometry and the coordinate conventions every module shares. It is header-only, depends only on the C++ standard library and is the lowest engine layer; it links nothing and knows no module, product or tool.

All active code that used the removed vector, matrix and geometry types (Collision, Model, Render, Ui, object_fps_pvp, the UI editor) now uses Math; inactive products still use the removed names and must migrate before re-enabling ([migration list](plans/math-foundation/inactive_products.md)). History and evidence: [math-foundation plan](plans/math-foundation/README.md).

## Boundary

| Belongs in Math | Stays with its owner |
|---|---|
| Vectors, matrices, quaternions and their operations | GPU ABI structs (`ShaderAbi::*`, `Vertex3D`): Render |
| Geometric primitives that describe "what a shape is" (`Ray`, `Plane`, `Sphere`, `Aabb`, `Capsule`, `Segment`, `Triangle`, `Rect`) | Domain shapes specialised for a purpose, e.g. `VerticalCapsule`: Collision |
| Pure geometric queries: closest points, containment, overlap, ray intersection without tolerance | Collision detection, contact, penetration, sweeps, skin widths and tolerances: Collision. Its queries take Math primitives (`RaycastAabb(Ray, ...)`, `SweepSphereAgainstCapsule(Segment, ...)`); Collision raycasts return world-space distances (direction normalized internally), unlike the `|direction|`-scaled `t` of `Math::Intersect` |
| Coordinate, matrix, Euler and clip-space conventions | Semantic transforms (`Transform3D`, `Model::Transform`), cameras, colours as data, wire and file formats |
| Scalar utilities, constants, angle and sRGB transfer functions | Gameplay rules and product-specific coordinates (for example ground-plane points) |

A function belongs in Math when it remains meaningful without any domain word: a closest point on a segment is geometry; a capsule sweep with an initial-overlap policy is collision.

## Layout

```text
engine/math/include/engine/math/
  linear/    Vec2 Vec3 Vec4 Vec3d VecInt(Vec2i Vec3i) Matrix3 Matrix4 Quaternion
  geometry/  Rect Segment Aabb Ray Plane Sphere Capsule Triangle Intersection
  scalar/    Scalar Constants Angle ColorSpace
```

Include the header of each type used; there is no umbrella header. Engine public headers must not write `using namespace Engine::Math`.

## Types

All types are `final` aggregates without constructors, trivially copyable and standard-layout; sizes are locked by `static_assert`.

- **Vectors**: `Vec2`, `Vec3`, `Vec4` (float), `Vec3d` (double), `Vec2i`, `Vec3i` (int32). There is one vector type per dimension for points, directions and sizes; `TransformPoint` and `TransformVector` express the difference. Float vectors have no `==`; integer vectors do.
- **Matrices**: `Matrix3`, `Matrix4`, column-major (`values[column * N + row]`), column vectors, default identity.
- **Quaternion**: `{x, y, z, w}`, default identity.
- **Geometry**: `Rect {x, y, width, height}`, `Segment {start, end}`, `Aabb {minimum, maximum}`, `Ray {origin, direction}`, `Plane {normal, distance}`, `Sphere {center, radius}`, `Capsule {segmentStart, segmentEnd, radius}`, `Triangle {a, b, c}`. `Segmentd` and `Aabbd` are double-precision variants for algorithms that work in `Vec3d`.

## Conventions

- **Space**: left-handed, +X right, +Y up, +Z forward. Angles are radians. Model and world space use metres; Math itself is unitless.
- **Matrices**: `p' = M * p`. `Multiply(a, b)` applies `b` first, so `Multiply(parent, local)` places a local transform under its parent. Translation is `values[12..14]`. `Matrix4{}` is the identity; `Zero()` (and `Matrix4{{}}`) is all zeros.
- **Rotation direction**: `MakeRotationY(a) * (0,0,1) = (sin a, 0, cos a)` (positive yaw turns +Z toward +X); `MakeRotationX(a) * (0,0,1) = (0, -sin a, cos a)` (positive pitch looks down); `MakeRotationZ(a) * (1,0,0) = (cos a, sin a, 0)`.
- **Euler**: `ComposeEulerXYZ(translation, rotation, scale)` applies scale, then X, Y, Z rotation, then translation.
- **Quaternions**: `Multiply(a, b)` applies `b` first; `Rotate(q, v)` equals `MakeRotation(q)` applied to `v`.
- **Clip space**: `MakePerspective` is left-handed with depth 0 at near and 1 at far, clip w = view z. `MakeOrthographicPixels` maps top-left-origin, +Y-down pixels to clip space.
- **GPU**: shaders declare `row_major float4x4` and compute `mul(float4(p, 1), M)`. Those 16 floats are the Math `values` in order, so uploads copy without a transpose.
- **Ray**: `direction` need not be normalized; ray parameters `t` are in units of `|direction|`.
- **Plane**: `normal` is unit length and points are those with `dot(normal, p) == distance`; `SignedDistance` is positive on the normal side. `MakePlane` normalizes.
- **Triangle**: vertex order defines `Normal = Cross(b - a, c - a)`. Math defines no front face; facing and culling are render pipeline state.
- **Rect**: queries assume a non-negative size; callers that can produce negative sizes (for example UI layout) handle them first. The axis direction (for example y-down pixels) belongs to the caller's space.
- **Containment and overlap** are closed: touching counts.

## Numeric policy

- Each operation has exactly one implementation in GYO. Recorded exceptions (semantic types, domain tolerances, -0-preserving authority code) are listed in the [math-foundation plan](plans/math-foundation/PLAN.md) (B7 audit). Changing an expression's shape (operand order, association, splitting or merging expressions) can change float results. Treat such edits as behaviour changes and re-run the characterization tests.
- GYO compiles its own code with floating-point contraction off: `-ffp-contract=off` for Clang, AppleClang and GCC (set in `build/cmake/GyoBuild.cmake`), and MSVC's default `/fp:precise`, which does not contract. No multiply and add is fused into FMA, so the supported x64 platforms (Linux, Windows, macOS) round identically apart from libm; arm64 matches as a by-product. Code compiled outside that setting (contraction on) may fuse at run time or when an optimizer constant-folds, and can then differ from GYO builds.
- Comparisons that must be bit-exact (characterization, digests) still feed both sides run-time data, so an accidental contraction setting shows up as a difference rather than being hidden by constant folding.
- `DegreesToRadians` and `RadiansToDegrees` are `constexpr` (one multiplication each, so constant evaluation gives the run-time bits) and may initialise constants. Operations with several roundings stay plain `inline`.
- Matrix product association matters: `World * (View * Proj)` in the old row-vector code equals `Multiply(Multiply(Proj, View), World)`, not `Multiply(Proj, Multiply(View, World))`.
- Math has no global epsilon. Functions that need a tolerance take none and document exact behaviour (`== 0` checks, closed intervals); tolerances belong to the caller's domain.
- `Normalize` requires a non-zero input (zero yields NaN). Use `NormalizeOrZero` (vectors) or `NormalizeOrIdentity` (quaternions, length <= 1e-12) when degenerate input is expected.
- Matrix `Inverse` returns `std::nullopt` when the determinant is exactly zero or the result is not finite. Quaternion `Inverse` divides by the squared length and requires a non-zero quaternion; use `Conjugate` for unit quaternions.
- Ray `Intersect` returns `std::nullopt` for a non-finite ray or when the computation overflows to NaN. Ray-sphere uses Lagrange's identity for the discriminant, so distant origins do not cancel catastrophically.
- `ClosestPoint(point, Triangle)` treats a triangle with `|ab x ac|^2 <= FLT_EPSILON * |ab|^2 * |ac|^2` as a segment (closest of the three edge points). This is a conditioning guard inside the function, not a caller tolerance; its error is about `sqrt(FLT_EPSILON)` times the edge length. Coordinates must stay below about 1e9.
- Documented edge behaviour: sRGB functions pass NaN through; `NormalizeOrIdentity` returns a zero quaternion when the length overflows; `WrapRadians` of an infinity is NaN.
- `Length` is `sqrt` of the sum of squares (not `hypot`); it overflows for components above about 1.8e19.
- Results involving `sin`, `cos`, `tan`, `pow`, `acos` depend on the platform's libm; Math does not make them bit-identical across platforms.

## Tests

`tests/common/math` builds `gyo_math_tests`: specification tests derived from this contract (with a double-precision reference for triangle queries), and characterization tests that freeze the engine helpers Math replaces and compare them bit by bit or record measured drift. Product helpers are characterized in their product's tests, not here, because common tests must not depend on a product.
