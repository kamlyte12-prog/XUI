#pragma once
/// @file layout.hpp  Retained layout engine: flex Row/Column, Stack, Grid, Absolute, Scroll and Blender-style Split.
///
/// Model
///  * Nodes live in structure-of-arrays storage addressed by generational NodeId handles.
///  * One resolver serves both phases: measure(node, constraint) -> size (cached), arrange(node, w, h) -> positions.
///  * All rects are LOCAL (relative to the parent's border box). Moving a subtree therefore never re-lays its descendants.
///  * Invalidation stops at "relayout boundaries" (nodes whose width and height are both fixed in px), so a change deep
///    inside a fixed-size panel never re-measures the rest of the tree.
///  * Steady-state layout() performs no heap allocation once scratch buffers have grown to the working-set size.
///
/// Sizes are border-box, in logical pixels. INF means "unconstrained".
#include <xui/core/handle.hpp>
#include <xui/core/math.hpp>
#include <limits>
#include <vector>

namespace xui {

constexpr f32 kInf = std::numeric_limits<f32>::infinity();

enum class LayoutKind : u8 {
    Leaf,      ///< no children; intrinsic size from the measure callback (text, images, ...)
    Row,       ///< flex, main axis = x
    Column,    ///< flex, main axis = y
    Stack,     ///< children overlap, each aligned inside the content box
    Grid,      ///< fixed/auto/fr tracks (max kMaxTracks per axis)
    Split,     ///< resizable panes separated by draggable gutters
    Scroll     ///< like Column (or Row if scrollX && !scrollY) but the main axis is unbounded; has a scroll offset
};

enum class LenKind : u8 { Auto, Px, Percent };
struct Length {
    f32 v = 0; LenKind k = LenKind::Auto;
    static constexpr Length autoLen() { return {}; }
    static constexpr Length px(f32 v) { return {v, LenKind::Px}; }
    static constexpr Length pct(f32 v) { return {v, LenKind::Percent}; }
};
enum class Align : u8 { Auto, Start, Center, End, Stretch };
enum class Justify : u8 { Start, Center, End, SpaceBetween, SpaceAround, SpaceEvenly };
enum class Position : u8 { Flow, Absolute };
enum class SplitDir : u8 { Horizontal, Vertical };   ///< Horizontal: panes side by side. Vertical: panes stacked.

struct Edges { f32 l = 0, t = 0, r = 0, b = 0; static constexpr Edges all(f32 v) { return {v, v, v, v}; } };
struct GridTrack {
    enum Kind : u8 { Auto, Px, Fr } kind = Auto; f32 v = 1;
    static constexpr GridTrack px(f32 v) { return {Px, v}; }
    static constexpr GridTrack fr(f32 v = 1) { return {Fr, v}; }
    static constexpr GridTrack autoTrack() { return {Auto, 0}; }
};
constexpr int kMaxTracks = 8;

/// All layout inputs for a node. Plain data; set through LayoutTree::setStyle() so caches are invalidated.
struct LayoutStyle {
    Length width, height;
    // Percentage constraints resolve against the parent's usable content box. The legacy
    // minW/minH/maxW/maxH pixel fields remain available to low-level callers.
    Length minWidth, maxWidth, minHeight, maxHeight;
    Length basis;                             ///< flex base size on the main axis; Auto = use width/height. `grow:1` + `basis:px(0)` is CSS `flex:1` and
                                              ///< avoids measuring the child's content just to size it.
    f32 minW = 0, minH = 0, maxW = kInf, maxH = kInf;
    f32 grow = 0, shrink = 1;                 ///< flex factors (as CSS)
    Edges margin, padding, border;             ///< border inset participates in the usable content box
    Vec2 gap;                                 ///< gap.x: Row main / Grid columns.  gap.y: Column main / Grid rows.
    Align alignItems = Align::Stretch;        ///< container default for children (both axes in Grid/Stack)
    Align alignSelf = Align::Auto;            ///< per-child override
    Justify justify = Justify::Start;         ///< main-axis distribution (flex containers)
    Position position = Position::Flow;       ///< Absolute: removed from flow, placed with the insets below
    Length left, top, right, bottom;          ///< insets for Absolute children (relative to the parent's border box)
    // Grid container
    u8 colCount = 0, rowCount = 0;
    GridTrack cols[kMaxTracks], rows[kMaxTracks];
    GridTrack autoRow = GridTrack::autoTrack();   ///< track used for implicit rows
    // Grid child
    i32 gridCol = -1, gridRow = -1;           ///< -1 = auto-place
    u8 colSpan = 1, rowSpan = 1;
    // Split container / child
    SplitDir splitDir = SplitDir::Horizontal;
    f32 gutter = 4;                           ///< gutter thickness (px)
    f32 splitWeight = 1;                      ///< child: relative share of the split's main axis
    f32 splitMin = 0;                         ///< child: minimum main-axis size (px)
    // Scroll
    bool scrollX = false, scrollY = true;
};

struct Size { f32 w = 0, h = 0; };
/// Available space for measurement. When `fw`/`fh` is true the border box is EXACTLY w/h on that axis.
struct Constraint {
    f32 w = kInf, h = kInf; bool fw = false, fh = false;
    friend bool operator==(const Constraint&, const Constraint&) = default;
};
/// Intrinsic-size callback for Leaf nodes. Receives the content-box space; return the desired border-box content size.
using LeafMeasureFn = Size (*)(void* ctx, NodeId node, Constraint c);

/// State captured when a gutter drag begins. Updating from the snapshot (not incrementally) avoids float drift.
struct SplitDrag {
    NodeId split; u32 left = 0, right = 0;    ///< child indices on either side of the gutter
    f32 startA = 0, startB = 0;               ///< their pixel sizes at drag start
    bool active = false;
};

struct LayoutStats { u32 measureCalls = 0, measureCacheHits = 0, arrangeCalls = 0, nodesArranged = 0; };

class LayoutTree {
public:
    LayoutTree();
    ~LayoutTree();   // out-of-line: scratch element types are private to the .cpp
    LayoutTree(const LayoutTree&) = delete;
    LayoutTree& operator=(const LayoutTree&) = delete;

    /// Pre-size storage so that creating up to @p nodes nodes performs no further allocation.
    void reserve(u32 nodes);

    // ---- structure ---------------------------------------------------------------------------
    NodeId create(LayoutKind kind, const LayoutStyle& style = {});
    /// Destroys the node and its whole subtree; detaches it from its parent.
    void destroy(NodeId n);
    /// Append @p child as last child of @p parent (re-parents if needed).
    bool append(NodeId parent, NodeId child);
    /// Insert @p child before @p before (which must be a child of @p parent); before invalid -> append.
    bool insertBefore(NodeId parent, NodeId child, NodeId before);
    void detach(NodeId child);
    bool alive(NodeId n) const;
    NodeId parent(NodeId n) const;
    NodeId firstChild(NodeId n) const;
    NodeId nextSibling(NodeId n) const;
    u32 childCount(NodeId n) const;
    u32 nodeCount() const { return m_live; }

    // ---- inputs ------------------------------------------------------------------------------
    const LayoutStyle& style(NodeId n) const;
    LayoutKind kind(NodeId n) const;
    void setStyle(NodeId n, const LayoutStyle& s);
    void setLeafMeasure(LeafMeasureFn fn, void* ctx) { m_measure = fn; m_measureCtx = ctx; }
    /// Call when a leaf's intrinsic size changed (text edited, image loaded) without a style change.
    void markContentChanged(NodeId n);
    /// Invalidate every cache and force re-arrangement of the whole subtree (theme / DPI / font-metrics change).
    void markAllDirty(NodeId subtreeRoot);

    // ---- run ---------------------------------------------------------------------------------
    /// Lay out the tree under @p root at exactly (w,h). Only dirty subtrees are recomputed; a clean tree costs O(1).
    void layout(NodeId root, f32 w, f32 h);
    bool needsLayout(NodeId root) const;

    // ---- results -----------------------------------------------------------------------------
    Rect rect(NodeId n) const;                ///< local rect (relative to parent's border box)
    Rect absRect(NodeId n) const;             ///< in root space, including ancestors' scroll offsets
    Vec2 contentSize(NodeId n) const;         ///< extent of laid-out children (for scroll ranges)
    Vec2 scrollOffset(NodeId n) const;
    /// Set and clamp the scroll offset to [0, content - viewport]. Does not trigger layout.
    void setScrollOffset(NodeId n, Vec2 offset);

    // ---- splitters ---------------------------------------------------------------------------
    u32 gutterCount(NodeId split) const;
    /// Gutter @p index (between child index and index+1) in the split's local coordinates.
    Rect gutterRect(NodeId split, u32 index) const;
    /// Snapshot for a drag. Also normalises sibling weights to their current pixel sizes.
    SplitDrag splitDragBegin(NodeId split, u32 gutterIndex);
    /// Move the gutter by @p delta px from where the drag began (along the split's main axis), clamped by the panes' minimums.
    void splitDragUpdate(const SplitDrag& d, f32 delta);

    const LayoutStats& stats() const { return m_stats; }
    void resetStats() { m_stats = {}; }

private:
    static constexpr u32 kNone = 0xFFFFFFFFu;
    enum : u8 { F_ARR = 1, F_DESC = 2 };
    /// tainted: the result is not a pure function of the content (an unresolved percentage, or a min/max clamp applied to a
    /// content-derived size, here or in any descendant). Tainted entries are reusable only for an identical constraint.
    struct CacheEntry { Constraint c; Size s; bool valid = false, tainted = false; };
    struct Cache { CacheEntry e[2]; u8 next = 0; };
    struct FlexItem; struct GridItem;

    u32 idx(NodeId n) const;                  // kNone if stale
    NodeId idOf(u32 i) const { return NodeId{i, m_gen[i]}; }
    void unlink(u32 c);
    void link(u32 parent, u32 c, u32 before);
    void destroyRec(u32 i);
    bool isBoundary(u32 i) const;
    void invalidateContent(u32 i);
    void invalidateStyle(u32 i);
    void markArrangeOnly(u32 i);
    void clampScroll(u32 i, Vec2 o);
    u32 flowKid(u32 split, u32 ordinal) const;
    bool cacheLookup(u32 i, const Constraint& c, Size& out);
    void cacheStore(u32 i, const Constraint& c, Size s, bool tainted);

    Size measure(u32 i, const Constraint& c);
    void arrange(u32 i, f32 w, f32 h);
    void layoutChildren(u32 i, f32 w, f32 h);
    Size contentFor(u32 i, f32 cw, f32 ch, bool fw, bool fh, bool doArrange);
    Size flexResolve(u32 i, f32 cw, f32 ch, bool fw, bool fh, bool doArrange);
    Size stackResolve(u32 i, f32 cw, f32 ch, bool fw, bool fh, bool doArrange);
    Size gridResolve(u32 i, f32 cw, f32 ch, bool fw, bool fh, bool doArrange);
    Size splitResolve(u32 i, f32 cw, f32 ch, bool fw, bool fh, bool doArrange);
    void arrangeAbsolute(u32 i, f32 w, f32 h);

    // SoA node storage
    std::vector<u32> m_parent, m_first, m_last, m_next, m_prev, m_gen, m_childCount;
    std::vector<u8> m_kind, m_flags, m_alive;
    std::vector<LayoutStyle> m_style;
    std::vector<Rect> m_rect;
    std::vector<Vec2> m_content, m_scroll;
    std::vector<Cache> m_cache;
    std::vector<u32> m_free;
    u32 m_live = 0;

    // Scratch stacks (reentrant via base offsets; grow once, then reused)
    std::vector<FlexItem> m_flexItems;
    std::vector<GridItem> m_gridItems;
    std::vector<f32> m_scratchF;
    std::vector<u32> m_scratchU;

    u32 m_taint = 0;   // bumped whenever a percentage length cannot resolve (infinite basis); see CacheEntry::tainted
    LeafMeasureFn m_measure = nullptr; void* m_measureCtx = nullptr;
    LayoutStats m_stats;
};
}
