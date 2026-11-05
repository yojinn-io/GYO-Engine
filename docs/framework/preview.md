# Offscreen 3D preview

Owner: the framework (`framework/`), target `GYO::ToolFrameworkPreview`, headers `gyo/framework/Preview.hpp`,
`gyo/framework/PreviewTexture.hpp` and `gyo/framework/PreviewCamera.hpp`. Plan and record:
[framework-preview plan](../architecture/plans/framework-preview/README.md), batch `v8-framework-preview-component`
(user, 2026-11-05: the 3D preview is the framework's, the second user being the voxel map editor,
`v8-tool-voxel-layers`; extracted from the model viewer after PR #146, `v8-tool-voxel-preview-framework`).

**Status (2026-11-05):** built and used by the model viewer; the voxel map editor's window
(`v8-tool-voxel-map-editor-window`) is the second user.

## What it is

A tool that shows 3D content inside its window draws it with Engine's `Renderer` on an offscreen GPU device, reads
each frame back and shows it as a texture of the window's own SDL renderer (the ImGui binding's). The framework puts
that together; what is drawn is the client's.

```text
client ──BeginFrame(width, height, camera, clear)──► OffscreenPreview ──► RenderQueue* (empty, camera set)
client ──Submit(MeshSubmission …) on the queue; meshes and textures created on Device()
client ──EndFrame()──► Renderer::Render + ReadFrame ──► TextureReadback (this frame's, Rgba8SRgb)
client ──UploadFrame(renderer, texture, frame)──► the window's texture ──ImGuiTextureId──► ImGui::Image
```

- `OffscreenPreview::Create(platform, options)`: Engine's shader bundle (`options.shaderBundle`, given by the caller),
  `SdlGpuRenderDevice::CreateOffscreen` with the bundle's complete shader formats and `options.driver`, and Engine's
  `Renderer` on it. The client creates and holds it; no shell does.
- `BeginFrame(width, height, camera, clearColor)` sets the frame size on the device (Engine keeps the request and
  remakes its target only when the size differs), resets the queue with the clear colour and sets the camera. The
  client gets the queue and submits Engine's `MeshSubmission`s to it as it would in a game: the material's shader
  (`builtin/unlit` by default), its tint and texture, `doubleSided`, the layer. Faces wind clockwise and back faces
  are culled, as Engine's renderer does everywhere.
- `EndFrame()` draws the queue and reads the frame back, waiting for the GPU. Each frame is its own: it is drawn from
  this frame's queue only and read after it is drawn. After a successful `BeginFrame` the client calls `EndFrame`
  exactly once, also when its own submission failed (it then draws what was submitted and becomes the device's last
  frame); a second `BeginFrame` before it, or an `EndFrame` without one, is a Programmer Error.
- `Device()` is the offscreen device (`SdlGpuRenderDevice`, an Engine `IRenderDevice`), where the client creates and
  releases its meshes and textures; the client releases them before the preview goes.
- `UploadFrame(renderer, texture, frame)` puts a frame into a texture of the window's renderer: a texture of another
  size (or none) is released and one of the frame's size made, then the bytes are uploaded as they were read back.
  A frame in another format than `Rgba8SRgb` is `UploadFailed`. The client calls it before the texture is drawn in a
  frame and releases the texture when it is done.
- `OrbitCamera` turns yaw, pitch (positive looks down) and a distance about a target into Engine's
  `PerspectiveCamera3D`, with near and far clips that follow the distance. A client that places its camera itself
  gives `BeginFrame` its own `PerspectiveCamera3D`.
- Errors are `PreviewError`: `StartFailed` (bundle, device or renderer), `DrawFailed` (a frame size, the drawing or
  the readback), `UploadFailed` (the window's renderer). Engine's cause is in the detail.

## Layers checks

The checks of `docs/architecture.md` ("Layers: admission and format ownership"); check 2 for each public piece.

| Piece | Engine capabilities it joins or adapts | Products |
|---|---|---|
| `OffscreenPreview` | `ShaderLibrary::AppendBundle` (through `NativeFileAssetSource` and `AssetPathResolver`), `SdlGpuRenderDevice::CreateOffscreen`, `Renderer::Initialize`; per frame `SetFrameSize`, `RenderQueue::Reset` and `SetCamera`, `Renderer::Render`, `ReadFrame`; the renderer reset before the device goes | model_viewer (animated model preview), voxel_map_editor (chunk mesh preview) |
| `UploadFrame`, `PreviewTexture` | `SdlRenderer::ReleaseTexture`, `CreateStreamingTexture`, `UploadTexture`, chosen by the frame's size | model_viewer, voxel_map_editor (both on the ImGui binding's renderer) |
| `OrbitCamera` | adapts orbit parameters to Engine's camera convention (Z, Y, X Euler angles, +Z forward) | model_viewer; voxel_map_editor when it previews by orbiting (user, 2026-11-05, D-pv-1: one user today, reviewed at the next checkup) |

None of them only passes one Engine call through. The model viewer's former `Resize`, which did, did not move: the
size comes with every frame.

## Design Invariants

A change against any item below is an Architecture Delta that needs the user's agreement, however small
(AGENTS.md section 3). So is changing this section.

1. **The client's preview.** The client creates the preview from its platform and holds it; the windowed shell and
   the ImGui binding neither create nor choose it (the framework's Replacement Criterion on render devices). Guard:
   the framework dependency guard's per-library link lists (the windowed shell links no render target, mutant
   `fw-shell-core-render`); review.
2. **Neutral.** The preview does not know what is drawn or who draws it: the shader bundle, the sizes, the driver,
   the camera and the clear colour come from the caller; it adds no flag, material or submission type of its own and
   draws whatever the client submits with Engine's `Renderer`. Guard: the framework dependency guard (no registered
   product or tool id); the tests use a synthetic square; review.
3. **Engine draws.** Projection, culling, shading and the readback are Engine's (`Renderer`, `SdlGpuRenderDevice`);
   a capability the preview lacks is added to Engine, not here (framework Design Invariant 5). Guard: the core's
   source rules (no `SDL_` symbol, no `*Native` header or target) and review.
4. **Each frame is its own.** A frame is drawn from the queue that `BeginFrame` handed out (empty then), at the size
   and camera it was given, and `EndFrame` hands out that frame's readback, which is the device's last frame.
   `UploadFrame` leaves the texture holding that frame's bytes at its size. Guard: PV2 to PV5 in
   `tests/framework/PreviewTests.cpp` and the `pv-*` mutants
   (`docs/architecture/plans/framework-preview/scripts/framework_preview_mutations.json`).
5. **Built where its Engine targets are.** `GYO::ToolFrameworkPreview` is built wherever the build has Engine's SDL
   GPU backend and SDL renderer, and only there; it reaches SDL only through them (framework Design Invariant 2).
   Guard: the configure-time check in `tests/framework/CMakeLists.txt` (mutant `fw-link-preview-missing`) and the
   link check (`fw-link-preview-native`).

### Replacement Criteria

Meeting one of these stops further repair: propose to the user whether to repair or rebuild the preview.

1. The preview has to know which product draws, or what it draws, to serve it (a branch, a mode, a mesh type).
2. A client has to create its own device or read frames back itself to get what it needs (for example a second
   view, another target format, depth), so the composition is no longer shared.
3. Serving one product needs a flag, callback or option that the other does not use.
4. A synchronous readback per frame misses a client's frame-time judgment (the model viewer's P1): propose the next
   step (an asynchronous readback, a smaller preview, drawing into the window's GPU device) instead of patching it here.

## Using it

```cpp
auto preview = Framework::OffscreenPreview::Create(platform, {bundleDirectory, 640, 480, "auto"});
// once: meshes on preview.value()->Device()
auto queue = preview.value()->BeginFrame(width, height, camera, background);
queue.value()->Submit(meshSubmission);            // as many as the client wants
auto frame = preview.value()->EndFrame();
Framework::UploadFrame(renderer, texture, frame.value());   // before ImGui draws the texture
ImGui::Image(*Framework::ImGuiTextureId(renderer, texture.handle), size);
```

A tool links `GYO::ToolFrameworkPreview` (it brings Engine's render targets with it) and lists it in its own
dependency guard; it needs the components of Engine's SDL GPU backend and SDL renderer in its `project.json`, and
Engine's builtin shaders next to its executable (`gyo_builtin_shaders`).
