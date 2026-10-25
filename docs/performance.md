# Performance and best practices

Xui is retained: it keeps a layout tree and caches measurements, resolves style sets at node creation/state changes, and redraws after events or requested ticks. Performance depends strongly on the renderer. The interactive examples use direct OpenGL draw-list rendering; `SoftRenderer` is a correctness/reference path.

## What measurements mean

Recorded benchmark values and their build conditions are kept in the canonical [project tracking history](../plan/tracking.md), so measurement snapshots have one home. The current hidden Windows Release loop profile exceeds 60 FPS, and the docking runtime microbenchmark reports pointer-path cost for a small synthetic tree. Neither establishes visible interaction or resize performance.

No benchmark guarantees a particular FPS on other GPUs, drivers, display sizes, operating systems, or under vsync. Uncapped hidden frame-loop FPS is a throughput ceiling, not a displayed monitor refresh rate or an interaction-latency guarantee. Visible scrolling, clipping, resize smoothness, and ordinary close still need an interactive smoke test.

Run the examples with `--profile` to measure their shared assets and render loop. Feature profiles `--profile-animation`, `--profile-glow`, `--profile-transform`, `--profile-docking`, and `--profile-menubar` select representative showcase states in a visible window and run uncapped. `--profile-animation-timer` uses the ordinary event/timer loop and selects the animated page. The latest Windows Release run measured 61.09 FPS over five seconds after fixing a due-timer wait bug; treat this as a machine-specific sample, not a cross-device guarantee. Uncapped profiles report loop throughput, not normal display refresh or input latency. Use `bench_docking` for headless DockTree/UiRuntime drag-preview and splitter-update timings. It measures runtime calls only, not event pumping, painting, GPU submission, or display presentation. Use dedicated benchmarks for stage-level profiling; the simple sample profile reports frame count, elapsed seconds, and effective FPS.

The standalone Release benchmarks exercise more controlled stages. `bench_style`, `bench_xui`, `bench_layout`, and `bench_paint` print their own reference budgets. `bench_docking_scale` samples idle layout, drag preview, splitter updates, and paint for wide and deep trees from 8 to 32 panels. Its DockHost padding is explicitly zero so geometry probes stay independent of the Fluent visual baseline. Results and the measured machine/build are recorded in [`tracking.md`](../plan/tracking.md).

## Keep layout inexpensive

- Build the tree once; update retained values/state rather than reparsing `.xui` every frame.
- Avoid changing parent dimensions every frame when only one leaf changed. Use fixed-size panels to create relayout boundaries where practical.
- Use `grow` and `shrink` deliberately; avoid oversized trees of nested containers for simple spacing.
- Prefer `Split` for user-adjustable pane proportions rather than recomputing child dimensions in application code.
- Use `.xui grow: 1` on content pages and flexible panes when they should occupy leftover space. Percentage lengths resolve once against a definite parent size; include a regression check when changing width constraints.
- Supply width constraints for long `Text` nodes so wrapping measurement can settle. Avoid changing text content every frame.
- Use `.xui align` for structural child alignment and `.xss` for reusable visual styles.

## Keep frames fast

- Use a GPU renderer for interactive full-window redraws. The CPU `SoftRenderer` is deterministic but full-frame software rasterization is much slower.
- Let the facade's event loop sleep when idle. Continuous redraw is intended for animation/profile use, not normal static UI.
- Enabling FPS with F3 opts into continuous redraw so the displayed FPS measures rendered frame cadence. This intentionally raises CPU/GPU use while visible; turn it off with F3 for event-driven idle behavior. F4 adds the previous frame's draw-batch count without enabling continuous redraw.
- Keep glyph atlases reusable and load fonts before `attach()` so atlas setup and glyph upload happen through the normal renderer lifecycle.
- HarfBuzz shapes text only when the text-layout cache misses; subsequent paints reuse the cached advances/positions and atlas glyphs. Each variable weight has a distinct font/layout/atlas identity, so the first use of a new weight incurs its own shaping and glyph-raster work.
- Avoid very large shadows or thousands of separately styled objects. Painter batches compatible draw commands, but each visible object still contributes geometry and style/layout work.
- Do not force `vsync = false` in shipped applications unless uncapped throughput is intentional. Vsync can cap frame rate at display refresh and reduce tearing.

## Diagnose the slow stage

Measure separately before changing architecture:

1. Record renderer and platform, build mode, resolution, DPI, vsync, and visible/hidden state.
2. Compare static idle, pointer movement, button activation, and continuous resize.
3. Check whether frame count is low because the loop is waiting/vsynced, or because one layout/paint/submit operation is expensive.
4. Compare GPU renderer with `SoftRenderer` at a modest test size. A large gap points to software raster/export work.
5. Profile layout separately; same-size layout should be cached and a splitter move should only invalidate affected geometry.

For reproducible results, note whether the process ran alongside other GPU-heavy apps. Concurrent profiles can distort elapsed time and throughput.

## Text-path profiling

Build and run `bench_text` to measure the CPU text path without GPU submission:

```powershell
cmake --build build --target bench_text --config Release
& .\build\benchmarks\Release\bench_text.exe
```

It runs four workloads: a cached short label, 400 distinct textbox edits, a cached long wrapped paragraph, and 300 wrap widths repeated twice. It reports elapsed microseconds per operation, layout-cache requests/hits/misses/rehashes, HarfBuzz shaping runs and glyph counts, time spent inside shaping, glyph rasterizations and texture uploads, atlas rebuilds, and Painter batches for the last operation. The first pass of the width sweep is deliberately cold; the second should be cache hits. This benchmark uses `NullRenderer`, so it does not measure OpenGL submission, GPU time, presentation, actual widget event handling, or visible frame rate.

`TextSystem::frameStats()` and `GlyphAtlas::frameStats()` reset at `beginFrame()`. The facade starts the text frame before layout so its counts include intrinsic measurement and paint-time requests for that UI frame. Shaping time is accumulated only on layout cache misses; atlas rasterization/upload counts are zero after glyphs are warm. Consult [`plan/tracking.md`](../plan/tracking.md) for the latest measured run and machine/build details.

## Live resize and the GLFW event drain

The GLFW platform drains callbacks queued by its last `glfwPollEvents()` call before polling again. Polling repeatedly inside one drain loop can starve frame submission while the OS produces a continuous stream. `GlfwPlatform::pumpEvents()` polls once per drain cycle. Some operating systems also enter a modal native resize loop, so the ordinary `Xui::run()` loop cannot run during the drag. To handle that case, the platform contract offers an optional `setLiveResizeHook`; the GLFW size callback invokes the facade's render-frame path, which resizes the backbuffer, recalculates layout, paints, and presents at each callback. Other backends can leave the hook as a no-op if their event loop remains active during resizing.

Raster images are decoded once and retained as texture data. SVG source is parsed once and rasterized only when its physical display bounds or DPI cross a 64-pixel size bucket; the resulting RGBA texture is then reused. Very large images are rejected at 16,384 pixels per side or 64 million pixels total to bound allocation and upload costs. Applications that resize SVG-heavy surfaces continuously should expect a reraster only when a bucket changes.

`Xui::run()` reads the latest logical client size and framebuffer pixel size every iteration. On a resize/DPI event it resizes the renderer in physical pixels, then runs retained layout with logical client dimensions before painting. During a modal resize, the GLFW live-resize hook runs the same frame path from the size callback. `ScrollView` clips descendants through the backend-neutral painter clip stack. The source path and automated suite were verified; a successful manual drag-resize could not be completed in the desktop helper session, so interactive resize remains unverified here.

Both example targets use the GLFW/OpenGL backend in this checkout. A visible resize/input pass and non-Windows backend verification still require an interactive environment; see the tracking history for the last recorded profile data and exact conditions.
