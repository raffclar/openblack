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

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Common/Zoomer.h"
#include "DialogPainter.h"

namespace openblack::gui
{

class GameFont;

/// One of the controls of the game's dialogs, laid out in the dialogs' 800 by 600 space.
///
/// Controls light up under the pointer, take the focus when the mouse button goes down on them, and act when it is let
/// go over the control it went down on (see Dialog).
class Control
{
public:
	explicit Control(DialogRect rect)
	    : rect(rect)
	{
	}
	virtual ~Control() = default;
	Control(const Control&) = delete;
	Control& operator=(const Control&) = delete;

	virtual void Draw(const DialogPainter& painter, bool hovered, bool focused, bool pressed) const = 0;
	/// Whether the control takes the mouse at a point
	[[nodiscard]] virtual bool HitTest(glm::ivec2 point) const { return visible && rect.Contains(point); }
	/// Static text takes no part in the mouse or the keyboard
	[[nodiscard]] virtual bool IsInteractive() const { return true; }
	/// Drawn after the others, over them
	[[nodiscard]] virtual bool IsOnTop() const { return false; }
	/// What the control is called on the screen, its label, for tools pressing it by name; none for one without
	[[nodiscard]] virtual std::u16string_view GetName() const { return {}; }
	/// What kind of control it is, for tools listing a page's controls: "button", "check box", "slider", "tab"
	[[nodiscard]] virtual std::string_view GetKind() const { return "control"; }

	/// The pointer moved, wherever it is
	virtual void MouseMove(glm::ivec2 /*point*/) {}
	/// The mouse button went down on the control
	virtual void MouseDown(glm::ivec2 /*point*/) {}
	/// Every frame while the mouse button is held down after going down on the control
	virtual void Drag(glm::ivec2 /*point*/) {}
	/// Every frame
	virtual void Update(float /*deltaSeconds*/) {}
	/// The mouse button was let go, wherever the pointer is, after going down on the control
	virtual void Release(glm::ivec2 /*point*/) {}
	/// The mouse button was let go over the control it went down on
	virtual void Activate(glm::ivec2 /*point*/)
	{
		if (onClick)
		{
			onClick();
		}
	}
	virtual void Wheel(int /*steps*/) {}
	/// Typing while the control has the focus, true when it took it
	virtual bool TextInput(std::u16string_view /*text*/) { return false; }
	virtual bool KeyDown(int /*key*/) { return false; }

	DialogRect rect;
	bool visible {true};
	std::function<void()> onClick;
};

/// Static text with a shadow, shrinking to fit
class StaticText final: public Control
{
public:
	enum class Layout
	{
		Left,
		Centre,
		Right,
		/// Broken into lines, centred
		Wrapped,
		/// Broken into lines from the left
		WrappedLeft,
	};

	StaticText(DialogRect rect, std::u16string text, Layout layout, int size = DialogPainter::k_BigTextSize);
	void Draw(const DialogPainter& painter, bool hovered, bool focused, bool pressed) const override;
	/// Static text takes the mouse only when asked to: a click on it then clicks, but does nothing
	[[nodiscard]] bool IsInteractive() const override { return takesClicks; }

	std::u16string text;
	Layout layout;
	int size;
	bool takesClicks {false};
};

/// A button: a dark box with a label, orange under the pointer
class Button final: public Control
{
public:
	Button(DialogRect rect, std::u16string label, int size = DialogPainter::k_BigTextSize);
	void Draw(const DialogPainter& painter, bool hovered, bool focused, bool pressed) const override;
	[[nodiscard]] std::u16string_view GetName() const override { return label; }
	[[nodiscard]] std::string_view GetKind() const override { return "button"; }

	std::u16string label;
	int size;
};

/// A big button: an arrow, or a black square, with a label beside or below it
class BigButton: public Control
{
public:
	enum class Look
	{
		/// A black square
		Square,
		LeftArrow,
		RightArrow,
		/// The disc of turning arrows, never pushed in
		Rotation,
	};
	enum class LabelSide
	{
		Right,
		Left,
		Below,
	};

	BigButton(const GameFont& font, glm::ivec2 position, int size, std::u16string label, LabelSide side, Look look);
	void Draw(const DialogPainter& painter, bool hovered, bool focused, bool pressed) const override;
	/// The button or its label
	[[nodiscard]] bool HitTest(glm::ivec2 point) const override;
	[[nodiscard]] std::u16string_view GetName() const override { return label; }
	[[nodiscard]] std::string_view GetKind() const override { return "button"; }

	std::u16string label;
	/// Labels beside the button are the dialog's text size, below it the middle size
	void SetLabelSize(int size) noexcept { _labelSize = size; }

protected:
	/// Where the label is drawn, without its shadow
	[[nodiscard]] DialogRect GetLabelRect() const;
	void DrawLabel(const DialogPainter& painter, bool hovered) const;

	const GameFont& _font;
	LabelSide _side;
	Look _look;
	int _labelSize {DialogPainter::k_MidTextSize};
	/// How much further from the button a label beside it is
	int _labelGap {0};
	bool _checked {false};
};

/// A check box: a square ticked when checked, labelled below unless asked otherwise. A radio button only ever ticks.
class CheckBox final: public BigButton
{
public:
	/// Check boxes are 25 pixel squares unless a size is given
	static constexpr int k_DefaultSize = 25;

	CheckBox(const GameFont& font, glm::ivec2 position, std::u16string label, bool checked);
	CheckBox(const GameFont& font, glm::ivec2 position, int size, std::u16string label, LabelSide side, bool checked,
	         bool radio);
	void Activate(glm::ivec2 point) override;
	/// The circle inside the square, or the label
	[[nodiscard]] bool HitTest(glm::ivec2 point) const override;
	[[nodiscard]] std::string_view GetKind() const override { return "check box"; }

	[[nodiscard]] bool IsChecked() const noexcept { return _checked; }
	void SetChecked(bool checked) noexcept { _checked = checked; }
	std::function<void(bool)> onChange;

private:
	bool _radio {false};
};

/// A slider: a dark bar with a square knob and a label along it. Holding the mouse button down beside the knob
/// steps it a tenth towards the pointer every frame, dragging the knob moves it with the pointer.
class Slider final: public Control
{
public:
	Slider(DialogRect rect, std::u16string label, float value);
	void Draw(const DialogPainter& painter, bool hovered, bool focused, bool pressed) const override;
	[[nodiscard]] std::u16string_view GetName() const override { return label; }
	[[nodiscard]] std::string_view GetKind() const override { return "slider"; }
	void MouseDown(glm::ivec2 point) override;
	void Drag(glm::ivec2 point) override;

	[[nodiscard]] float GetValue() const noexcept { return _value; }
	void SetValue(float value) noexcept;
	std::function<void(float)> onChange;
	std::u16string label;

private:
	[[nodiscard]] int GetKnobLeft(float value) const;

	float _value;
	/// Where the mouse button went down, and the value then
	int _downX {0};
	float _downValue {0.0f};
};

/// A list: lines of text in a dark box, the selected one on a grey bar, with a scroll bar when they don't fit
class List final: public Control
{
public:
	struct Item
	{
		std::u16string text;
		/// A mouse drawn in the gap left for it by five spaces before a "(", or after the text without one
		std::optional<int> mouseCell;
		bool mouseMirrored {false};
	};

	List(const GameFont& font, DialogRect rect, int size, bool centred, bool scrollBar, bool showSelection);
	void Draw(const DialogPainter& painter, bool hovered, bool focused, bool pressed) const override;
	void MouseMove(glm::ivec2 point) override { _pointer = point; }
	void MouseDown(glm::ivec2 point) override;
	void Drag(glm::ivec2 point) override;
	void Wheel(int steps) override;

	void SetItems(std::vector<Item> items);
	void SetMouseCell(size_t index, int cell)
	{
		if (index < _items.size())
		{
			_items[index].mouseCell = cell;
		}
	}
	[[nodiscard]] const std::vector<Item>& GetItems() const noexcept { return _items; }
	[[nodiscard]] std::optional<size_t> GetSelection() const noexcept { return _selection; }
	void SetSelection(std::optional<size_t> selection) noexcept { _selection = selection; }
	[[nodiscard]] float GetScroll() const noexcept { return _scroll; }
	/// Width of the scroll bar, beside the box
	[[nodiscard]] int GetScrollBarWidth() const noexcept { return _size - 2; }
	/// Right of the box the items are in, left of the scroll bar
	[[nodiscard]] int GetBoxRight() const;

private:
	[[nodiscard]] int GetContentHeight() const;
	[[nodiscard]] std::optional<size_t> ItemAt(int y) const;
	void ScrollTo(float scroll);

	const GameFont& _font;
	int _size;
	bool _centred;
	bool _scrollBar;
	bool _showSelection;
	std::vector<Item> _items;
	std::vector<int> _heights;
	std::optional<size_t> _selection;
	float _scroll {0.0f};
	bool _draggingThumb {false};
	int _grabY {0};
	float _grabScroll {0.0f};
	glm::ivec2 _pointer {-1, -1};
};

/// An edit box: a dark box of text the player types into when it has the focus
class EditBox final: public Control
{
public:
	EditBox(DialogRect rect, std::u16string text, size_t maxLength);
	void Draw(const DialogPainter& painter, bool hovered, bool focused, bool pressed) const override;
	void MouseDown(glm::ivec2 point) override;
	bool TextInput(std::u16string_view text) override;
	bool KeyDown(int key) override;

	[[nodiscard]] const std::u16string& GetText() const noexcept { return _text; }
	std::function<void(const std::u16string&)> onChange;

private:
	std::u16string _text;
	size_t _maxLength;
	size_t _caret;
};

/// A colour picker: a strip of the palette's colours, or a brightness bar from black at the top through a colour at the
/// middle to white at the bottom, in a bevelled box, with an arrow beside it at the height picked. Holding the mouse
/// button down on it moves the arrow with the pointer.
class ColourPicker final: public Control
{
public:
	enum class Kind
	{
		Palette,
		Brightness,
	};

	ColourPicker(DialogRect rect, Kind kind);
	void Draw(const DialogPainter& painter, bool hovered, bool focused, bool pressed) const override;
	void MouseMove(glm::ivec2 point) override { _pointer = point; }
	void Drag(glm::ivec2 point) override;

	/// How far down the arrow is, 0 to 1
	[[nodiscard]] float GetPosition() const noexcept { return _position; }
	/// The brightness bar's colour at its middle, see-through grey until a colour is picked
	glm::u8vec4 colour {128, 128, 128, 0};
	/// While the mouse button is held, every frame, at the pointer
	std::function<void(glm::ivec2)> onDrag;

private:
	Kind _kind;
	float _position {0.5f};
	glm::ivec2 _pointer {-1, -1};
};

/// One of the player symbols as a picture the player drags: drawn in its tint, orange under the pointer, and while it
/// is dragged drawn again under the pointer as well, over everything, flashing between its tint and the tint's
/// opposite while it is over somewhere it can be dropped
class DraggedSymbol final: public Control
{
public:
	static constexpr int k_Size = 64;

	DraggedSymbol(glm::ivec2 position, int symbol);
	void Draw(const DialogPainter& painter, bool hovered, bool focused, bool pressed) const override;
	[[nodiscard]] bool IsOnTop() const override { return _dragging; }
	void MouseMove(glm::ivec2 point) override { _pointer = point; }
	void MouseDown(glm::ivec2 point) override;
	void Release(glm::ivec2 point) override;

	/// Starts dragging it without the mouse button going down on it, as when a tattoo is lifted off the creature
	void StartDragging() noexcept { _dragging = true; }
	[[nodiscard]] bool IsDragging() const noexcept { return _dragging; }
	[[nodiscard]] int GetSymbol() const noexcept { return _symbol; }

	/// Its colour. With none at all, not even opacity, it is drawn as text is, with a shadow.
	glm::u8vec4 tint {255, 255, 255, 255};
	/// Whether it is over somewhere it can be dropped, which makes it flash
	bool overTarget {false};
	/// The milliseconds the flashing goes by
	uint32_t milliseconds {0};
	/// Let go of after being dragged, wherever it is
	std::function<void()> onDrop;

private:
	int _symbol;
	bool _dragging {false};
	glm::ivec2 _pointer {-1, -1};
};

/// The player's symbol. Holding the mouse button down on it brings the symbols up around it on a dark
/// see-through disc, to let go over the one to pick: a ring of six inside a ring of ten. The disc fades in over half a
/// second and out over a second, the symbols growing as it does and the two rings turning into place, the inner one
/// way and the outer the other.
class SymbolPicture final: public Control
{
public:
	static constexpr int k_SymbolCount = 16;

	SymbolPicture(DialogRect rect, int symbol);
	void Draw(const DialogPainter& painter, bool hovered, bool focused, bool pressed) const override;
	[[nodiscard]] bool HitTest(glm::ivec2 point) const override;
	void MouseMove(glm::ivec2 point) override { _pointer = point; }
	void MouseDown(glm::ivec2 point) override;
	void Activate(glm::ivec2 point) override;
	void Update(float deltaSeconds) override { _ring.Update(deltaSeconds); }
	/// The dialog draws the picture last, its ring over the controls around it
	[[nodiscard]] bool IsOnTop() const override { return GetRingOpening() > 0.0f; }

	[[nodiscard]] int GetSymbol() const noexcept { return _symbol; }
	[[nodiscard]] bool IsRingOpen() const noexcept { return _ringOpen; }
	/// How far the ring has come up, 0 to 1
	[[nodiscard]] float GetRingOpening() const noexcept { return std::clamp(_ring.GetValue(), 0.0f, 1.0f); }
	/// The mouse button was let go, wherever it was: the ring goes
	void Release();
	std::function<void(int)> onChange;

	/// Where a symbol sits around the picture once the ring is up
	[[nodiscard]] DialogRect GetRingRect(int symbol) const;

private:
	/// The centre and half the size of a symbol on the ring, as far as it has come up
	[[nodiscard]] std::pair<glm::vec2, float> GetRingSymbol(int symbol, float opening) const;
	/// The symbol under a point, once the ring is all the way up
	[[nodiscard]] std::optional<int> RingSymbolAt(glm::ivec2 point) const;

	int _symbol;
	bool _ringOpen {false};
	Zoomer _ring;
	glm::ivec2 _pointer {0, 0};
};

} // namespace openblack::gui
