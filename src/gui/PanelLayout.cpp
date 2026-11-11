#include "PanelLayout.h"

#include "imgui.h"

#include <algorithm>

namespace gm {

ofRectangle PanelLayout::region(float x0, float y0, float x1, float y1) const {
	return ofRectangle(canvas_.x + x0 * canvas_.width, canvas_.y + y0 * canvas_.height,
		(x1 - x0) * canvas_.width, (y1 - y0) * canvas_.height);
}

ofRectangle PanelLayout::inset(const ofRectangle & rect, float gutter) {
	return ofRectangle(rect.x + gutter, rect.y + gutter,
		std::max(rect.width - 2.0f * gutter, 0.0f), std::max(rect.height - 2.0f * gutter, 0.0f));
}

bool PanelLayout::begin(const std::string & title, const ofRectangle & rect, bool * open) const {
	ImGui::SetNextWindowPos(ImVec2(rect.x, rect.y), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(rect.width, rect.height), ImGuiCond_Always);
	constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
	return ImGui::Begin(title.c_str(), open, flags);
}

bool PanelLayout::beginBar(const std::string & id, const ofRectangle & rect) const {
	ImGui::SetNextWindowPos(ImVec2(rect.x, rect.y), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(rect.width, rect.height), ImGuiCond_Always);
	constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	// A bar is lower than ImGui's minimum window height (which also grows
	// with the display scale), so lift that limit for this one window.
	ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(8.0f, 8.0f));
	bool open = ImGui::Begin(id.c_str(), nullptr, flags);
	ImGui::PopStyleVar();
	return open;
}

void PanelLayout::end() const {
	ImGui::End();
}

} // namespace gm
