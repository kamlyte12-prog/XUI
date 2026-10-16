#include <xui/layout/docking.hpp>
#include <algorithm>
#include <cmath>

namespace xui {

    void DockTree::reserve(u32 panels, u32 nodes) {
        m_panelData.reserve(panels);
        m_panels.reserve(panels);
        m_nodes.reserve(nodes);
    }

    u32 DockTree::findPanel(u32 key) const {
        for (u32 i = 0; i < m_panelData.size(); ++i)
            if (m_panelData[i].key == key && !m_panelData[i].closed) return i;
        return kNone;
    }

    bool DockTree::contains(u32 key) const { return findPanel(key) != kNone; }

    u32 DockTree::makeGroup(u32 panel) {
        TreeNode group;
        group.kind = Kind::Group;
        group.panels.push_back(panel);
        const u32 index = static_cast<u32>(m_nodes.size());
        m_nodes.push_back(std::move(group));
        m_panelData[panel].group = index;
        m_panelData[panel].floating = false;
        if (m_root == kNone) m_root = index;
        return index;
    }

    u32 DockTree::makeSplit(u32 first, u32 second, bool horizontal, f32 ratio) {
        TreeNode split;
        split.kind = Kind::Split;
        split.first = first;
        split.second = second;
        split.horizontal = horizontal;
        split.ratio = std::clamp(ratio, 0.05f, 0.95f);
        const u32 index = static_cast<u32>(m_nodes.size());
        m_nodes.push_back(std::move(split));
        m_nodes[first].parent = index;
        m_nodes[second].parent = index;
        return index;
    }

    bool DockTree::addPanel(u32 key, Vec2 min, Vec2 preferred, Rect floatingBounds, Vec2 max) {
        if (findPanel(key) != kNone) return false;
        Panel panel;
        panel.key = key;
        panel.minimum = { std::max(0.f, min.x), std::max(0.f, min.y) };
        panel.maximum = { std::max(panel.minimum.x, max.x), std::max(panel.minimum.y, max.y) };
        panel.preferred = { std::max(panel.minimum.x, preferred.x), std::max(panel.minimum.y, preferred.y) };
        panel.bounds = floatingBounds.empty() ? Rect{ 24.f, 24.f, panel.preferred.x, panel.preferred.y } : floatingBounds;
        const u32 index = static_cast<u32>(m_panelData.size());
        m_panelData.push_back(panel);
        m_panels.push_back({ key, panel.bounds, kNone, 0, false, true });
        if (m_root == kNone) makeGroup(index);
        return true;
    }

    bool DockTree::dockRoot(u32 key) {
        const u32 panel = findPanel(key);
        if (panel == kNone) return false;
        if (!m_panelData[panel].floating) return true;
        detachPanel(panel);
        makeGroup(panel);
        m_root = m_panelData[panel].group;
        return true;
    }

    bool DockTree::setPanelConstraints(u32 key, Vec2 min, Vec2 preferred, Vec2 max) {
        const u32 index = findPanel(key);
        if (index == kNone) return false;
        Panel& panel = m_panelData[index];
        min = { std::max(0.f, min.x), std::max(0.f, min.y) };
        max = { std::max(min.x, max.x), std::max(min.y, max.y) };
        preferred = { std::clamp(preferred.x, min.x, max.x), std::clamp(preferred.y, min.y, max.y) };
        if (panel.minimum == min && panel.maximum == max && panel.preferred == preferred) return false;
        panel.minimum = min;
        panel.maximum = max;
        panel.preferred = preferred;
        return true;
    }

    void DockTree::removeEmptyGroup(u32 group) {
        if (group >= m_nodes.size() || m_nodes[group].kind != Kind::Group || !m_nodes[group].panels.empty()) return;
        const u32 parent = m_nodes[group].parent;
        if (parent == kNone) {
            m_root = kNone;
            m_nodes[group].kind = Kind::Dead;
            return;
        }
        if (parent >= m_nodes.size() || m_nodes[parent].kind != Kind::Split) return;
        const u32 sibling = m_nodes[parent].first == group ? m_nodes[parent].second : m_nodes[parent].first;
        const u32 grand = m_nodes[parent].parent;
        m_nodes[sibling].parent = grand;
        if (grand == kNone) m_root = sibling;
        else if (m_nodes[grand].first == parent) m_nodes[grand].first = sibling;
        else m_nodes[grand].second = sibling;
        m_nodes[parent].kind = Kind::Dead;
        m_nodes[parent].first = m_nodes[parent].second = kNone;
        m_nodes[group].kind = Kind::Dead;
        m_nodes[group].parent = kNone;
    }

    void DockTree::detachPanel(u32 panel) {
        Panel& entry = m_panelData[panel];
        if (entry.floating || entry.group >= m_nodes.size()) { entry.group = kNone; entry.floating = true; return; }
        const u32 group = entry.group;
        TreeNode& groupNode = m_nodes[group];
        const auto it = std::find(groupNode.panels.begin(), groupNode.panels.end(), panel);
        if (it != groupNode.panels.end()) {
            const u32 ordinal = static_cast<u32>(it - groupNode.panels.begin());
            groupNode.panels.erase(it);
            if (groupNode.panels.empty()) groupNode.active = 0;
            else if (groupNode.active > ordinal) --groupNode.active;
            else if (groupNode.active >= groupNode.panels.size()) groupNode.active = static_cast<u32>(groupNode.panels.size() - 1);
        }
        entry.group = kNone;
        entry.floating = true;
        removeEmptyGroup(group);
    }

    u32 DockTree::groupForPanel(u32 key) const {
        const u32 index = findPanel(key);
        if (index == kNone || m_panelData[index].floating) return kNone;
        return m_panelData[index].group;
    }

    bool DockTree::dock(u32 key, u32 targetKey, DockZone dropZone) {
        const u32 panel = findPanel(key), target = findPanel(targetKey);
        if (panel == kNone || target == kNone || key == targetKey || dropZone == DockZone::Floating) return false;
        const u32 targetGroup = groupForPanel(targetKey);
        if (targetGroup == kNone || targetGroup >= m_nodes.size()) return false;
        if (dropZone == DockZone::Center && m_panelData[panel].group == targetGroup && !m_panelData[panel].floating) {
            TreeNode& group = m_nodes[targetGroup];
            const auto it = std::find(group.panels.begin(), group.panels.end(), panel);
            if (it == group.panels.end()) return false;
            group.panels.erase(it);
            group.panels.push_back(panel);
            group.active = static_cast<u32>(group.panels.size() - 1);
            return true;
        }

        detachPanel(panel);
        if (dropZone == DockZone::Center) {
            TreeNode& group = m_nodes[targetGroup];
            group.panels.push_back(panel);
            group.active = static_cast<u32>(group.panels.size() - 1);
            m_panelData[panel].group = targetGroup;
            m_panelData[panel].floating = false;
            return true;
        }

        const u32 newGroup = makeGroup(panel);
        const u32 oldParent = m_nodes[targetGroup].parent;
        const bool horizontal = dropZone == DockZone::Left || dropZone == DockZone::Right;
        const bool before = dropZone == DockZone::Left || dropZone == DockZone::Top;
        const u32 split = makeSplit(before ? newGroup : targetGroup, before ? targetGroup : newGroup, horizontal, before ? 0.32f : 0.68f);
        m_nodes[split].parent = oldParent;
        if (oldParent == kNone) m_root = split;
        else if (m_nodes[oldParent].first == targetGroup) m_nodes[oldParent].first = split;
        else m_nodes[oldParent].second = split;
        m_nodes[targetGroup].parent = split;
        return true;
    }

    bool DockTree::undock(u32 key, Rect floatingBounds) {
        const u32 panel = findPanel(key);
        if (panel == kNone || m_panelData[panel].floating) return false;
        detachPanel(panel);
        Panel& entry = m_panelData[panel];
        entry.bounds = floatingBounds.empty() ? Rect{ m_host.x + 24.f, m_host.y + 24.f, entry.preferred.x, entry.preferred.y } : floatingBounds;
        m_panels[panel].bounds = entry.bounds;
        m_panels[panel].floating = true;
        return true;
    }

    bool DockTree::moveFloating(u32 key, Rect floatingBounds) {
        const u32 panel = findPanel(key);
        if (panel == kNone || !m_panelData[panel].floating || floatingBounds.empty()) return false;
        m_panelData[panel].bounds = floatingBounds;
        m_panels[panel].bounds = floatingBounds;
        return true;
    }

    bool DockTree::close(u32 key) {
        const u32 panel = findPanel(key);
        if (panel == kNone) return false;
        if (!m_panelData[panel].floating) detachPanel(panel);
        m_panelData[panel].closed = true;
        m_panels[panel].floating = false;
        m_panels[panel].active = false;
        m_panels[panel].bounds = {};
        return true;
    }

    bool DockTree::activate(u32 key) {
        const u32 panel = findPanel(key);
        if (panel == kNone || m_panelData[panel].floating) return false;
        TreeNode& group = m_nodes[m_panelData[panel].group];
        const auto it = std::find(group.panels.begin(), group.panels.end(), panel);
        if (it == group.panels.end()) return false;
        const u32 active = static_cast<u32>(it - group.panels.begin());
        if (active == group.active) return false;
        group.active = active;
        return true;
    }

    bool DockTree::reorder(u32 key, u32 order) {
        const u32 panel = findPanel(key);
        if (panel == kNone || m_panelData[panel].floating) return false;
        TreeNode& group = m_nodes[m_panelData[panel].group];
        const auto it = std::find(group.panels.begin(), group.panels.end(), panel);
        if (it == group.panels.end() || group.panels.empty()) return false;
        const u32 previous = static_cast<u32>(it - group.panels.begin());
        const u32 next = std::min<u32>(order, static_cast<u32>(group.panels.size() - 1));
        if (previous == next) return false;
        group.panels.erase(it);
        group.panels.insert(group.panels.begin() + next, panel);
        group.active = next;
        return true;
    }

    bool DockTree::resizeSplit(u32 splitNode, f32 ratio) {
        if (splitNode >= m_nodes.size() || m_nodes[splitNode].kind != Kind::Split || !std::isfinite(ratio)) return false;
        const f32 next = std::clamp(ratio, 0.01f, 0.99f);
        if (m_nodes[splitNode].ratio == next) return false;
        m_nodes[splitNode].ratio = next;
        return true;
    }

    Vec2 DockTree::minimum(u32 index) const {
        if (index >= m_nodes.size()) return {};
        const TreeNode& node = m_nodes[index];
        if (node.kind == Kind::Group) {
            Vec2 result{};
            for (u32 panel : node.panels) {
                result.x = std::max(result.x, m_panelData[panel].minimum.x);
                result.y = std::max(result.y, m_panelData[panel].minimum.y);
            }
            return result;
        }
        if (node.kind != Kind::Split) return {};
        const Vec2 a = minimum(node.first), b = minimum(node.second);
        return node.horizontal ? Vec2{ a.x + b.x + m_gutter, std::max(a.y, b.y) }
                               : Vec2{ std::max(a.x, b.x), a.y + b.y + m_gutter };
    }

    Vec2 DockTree::maximum(u32 index) const {
        if (index >= m_nodes.size()) return { kInf, kInf };
        const TreeNode& node = m_nodes[index];
        if (node.kind == Kind::Group) {
            Vec2 result{ kInf, kInf };
            for (u32 panel : node.panels) {
                result.x = std::min(result.x, m_panelData[panel].maximum.x);
                result.y = std::min(result.y, m_panelData[panel].maximum.y);
            }
            return result;
        }
        if (node.kind != Kind::Split) return { kInf, kInf };
        const Vec2 a = maximum(node.first), b = maximum(node.second);
        return node.horizontal ? Vec2{ a.x + b.x + m_gutter, std::min(a.y, b.y) }
                               : Vec2{ std::min(a.x, b.x), a.y + b.y + m_gutter };
    }

    void DockTree::layoutNode(u32 index, Rect bounds, f32 gutter) {
        if (index >= m_nodes.size()) return;
        TreeNode& node = m_nodes[index];
        if (node.kind == Kind::Dead) return;
        node.bounds = bounds;
        if (node.kind != Kind::Split) return;
        const f32 main = node.horizontal ? bounds.w : bounds.h;
        const f32 available = std::max(0.f, main - gutter);
        const Vec2 amin = minimum(node.first), bmin = minimum(node.second);
        const Vec2 amax = maximum(node.first), bmax = maximum(node.second);
        const f32 minA = node.horizontal ? amin.x : amin.y;
        const f32 minB = node.horizontal ? bmin.x : bmin.y;
        const f32 maxA = node.horizontal ? amax.x : amax.y;
        const f32 maxB = node.horizontal ? bmax.x : bmax.y;
        f32 first = available * node.ratio;
        f32 second = 0.f;
        if (available >= minA + minB) {
            const f32 low = std::max(minA, available - maxB);
            const f32 high = std::min(maxA, available - minB);
            first = low <= high ? std::clamp(first, low, high) : minA;
            second = available - first;
        }
        else { first = minA; second = minB; }
        Rect a = bounds, b = bounds;
        if (node.horizontal) { a.w = first; b.x += first + gutter; b.w = second; }
        else { a.h = first; b.y += first + gutter; b.h = second; }
        const Vec2 aminor = minimum(node.first), bminor = minimum(node.second);
        if (node.horizontal) { a.h = std::max(a.h, aminor.y); b.h = std::max(b.h, bminor.y); }
        else { a.w = std::max(a.w, aminor.x); b.w = std::max(b.w, bminor.x); }
        layoutNode(node.first, a, gutter);
        layoutNode(node.second, b, gutter);
    }

    void DockTree::layout(Rect bounds, f32 gutter) {
        m_host = bounds;
        m_gutter = std::max(0.f, gutter);
        if (m_root != kNone) layoutNode(m_root, bounds, m_gutter);
        for (u32 i = 0; i < m_panelData.size(); ++i) {
            const Panel& panel = m_panelData[i];
            DockPanelLayout& result = m_panels[i];
            result.panel = panel.key;
            result.group = panel.group;
            result.order = 0;
            result.active = false;
            result.floating = panel.floating && !panel.closed;
            if (panel.closed) { result.bounds = {}; continue; }
            if (panel.floating) { result.bounds = panel.bounds; result.group = kNone; continue; }
            if (panel.group >= m_nodes.size() || m_nodes[panel.group].kind != Kind::Group) { result.bounds = {}; continue; }
            const TreeNode& group = m_nodes[panel.group];
            result.bounds = group.bounds;
            const auto it = std::find(group.panels.begin(), group.panels.end(), i);
            if (it != group.panels.end()) {
                result.order = static_cast<u32>(it - group.panels.begin());
                result.active = result.order == group.active;
            }
        }
    }

    DockPreview DockTree::preview(u32 targetKey, DockZone zone) const {
        DockPreview result;
        const u32 target = findPanel(targetKey);
        if (target == kNone || m_panelData[target].floating || zone == DockZone::Floating) return result;
        const Rect area = m_nodes[m_panelData[target].group].bounds;
        if (area.empty()) return result;
        result.targetPanel = targetKey;
        result.zone = zone;
        result.valid = true;
        if (zone == DockZone::Center) result.bounds = area;
        else if (zone == DockZone::Left) result.bounds = { area.x, area.y, area.w * 0.32f, area.h };
        else if (zone == DockZone::Right) result.bounds = { area.right() - area.w * 0.32f, area.y, area.w * 0.32f, area.h };
        else if (zone == DockZone::Top) result.bounds = { area.x, area.y, area.w, area.h * 0.32f };
        else result.bounds = { area.x, area.bottom() - area.h * 0.32f, area.w, area.h * 0.32f };
        return result;
    }

    bool DockTree::floating(u32 key) const { const u32 i = findPanel(key); return i != kNone && m_panelData[i].floating; }
    Rect DockTree::panelBounds(u32 key) const { const u32 i = findPanel(key); return i == kNone ? Rect{} : m_panels[i].bounds; }
    Rect DockTree::groupBounds(u32 key) const { const u32 group = groupForPanel(key); return group == kNone ? Rect{} : m_nodes[group].bounds; }
    u32 DockTree::groupOf(u32 key) const { return groupForPanel(key); }
    u32 DockTree::panelOrder(u32 key) const { const u32 i = findPanel(key); return i == kNone ? kNone : m_panels[i].order; }
    u32 DockTree::panelCount(u32 key) const { const u32 group = groupForPanel(key); return group == kNone ? 0 : static_cast<u32>(m_nodes[group].panels.size()); }

    u32 DockTree::activePanelInGroup(u32 group) const {
        if (group >= m_nodes.size() || m_nodes[group].kind != Kind::Group || m_nodes[group].panels.empty()) return kNone;
        const TreeNode& node = m_nodes[group];
        return m_panelData[node.panels[std::min<u32>(node.active, static_cast<u32>(node.panels.size() - 1))]].key;
    }

    u32 DockTree::activePanel(u32 key) const { const u32 group = groupForPanel(key); return group == kNone ? kNone : activePanelInGroup(group); }

    u32 DockTree::panelAt(Vec2 point) const {
        for (std::size_t i = m_panelData.size(); i > 0; --i) {
            const u32 index = static_cast<u32>(i - 1);
            const Panel& panel = m_panelData[index];
            if (panel.closed || panel.floating || panel.group >= m_nodes.size()) continue;
            const TreeNode& group = m_nodes[panel.group];
            if (group.active < group.panels.size() && group.panels[group.active] == index && group.bounds.contains(point)) return panel.key;
        }
        for (std::size_t i = m_panelData.size(); i > 0; --i) {
            const Panel& panel = m_panelData[i - 1];
            if (!panel.closed && panel.floating && panel.bounds.contains(point)) return panel.key;
        }
        return kNone;
    }

    u32 DockTree::splitAt(Vec2 point, bool* horizontal) const {
        for (std::size_t i = m_nodes.size(); i > 0; --i) {
            const u32 index = static_cast<u32>(i - 1);
            const TreeNode& node = m_nodes[index];
            if (node.kind != Kind::Split) continue;
            const Rect a = m_nodes[node.first].bounds;
            Rect gutter = node.bounds;
            if (node.horizontal) { gutter.x = a.right(); gutter.w = m_gutter; }
            else { gutter.y = a.bottom(); gutter.h = m_gutter; }
            if (gutter.contains(point)) { if (horizontal) *horizontal = node.horizontal; return index; }
        }
        return kNone;
    }

    Rect DockTree::splitBounds(u32 index) const { return index < m_nodes.size() && m_nodes[index].kind == Kind::Split ? m_nodes[index].bounds : Rect{}; }
    f32 DockTree::splitRatio(u32 index) const { return index < m_nodes.size() && m_nodes[index].kind == Kind::Split ? m_nodes[index].ratio : 0.f; }

    u32 DockTree::splitterCount() const {
        u32 count = 0;
        for (const TreeNode& node : m_nodes) count += node.kind == Kind::Split;
        return count;
    }

    DockSplitterLayout DockTree::splitterAt(u32 ordinal) const {
        for (u32 index = 0; index < m_nodes.size(); ++index) {
            const TreeNode& node = m_nodes[index];
            if (node.kind != Kind::Split) continue;
            if (ordinal--) continue;
            const Rect first = m_nodes[node.first].bounds;
            Rect bounds = node.bounds;
            if (node.horizontal) { bounds.x = first.right(); bounds.w = m_gutter; }
            else { bounds.y = first.bottom(); bounds.h = m_gutter; }
            return { index, bounds, node.horizontal };
        }
        return {};
    }

    DockZone DockTree::zone(u32 key) const {
        const u32 panel = findPanel(key);
        if (panel == kNone) return DockZone::Floating;
        if (m_panelData[panel].floating) return DockZone::Floating;
        u32 child = m_panelData[panel].group;
        for (u32 parent = child < m_nodes.size() ? m_nodes[child].parent : kNone; parent != kNone; child = parent, parent = m_nodes[parent].parent) {
            if (parent == m_root && m_nodes[parent].kind == Kind::Split) {
                const bool first = m_nodes[parent].first == child;
                if (m_nodes[parent].horizontal) return first ? DockZone::Left : DockZone::Right;
                return first ? DockZone::Top : DockZone::Bottom;
            }
        }
        return DockZone::Center;
    }

} // namespace xui
