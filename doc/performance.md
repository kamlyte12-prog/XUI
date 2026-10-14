# Performance and best practices

Xui is retained: it keeps a layout tree and caches measurements, resolves style sets at node creation/state changes, and redraws after events or requested ticks. Performance depends strongly on the renderer. The interactive examples use direct OpenGL draw-list rendering; `SoftRenderer` is a correctness/reference path.

## What the measurements mean

Recorded Windows Release measurements are host-specific:

- The latest hidden, uncapped facade profiles for the tabbed showcase reported 981 FPS (`xui_glfw_demo`, 4,906 frames in 5.00019 s) and 697 FPS (`xui_hello`, 4,804 frames in 6.89123 s), with the FPS widget enabled and vsync disabled. Both processes exited normally. This matches the prior observation that bounded-profile timing varies between runs; the five-second bound is not a reliable duration guarantee until the occasional overrun is explained.
- Earlier synthetic benchmarks measured a 938-node layout resize near 95 microseconds and splitter drag layout near 87 microseconds, with unchanged same-size layout doing no work.
- An earlier 640×360 software raster benchmark measured about 28 milliseconds for a full frame before export, below 60 FPS. Direct OpenGL drawing avoids the CPU full-frame raster loop for the bundled windows.

These results verify one local Windows Release configuration. They do not guarantee a particular FPS on other GPUs, drivers, display sizes, operating systems, or under vsync. Uncapped hidden frame-loop FPS is a throughput ceiling, not a displayed monitor refresh rate or interaction-latency guarantee. This run did not isolate layout, event, text, or GPU timings; visible rendering and resize/interaction latency still need an interactive smoke test.

Run the examples with `--profile` to measure their actual shared assets and loop. Use dedicated benchmarks for stage-level profiling; the simple sample profile reports frame count, elapsed seconds, and effective FPS.

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
- Enable the FPS counter only while needed; in idle event-driven mode its display refreshes every 250 ms (up to four overlay redraws per second), and platform input/resize events still wake the loop and redraw promptly. Continuous profile mode does not wait on the FPS timer, so reported loop throughput is not capped by the widget.
- Keep glyph atlases reusable and load fonts before `attach()` so atlas setup and glyph upload happen through the normal renderer lifecycle.
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
