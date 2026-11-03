/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Editor/EditorContext.h"
#include "Editor/EditorPaths.h"
#include "Editor/LandBrush.h"

namespace openblack::editor
{

/// The editor's tools for the land itself: the brush that raises and lowers the land and paints its countries, the
/// drawing of footpaths and streams, and the save of what the editor placed and changed to files of their own. The
/// tab sets the tools up; the window gives it the clicks and drags in the world while the land or paths tool is on.
class LandToolsPanel
{
public:
	void Draw(EditorContext& context) noexcept;

	/// A left click in the world with the land or paths tool: the land tool makes a stroke of its brush, the paths
	/// tool adds a point to the path that it draws. The point is where the click meets the land, and the thing is the
	/// thing under the click.
	void PressAt(EditorContext& context, std::optional<glm::vec3> land, std::optional<entt::entity> picked) noexcept;
	/// The land tool's drag: another stroke each time the mouse moves far enough over the land
	void DragTo(std::optional<glm::vec3> land) noexcept;
	/// Draws the brush's circles round the point of the land under the mouse, the path being drawn and the scripts'
	/// markers, which the game does not draw
	void DrawOverlays(const EditorContext& context, std::optional<glm::vec3> mouseLand) const noexcept;

	/// The path being drawn ends and becomes a footpath or a stream on the land
	void FinishPath(EditorContext& context) noexcept;

	/// The brush is the editor's, so that the land script can set it
	[[nodiscard]] static land_brush::Brush& GetBrush() noexcept;
	[[nodiscard]] paths::Kind GetPathKind() const noexcept { return _pathKind; }
	void SetPathKind(paths::Kind kind) noexcept;
	[[nodiscard]] const paths::Draft& GetDraft() const noexcept { return _draft; }

	/// Saves the land's changed cells as a landscape file, what the editor placed and drew as a land script, and the
	/// whole world as a world file, into the editor's folder in the profile folder. Gives what it wrote.
	std::string Save(EditorContext& context) noexcept;
	/// Loads the world file that Save wrote for the land that is open
	std::string LoadWorld() noexcept;

private:
	void Stroke(glm::vec2 centre) noexcept;

	/// The point of the last stroke of a drag of the brush
	std::optional<glm::vec2> _lastStroke;
	paths::Kind _pathKind {paths::Kind::Footpath};
	paths::Draft _draft;
	/// The buildings that the first and the last point of the footpath were put on, to link the path to
	std::optional<entt::entity> _firstHolder;
	std::optional<entt::entity> _lastHolder;
	std::string _status;
};

} // namespace openblack::editor
