/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Controls.h"

namespace openblack::gui
{

/// The background of a box with tabs along its top: as wide as the box, from half its height above the middle of the
/// dialog space to 40 more than half its height below it
[[nodiscard]] constexpr DialogRect TabbedBox(glm::ivec2 size)
{
	return {.min = {400 - (size.x / 2), 300 - (size.y / 2)}, .max = {400 + (size.x / 2), 340 + (size.y / 2)}};
}

/// A page of the game's menu (a DialogBoxBase and its SetupBox): a see-through box with tabs along its top, of which
/// one is open, and the controls in it.
///
/// The control under the pointer lights up. The mouse button going down on a control gives it the focus, and while the
/// button is held the control follows the pointer every frame. Letting go over the control the button went down on
/// makes it act; while the button is held no other control lights up.
class Dialog
{
public:
	struct Tab
	{
		std::u16string label;
		std::function<void()> onSelect;
		/// The size of the label's text
		int textSize {DialogPainter::MidTextSize()};
	};

	/// MainMenu and the options' box: 780 by 500 in the middle of the dialog space, 40 deeper for its tabs
	static constexpr DialogRect k_Box = TabbedBox({780, 500});
	/// AddMainMenuTabs and AddOptionsTabs: five tabs, a fifth of the box's width each
	static constexpr size_t k_TabCount = 5;

	Dialog(std::vector<Tab> tabs, size_t selectedTab);
	/// A box of its own size with as many tabs as given along its top, each a share of its width. Every tab keeps
	/// what it does, the selected one too.
	Dialog(std::vector<Tab> tabs, size_t selectedTab, DialogRect box);
	/// Controls on their own, without a box or tabs
	Dialog() = default;

	template <typename T, typename... Args>
	T& Add(Args&&... args)
	{
		return static_cast<T&>(*_controls.emplace_back(std::make_unique<T>(std::forward<Args>(args)...)));
	}

	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	/// True when a control acted, which SetupBox answers with the menu button sound
	bool MouseUp(glm::ivec2 point);
	/// The mouse button, already down, now holds a control, as though it had gone down on it
	void Hold(Control& control);
	/// Whether the mouse button is down on a control
	[[nodiscard]] bool IsHolding() const noexcept { return _pressed != nullptr; }
	/// Whether the pointer is over a control that takes the mouse
	[[nodiscard]] bool IsPointerOverControl() const;
	/// The mouse wheel turned some notches over a point: steps away from the player, and across to the right on a
	/// horizontal wheel. The top control that takes the wheel at the point gets them.
	void Wheel(glm::ivec2 point, int steps, int across = 0);
	/// The same, over the point where the last move of the pointer put it
	void Wheel(int steps, int across) { Wheel(_pointer, steps, across); }
	bool TextInput(std::u16string_view text);
	/// A key pressed while the dialog has the keyboard. Tab, or Shift and Tab, moves the focus to the next or previous
	/// control (see MoveFocus). Return in an edit box clicks it and moves the focus on. Other keys go to the control that
	/// has the focus. True when a control took the key.
	bool KeyDown(int key, bool shift = false);
	/// Tab moves the focus over the visible controls but for static text, in the order they were made. The tabs of the
	/// menu's pages come after the other controls, because the game adds them last. Backwards, the focus moves the other
	/// way.
	void MoveFocus(bool backwards);
	/// Every frame: the controls move on, and the one the mouse button is held down on follows the pointer
	void Update(float deltaSeconds);
	/// Forgets the pointer, the button and the focus
	void Reset();

	/// The box, its tabs and its controls. Inactive, behind a question, nothing lights up.
	void Draw(const DialogPainter& painter, bool active) const;

	/// A control that has a name on the screen, where it is, for tools pressing it by name
	struct NamedControl
	{
		std::u16string name;
		std::string kind;
		DialogRect rect;
	};
	/// The visible controls that act and have a name: buttons, check boxes, sliders and tabs, in the order they were made
	[[nodiscard]] std::vector<NamedControl> GetNamedControls() const;
	/// The places of all the visible controls, texts too, in the order they were made
	[[nodiscard]] std::vector<DialogRect> GetVisibleRects() const;

	[[nodiscard]] static DialogRect GetTabRect(size_t index);
	/// Opens another of the tabs
	void SelectTab(size_t index);
	/// Names one of the tabs again
	void SetTabLabel(size_t index, std::u16string label);
	[[nodiscard]] Control* GetFocus() const noexcept { return _focused; }
	/// Gives a control the keyboard's focus, or none; the control losing it and the one taking it are told
	void SetFocus(Control* control);
	/// The control that takes the mouse at a point: the control that the mouse button is held on, else the top control
	[[nodiscard]] Control* HitTest(glm::ivec2 point) const;

private:
	/// The box behind the controls, none for controls on their own, and its tabs
	std::optional<DialogRect> _box;
	size_t _tabCount {0};
	/// Tab comes to the tabs after the other controls
	bool _tabsLast {false};

public:
	/// Drawn before the box, under it
	std::function<void(const DialogPainter&)> drawUnder;

private:
	std::vector<std::unique_ptr<Control>> _controls;
	Control* _hovered {nullptr};
	Control* _pressed {nullptr};
	Control* _focused {nullptr};
	glm::ivec2 _pointer {-1, -1};
};

} // namespace openblack::gui
