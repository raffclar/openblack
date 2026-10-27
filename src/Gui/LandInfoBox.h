/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <string>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "Common/Zoomer.h"
#include "Dialog.h"

namespace openblack::gui
{

class GameFont;
class List;
class StaticText;
class TextDatabase;

/// The land information box. The script of a playground land shows it as the land loads: the title of the land, and the
/// lines of a list under it. The game goes on under the box, but the box takes the mouse and the keyboard.
///
/// A box of 780 by 580 in the middle of the dialog space. The title is across its top, on a light grey band at half
/// opacity. The list is under the title and shows no selection. Back is an arrow at the bottom left. Go! is a button in
/// the middle of the bottom. Go! and Escape close the box. Back closes the box and ends the skirmish, and the skirmish
/// box shows again. The box fades in over half a second and out over a fifth.
class LandInfoBox
{
public:
	enum class Choice : uint8_t
	{
		None,
		/// The player plays the land
		Go,
		/// The player leaves the land for the skirmish box
		Back,
	};

	static constexpr DialogRect k_Box {.min = {10, 10}, .max = {790, 590}};
	static constexpr DialogRect k_Title {.min = {40, 30}, .max = {760, 70}};
	static constexpr DialogRect k_List {.min = {100, 110}, .max = {700, 490}};
	static constexpr DialogRect k_Go {.min = {320, 530}, .max = {480, 570}};
	static constexpr glm::ivec2 k_BackPosition {30, 530};
	static constexpr int k_ArrowSize = 40;

	LandInfoBox(const TextDatabase& texts, const GameFont& font);
	LandInfoBox(const LandInfoBox&) = delete;
	LandInfoBox& operator=(const LandInfoBox&) = delete;
	LandInfoBox(LandInfoBox&&) = delete;
	LandInfoBox& operator=(LandInfoBox&&) = delete;
	~LandInfoBox();

	/// Shows the box with this title and an empty list
	void Show(std::u16string title);
	/// Adds a line at the end of the list. The box keeps the line also while it does not show.
	void AddLine(std::u16string line);
	/// The box shows and takes the input
	[[nodiscard]] bool IsActive() const noexcept { return _active; }
	/// The box shows, or it fades out
	[[nodiscard]] bool IsVisible() const noexcept { return _active || _fade.GetValue() > 0.0f; }

	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	/// True when a control was let go over: the game clicks
	bool MouseUp(glm::ivec2 point);
	void Wheel(glm::ivec2 point, int steps);
	/// A key with the SDL modifier keys held. Escape with no modifier key closes the box as Go! does.
	void KeyDown(int key, uint16_t modifiers);
	void Update(float deltaSeconds);
	void Draw(const DialogPainter& painter) const;

	/// What the player chose, once
	[[nodiscard]] Choice TakeChoice() noexcept { return std::exchange(_choice, Choice::None); }
	[[nodiscard]] const StaticText& GetTitle() const noexcept { return *_title; }
	[[nodiscard]] const List& GetList() const noexcept { return *_list; }

	/// The controls that act, for tools that press them by name: Back and Go!
	[[nodiscard]] std::vector<Dialog::NamedControl> GetNamedControls() const;

private:
	void Hide(Choice choice);

	Dialog _dialog;
	StaticText* _title {nullptr};
	List* _list {nullptr};
	Choice _choice {Choice::None};
	bool _active {false};
	Zoomer _fade;
};

} // namespace openblack::gui
