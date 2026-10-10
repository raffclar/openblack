/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>
#include <span>

#include <glm/vec2.hpp>

#include "Common/Zoomer.h"
#include "Dialog.h"

namespace openblack::ecs::systems
{
class TattooEditorSystemInterface;
}

namespace openblack::gui
{

class ColourPicker;
class DraggedSymbol;
class GameFont;
class TextDatabase;

/// The tattoo editor's dialog: a line of help along the top, the sixteen symbols in two columns of eight down the
/// sides, a colour palette at the far left and a brightness bar at the far right, OK and Cancel along the bottom and
/// the round button that turns the view in the middle below the creature.
///
/// Dragging a symbol over a place on the creature makes it flash and letting go puts it on there. Pressing on a place
/// with a tattoo lifts it off to be dragged elsewhere. The palette picks the colour the brightness bar runs through,
/// and the brightness bar how light or dark the symbols are, which is the colour they go on in. Enter is OK and Escape
/// Cancel. From the main menu the dialog has a frame of its own; in the temple it stands over the cave.
class TattooEditorDialog
{
public:
	TattooEditorDialog(const TextDatabase& texts, const GameFont& font);
	~TattooEditorDialog();

	/// Fades in, with a frame of its own or none
	void Open(bool framed);
	/// Fades out
	void Close();
	[[nodiscard]] bool IsOpen() const noexcept { return _open; }
	[[nodiscard]] bool IsVisible() const noexcept { return _open || _fade.GetValue() > 0.0f; }

	// Input at points of the dialog space
	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	/// True when a control acted, which the game's dialogs answer with the menu button sound
	bool MouseUp(glm::ivec2 point, ecs::systems::TattooEditorSystemInterface& editor);
	/// Enter is OK and Escape Cancel; true when the key was taken
	bool KeyDown(int key, ecs::systems::TattooEditorSystemInterface& editor);

	/// Every frame: what the held controls do, and lifting a tattoo off the place the mouse button is down on. The
	/// palette is the tattoo palette's colours.
	void Update(float deltaSeconds, ecs::systems::TattooEditorSystemInterface& editor,
	            std::span<const std::array<uint8_t, 3>> palette, bool leftButtonDown);

	/// The dialog, and the marker on the place under the pointer while the mouse button is down, at a point of the
	/// dialog space
	void Draw(const DialogPainter& painter, std::optional<glm::ivec2> marker, uint32_t milliseconds) const;

	/// The symbols' colour
	[[nodiscard]] glm::u8vec4 GetTint() const noexcept { return _tint; }
	[[nodiscard]] const DraggedSymbol& GetSymbol(size_t index) const { return *_symbols.at(index); }

	/// Where the controls are, for hit tests
	static constexpr glm::ivec2 k_OkPosition {150, 500};
	static constexpr glm::ivec2 k_CancelPosition {610, 500};
	static constexpr glm::ivec2 k_RotationPosition {352, 450};
	static constexpr int k_ArrowSize = 40;
	static constexpr int k_RotationSize = 96;
	[[nodiscard]] static glm::ivec2 GetSymbolPosition(size_t index);

private:
	void ApplyTint();
	void SetTint(glm::u8vec4 tint);
	void Ok(ecs::systems::TattooEditorSystemInterface& editor);
	void Cancel(ecs::systems::TattooEditorSystemInterface& editor);

	Dialog _dialog;
	ColourPicker* _palette {nullptr};
	ColourPicker* _brightness {nullptr};
	std::array<DraggedSymbol*, 16> _symbols {};
	glm::u8vec4 _tint {255, 255, 255, 255};
	std::span<const std::array<uint8_t, 3>> _paletteColours;
	/// What the held controls want of the editor this frame
	ecs::systems::TattooEditorSystemInterface* _editor {nullptr};
	float _frameMilliseconds {0.0f};
	/// Where the rotation button was pressed, while it is held
	std::optional<glm::ivec2> _grab;
	/// The symbol let go of this frame, to drop onto the creature
	std::optional<size_t> _dropped;
	bool _framed {false};
	bool _open {false};
	Zoomer _fade;
};

} // namespace openblack::gui
