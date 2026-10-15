// Paint benchmark: walk a Blender-shaped tree (~940 nodes), emit + batch + finish a DrawList. Budget: < 1 ms for 1000 widgets.
#include "paint_tree.hpp"
#include <xui/render/soft_renderer.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
using namespace xui;
using namespace blender;
using Clock = std::chrono::steady_clock;
static double median(std::vector<double>& v) { std::sort(v.begin(), v.end()); return v[v.size() / 2]; }
int main() {
	Ui u; build(u); u.t.layout(u.root, 1920, 1080);
	const TextureHandle atlas{ 0, 1 };
	for (int mode = 0; mode < 2; ++mode) {
		Painter p(Painter::Options{ mode == 0, true }); PaintCtx c{ &p, &u.t, atlas };
		for (int i = 0; i < 20; ++i) { p.begin(1920, 1080, 1); paintNode(c, u.root, { 0, 0 }, 0); (void)p.finish(); }   // warm buffers
		std::vector<double> all, walk, fin; DrawList dl;
		for (int i = 0; i < 2000; ++i) {
			c.widgets = 0;
			const auto t0 = Clock::now();
			p.begin(1920, 1080, 1); paintNode(c, u.root, { 0, 0 }, 0);
			const auto t1 = Clock::now();
			dl = p.finish();
			const auto t2 = Clock::now();
			walk.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
			fin.push_back(std::chrono::duration<double, std::micro>(t2 - t1).count());
			all.push_back(std::chrono::duration<double, std::micro>(t2 - t0).count());
		}
		const auto& s = p.stats();
		std::printf("%s\n", mode == 0 ? "== reordering batcher (default) ==" : "== naive order (merge only with the previous batch) ==");
		std::printf("  %u widgets, %u prims (%u culled) -> %u draw calls, %u vertices, %u indices\n", c.widgets, s.prims, s.culled, s.drawCalls, s.vertices, s.indices);
		std::printf("  paint + batch + finish: %7.1f us   [budget 1000 us]   (emit %.1f us + finish %.1f us)\n", median(all), median(walk), median(fin));
		std::printf("  draw data: %.1f KB vertices + %.1f KB indices + %.2f KB commands\n", dl.nVerts * sizeof(Vertex) / 1024.0, dl.nIdx * 4 / 1024.0, dl.nCmds * sizeof(DrawCmd) / 1024.0);
	}
	Painter painter; PaintCtx ctx{ &painter, &u.t, atlas };
	painter.begin(1920, 1080, 1); paintNode(ctx, u.root, { 0, 0 }, 0);
	const DrawList frame = painter.finish();
	SoftRenderer software;
	software.init({ 640, 360, 1.f, nullptr, false });
	std::vector<u8> bgra;
	std::vector<double> raster, exportPixels;
	for (int i = 0; i < 5; ++i) {
		software.beginFrame({ 640, 360, 1.f, Color{0, 0, 0, 1}, u64(i) });
		auto a = Clock::now(); software.submit(frame); auto b = Clock::now();
		software.snapshotBGRA(bgra); auto c = Clock::now();
		raster.push_back(std::chrono::duration<double, std::micro>(b - a).count());
		exportPixels.push_back(std::chrono::duration<double, std::micro>(c - b).count());
	}
	std::printf("== reference software backend, 640x360 ==\n");
	std::printf("  rasterize DrawList: %7.1f us; reusable BGRA export: %7.1f us\n", median(raster), median(exportPixels));
	Painter dirtyPainter; PaintCtx dirtyCtx{ &dirtyPainter, &u.t, atlas };
	dirtyPainter.begin(1920, 1080, 1); dirtyPainter.pushClip({ 100, 80, 100, 50 });
	paintNode(dirtyCtx, u.root, { 0, 0 }, 0); dirtyPainter.popClip();
	const DrawList dirtyFrame = dirtyPainter.finish();
	const Recti dirty{ 100, 80, 100, 50 };
	std::vector<double> dirtyRaster, dirtyExport;
	for (int i = 0; i < 20; ++i) {
		FrameInfo partial{ 640, 360, 1.f, Color{0, 0, 0, 1}, u64(i) };
		partial.damage = dirty; partial.partialUpdate = true;
		software.beginFrame(partial);
		auto a = Clock::now(); software.submit(dirtyFrame); auto b = Clock::now();
		software.snapshotBGRA(bgra, dirty); auto c = Clock::now();
		dirtyRaster.push_back(std::chrono::duration<double, std::micro>(b - a).count());
		dirtyExport.push_back(std::chrono::duration<double, std::micro>(c - b).count());
	}
	std::printf("  100x50 partial update: %7.1f us raster + %7.1f us BGRA export\n", median(dirtyRaster), median(dirtyExport));
	std::printf("  note: reference CPU path only; GPU backends should consume DrawList directly\n");
	software.shutdown();
	std::printf("sizeof(Vertex)=%zu sizeof(DrawCmd)=%zu\n", sizeof(Vertex), sizeof(DrawCmd));
}
