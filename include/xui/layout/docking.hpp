#pragma once

#include <xui/layout/layout.hpp>
#include <span>
#include <vector>

namespace xui {

	enum class DockZone : u8 { Center, Left, Right, Top, Bottom, Floating };

	struct DockPanelLayout {
		u32 panel = 0;
		Rect bounds{};
		u32 group = 0xFFFFFFFFu;
		u32 order = 0;
		bool active = false;
		bool floating = false;
		u32 zOrder = 0;
	};

	struct DockPreview {
		Rect bounds{};
		u32 targetPanel = 0xFFFFFFFFu;
		DockZone zone = DockZone::Center;
		bool valid = false;
	};

	struct DockSplitterLayout {
		u32 node = 0xFFFFFFFFu;
		Rect bounds{};
		bool horizontal = true;
	};

	/// A recursive split/group tree. Panel keys are application-owned stable indices.
	/// Dock operations may grow storage, but layout, preview updates and split dragging do not.
	class DockTree {
	public:
		DockTree() = default;
		DockTree(const DockTree&) = delete;
		DockTree& operator=(const DockTree&) = delete;
		DockTree(DockTree&&) noexcept = default;
		DockTree& operator=(DockTree&&) noexcept = default;

		void reserve(u32 panels, u32 nodes);
		bool addPanel(u32 panel, Vec2 minimum, Vec2 preferred, Rect floatingBounds = {},
			Vec2 maximum = { kInf, kInf });
		bool setPanelConstraints(u32 panel, Vec2 minimum, Vec2 preferred, Vec2 maximum);
		bool dockRoot(u32 panel);
		bool dock(u32 panel, u32 targetPanel, DockZone zone);
		bool undock(u32 panel, Rect floatingBounds);
		bool moveFloating(u32 panel, Rect floatingBounds);
		bool raiseFloating(u32 panel);
		bool close(u32 panel);
		bool activate(u32 panel);
		bool reorder(u32 panel, u32 order);
		bool resizeSplit(u32 splitNode, f32 ratio);
		void layout(Rect bounds, f32 gutter);
		DockPreview preview(u32 targetPanel, DockZone zone) const;

		bool contains(u32 panel) const;
		bool floating(u32 panel) const;
		DockZone zone(u32 panel) const;
		Rect panelBounds(u32 panel) const;
		Rect groupBounds(u32 panel) const;
		u32 groupOf(u32 panel) const;
		u32 activePanel(u32 panel) const;
		u32 panelOrder(u32 panel) const;
		u32 panelCount(u32 panel) const;
		u32 panelAt(Vec2 point, u32 excludePanel = 0xFFFFFFFFu) const;
		u32 splitAt(Vec2 point, bool* horizontal = nullptr) const;
		Rect splitBounds(u32 splitNode) const;
		f32 splitRatio(u32 splitNode) const;
		u32 splitterCount() const;
		DockSplitterLayout splitterAt(u32 ordinal) const;
		u32 root() const { return m_root; }
		std::span<const DockPanelLayout> panels() const { return m_panels; }

	private:
		static constexpr u32 kNone = 0xFFFFFFFFu;
		enum class Kind : u8 { Dead, Group, Split };
		struct TreeNode {
			Kind kind = Kind::Dead;
			u32 parent = kNone, first = kNone, second = kNone, active = 0;
			std::vector<u32> panels;
			Rect bounds{};
			f32 ratio = 0.5f;
			bool horizontal = true;
		};
		struct Panel {
			u32 key = 0;
			u32 group = kNone;
			u32 active = 0;
			Vec2 minimum{};
			Vec2 preferred{};
			Vec2 maximum{ kInf, kInf };
			Rect bounds{};
			bool floating = true;
			bool closed = false;
			u32 zOrder = 0;
		};

		u32 findPanel(u32 key) const;
		u32 makeGroup(u32 panel);
		u32 makeSplit(u32 first, u32 second, bool horizontal, f32 ratio);
		void detachPanel(u32 panelIndex);
		void removeEmptyGroup(u32 group);
		u32 groupForPanel(u32 panel) const;
		void layoutNode(u32 node, Rect bounds, f32 gutter);
		Vec2 minimum(u32 node) const;
		Vec2 maximum(u32 node) const;
		u32 activePanelInGroup(u32 group) const;
		Rect m_host{};
		f32 m_gutter = 5.f;
		u32 m_root = kNone;
		u32 m_nextFloatZ = 1;
		std::vector<TreeNode> m_nodes;
		std::vector<Panel> m_panelData;
		std::vector<DockPanelLayout> m_panels;
	};

} // namespace xui
