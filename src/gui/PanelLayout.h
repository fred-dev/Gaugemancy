#pragma once

// -----------------------------------------------------------------------------
// PanelLayout: a fixed, proportional panel grid for ofxImGui.
//
// Floating ImGui windows are great for occasional, on-demand dialogs, but a
// panel you read and use constantly (the transport, the slot editor, the
// output meter...) should never need to be found, moved or resized - it
// should always be in the same place, at the same size, relative to the
// window. That is what this gives you: each panel is a fraction [0,1] of a
// canvas rectangle (normally "the window, minus the menu bar"), recomputed
// every frame, so resizing the app window scales the whole grid instead of
// leaving panels stranded, overlapped or off-screen.
//
// Panels built with begin()/end() cannot be moved, resized or collapsed by
// the user; content that doesn't fit scrolls in place (ImGui's normal
// behaviour for a fixed-size window) rather than requiring the panel itself
// to grow or move. Reserve this for the small set of panels that must always
// be visible in a reliable spot; a one-off or occasional dialog (Settings,
// a confirmation popup) should stay an ordinary, user-movable ImGui window -
// it is a different context, not part of the fixed working layout.
// -----------------------------------------------------------------------------

#include "ofRectangle.h"

#include <string>

namespace gm {

class PanelLayout {
public:
	/// The rectangle fractional regions are measured against this frame.
	void setCanvas(const ofRectangle & canvas) { canvas_ = canvas; }
	const ofRectangle & canvas() const { return canvas_; }

	/// Pixel rectangle for the fractional region [x0,y0]-[x1,y1] (each in
	/// 0..1) of the canvas - e.g. region(0, 0, 0.125f, 0.166f) is the top
	/// left, 1/8 of the canvas wide and 1/6 tall, wherever the canvas is.
	ofRectangle region(float x0, float y0, float x1, float y1) const;

	/// Shrinks `rect` by `gutter` on every side - the visible gap between
	/// adjacent fixed panels, without changing their nominal grid footprint.
	static ofRectangle inset(const ofRectangle & rect, float gutter);

	/// Begins a panel pinned at exactly `rect`, every frame: no move, no
	/// resize, no collapse. Mirrors ImGui::Begin's bool return (always true
	/// unless `open` is given and closed). Pair with end().
	bool begin(const std::string & title, const ofRectangle & rect, bool * open = nullptr) const;

	/// The same, without a title bar and without scrolling: for the thin
	/// bars at the top and bottom of the window. Pair with end().
	bool beginBar(const std::string & id, const ofRectangle & rect) const;
	void end() const;

private:
	ofRectangle canvas_;
};

} // namespace gm
