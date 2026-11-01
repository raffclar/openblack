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
#include <string_view>
#include <utility>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Debug/Window.h"
#include "EditorActions.h"
#include "EditorContext.h"
#include "EditorEntities.h"
#include "Panels/InspectorPanel.h"
#include "Panels/LandScriptPanel.h"
#include "Panels/LogPanel.h"
#include "Panels/OutlinerPanel.h"
#include "Panels/PalettePanel.h"
#include "Panels/ScriptsPanel.h"

namespace openblack::debug::gui
{
class CreatureSpawner;
}

namespace openblack::editor
{

/// The in-game editor for testing and building on the land, toggled with F2. It lays out a tool bar along the top, the
/// outliner on the left, the inspector on the right and a strip of tabs along the bottom (the palette, the testbed's
/// scenarios, the scripts and the scripts' log), round the view of the world, where the mouse picks, moves, turns and
/// places things and drives the editor's cameras. Closed, it only listens for its key and the game is as without it.
class EditorWindow final: public debug::gui::Window
{
public:
	EditorWindow(debug::gui::CreatureSpawner& spawner, debug::gui::Window& scenarios) noexcept;
	~EditorWindow() noexcept override;

	void Open() noexcept override;
	void Close() noexcept override;
	void WindowDraw() noexcept override;

protected:
	void Draw() noexcept override {}
	void Update() noexcept override;
	void UpdateAlways() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;
	[[nodiscard]] bool TakesEvent(const SDL_Event& event) const noexcept override;

private:
	enum class Drag : uint8_t
	{
		None,
		Move,
		Rotate,
		Camera,
		/// A rectangle on the screen that picks the things in it
		Box,
	};
	enum class BottomTab : uint8_t
	{
		Palette,
		Scenarios,
		Scripts,
		LandScript,
		Log,
	};

	[[nodiscard]] std::optional<EditorContext> Context() noexcept;
	void DrawToolbar(EditorContext& context) noexcept;
	void DrawBottom(EditorContext& context) noexcept;
	void DrawOverlays(EditorContext& context) noexcept;
	/// The box round a thing, or round where an item would go, drawn over the world
	static void DrawBox(const AxisAlignedBoundingBox& box, uint32_t colour, float thickness) noexcept;

	/// Whether a key is one of the editor's while it is open
	[[nodiscard]] bool IsEditorKey(const SDL_Event& event) const noexcept;
	void HandleKey(const SDL_Event& event) noexcept;
	/// The thing whose bounds the ray under a point of the screen, 0 to 1, meets first
	[[nodiscard]] std::optional<entt::entity> Pick(glm::vec2 screen) const noexcept;
	/// Where the ray under a point of the screen meets the land
	[[nodiscard]] static std::optional<glm::vec3> LandAt(glm::vec2 screen) noexcept;
	[[nodiscard]] static glm::vec2 MouseOnScreen() noexcept;
	void PlaceAt(EditorContext& context, glm::vec2 screen) noexcept;
	/// Picks what a click of the left button meets, with the keys held at the click, and starts a drag of the tool
	void PickAt(EditorContext& context, glm::vec2 screen) noexcept;
	/// Moves or turns the picked things for the drag of the move or rotate tool
	void UpdateDrag(EditorContext& context, entt::entity primary) noexcept;
	/// Picks the things whose middles are in the rectangle of the drag
	void FinishBox(EditorContext& context) noexcept;
	/// Ends a drag of the move or rotate tool, as one step of the history
	void EndDrag(EditorContext& context) noexcept;

	debug::gui::CreatureSpawner& _spawner;
	debug::gui::Window& _scenarios;
	std::optional<NameTables> _names;
	Placement _placement;
	OutlinerPanel _outliner;
	InspectorPanel _inspector;
	PalettePanel _palette;
	ScriptsPanel _scripts;
	LandScriptPanel _landScript;
	LogPanel _log;

	bool _showOutliner {true};
	bool _showInspector {true};
	bool _showBottom {true};
	float _leftWidth {300.0f};
	float _rightWidth {440.0f};
	float _bottomHeight {320.0f};
	/// The toolbar wraps onto more rows when the window is too narrow for it. Its height, and the widths of the
	/// groups that may move down a row, are taken from the frame before.
	float _toolbarHeight {0.0f};
	float _cameraGroupWidth {0.0f};
	float _panelGroupWidth {0.0f};
	std::optional<BottomTab> _showTab;

	/// Clicks in the world to act on at the next update, as points of the screen from 0 to 1
	std::optional<glm::vec2> _pickAt;
	/// The keys held at the click: Ctrl picks or lets go of one more thing, Shift picks one more thing
	bool _pickToggle {false};
	bool _pickAdd {false};
	/// The corners of the rectangle that picks, as points of the screen from 0 to 1. The second corner is set when the
	/// button comes up.
	glm::vec2 _boxFrom {0.0f};
	std::optional<glm::vec2> _boxTo;
	/// The poses of the picked things as a drag of the move or rotate tool started
	PoseEdit _dragEdit;
	std::vector<std::pair<entt::entity, glm::vec3>> _dragStarts;
	/// Where the mouse was as a move started, until it first moves
	std::optional<glm::vec2> _dragFrom;
	/// A drag of the move or rotate tool that stopped, to keep in the history at the next update
	bool _dragEnded {false};
	/// The label of the step that the drag becomes
	std::string_view _dragAction;
	/// What the last undo or redo did
	std::string _historyNote;
	std::optional<glm::vec2> _placeAt;
	Drag _drag {Drag::None};
	glm::ivec2 _dragLast {0};
	float _dragAcross {0.0f};
	/// The picked thing's place less the land under the mouse, and its turn, as a drag starts
	glm::vec3 _moveOffset {0.0f};
	float _rotateFrom {0.0f};
	float _rotated {0.0f};
};

} // namespace openblack::editor
