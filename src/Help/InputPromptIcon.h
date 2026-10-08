/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Graphics/InputPromptFrame.h"

// The InputPromptIcon: a mouse button or key picture with a text, faded in real time and drawn every frame
// (Renderer::DrawKeyOrMouse). Creating one links it at the head of the list, destroying it unlinks it, and the frame
// loop updates each with the real frame time, head first. Made by the tooltip (help::tooltips), the help system's
// click cue and the $M control icon.
namespace openblack::help::input_prompt
{

/// openblack's name of an InputPromptIcon (the original keeps the pointer); 0 is none
using Handle = uint32_t;
constexpr Handle k_None = 0;

/// A colour {b, g, r, a} as ARGB (0xAARRGGBB)
constexpr uint32_t k_White = 0xFFFFFFFFu;
/// {b 0, g 0xFF, r 0xFF, a 0xFF}: the tooltip's and the click cue's c1
constexpr uint32_t k_Yellow = 0xFFFFFF00u;

/// The InputPromptIcon's fields, only those used
struct InputPromptIcon
{
	Handle id {k_None}; ///< (openblack) its name, for Get / Destroy
	float start {0.0f};
	float target {1.0f};
	float duration {0.0f};  ///< real seconds
	float current {0.0f};   ///< = start when created
	float elapsed {0.0f};   ///< = 0 when created
	int32_t animType {0};   ///< CH_ANIMTYPE: -1 a key; 0..3 the mouse button's blink
	int32_t clickType {0};  ///< the mouse click type, or a key code; 0 with a key name
	int32_t row {3};        ///< the mouse row (MouseRow: 0, 1, 2; 3 = no icon), or a key code
	std::u16string keyName; ///< (animType -1, clickType 0): the key's name. (approximate) a copy: the original keeps
	                        ///< the pointer to a shared buffer, which every key resolve rewrites
	std::u16string text;    ///< a buffer of 0x80 characters in the original (L"" for a null text)
	int32_t x {0};          ///< (an offset from the hand with align & 6 == 6)
	int32_t y {0};          ///< (the box's vertical centre, or an offset from the hand)
	int32_t size {0};       ///< S in pixels
	uint32_t align {0};     ///< KEYALIGN
	uint32_t c1 {k_White};  ///< ARGB; its alpha is rewritten every frame
	uint32_t c2 {k_White};  ///< ARGB; its alpha is rewritten every frame
	int32_t alpha8 {0};     ///< the panels' alpha (0: no panels)
	/// (openblack) a = Alpha() after the frame's update `pass` (0: from the help system's draw, 1: from the help
	/// text's callback), what each of the two draws uses
	std::array<float, 2> passAlpha {0.0f, 0.0f};
};

/// The creation's 14 arguments: (start, target, duration, animType, clickType, row, text, x, y, S, align, c1*, c2*,
/// alpha8). A null text, c1 or c2 is std::nullopt
struct Desc
{
	float start {0.0f};
	float target {1.0f};
	float duration {0.0f};
	int32_t animType {0};
	int32_t clickType {0};
	int32_t row {3};
	std::u16string keyName;
	std::optional<std::u16string> text;
	int32_t x {0};
	int32_t y {0};
	int32_t size {0};
	uint32_t align {0};
	std::optional<uint32_t> c1;
	std::optional<uint32_t> c2;
	int32_t alpha8 {0};
};

/// A new InputPromptIcon at the head of the list
Handle Create(const Desc& desc);
/// Unlinks and deletes it; k_None or an unknown handle does nothing
void Destroy(Handle handle);
/// The InputPromptIcon, nullptr when it does not exist (the callers' test). The pointer is valid until the
/// next Create or Destroy
[[nodiscard]] InputPromptIcon* Get(Handle handle);

/// The update of every icon, head first, with the real frame time in seconds. The original runs it twice a frame with
/// the same dt, at the end of the help system's draw and of the help text's end of frame callback: call it with pass
/// 0, then pass 1; each records passAlpha.
/// Its draw half is Renderer::DrawInputPrompts over the frame's copy (Snapshot)
void Frame(float realSeconds, size_t pass);
/// a = clamp(current, 0, 1), the factor of the text, the icon and the panels
[[nodiscard]] float Alpha(const InputPromptIcon& icon);
/// The list in its order (the newest first, as the original walks it)
[[nodiscard]] std::span<const InputPromptIcon> All();
/// The list copied for the draw (FillOverlayFrame -> OverlayFrame::inputPrompts.icons), in the same order
void Snapshot(std::vector<graphics::InputPromptDraw>& out);

/// The picture of a BINDABLE_ACTION. False for -1 and 0x21
/// and for an action with no binding (the caller's own fallback). A mouse binding gives animType 0, its click type and
/// the mouse row; a key gives animType -1, clickType 0 and its name in keyName
bool ResolveAction(int32_t action, int32_t& animType, int32_t& clickType, int32_t& row, std::u16string& keyName);
/// The mouse row, 2 with a wheel, 1 with more than 2 buttons, else 0
[[nodiscard]] int32_t MouseRow();

} // namespace openblack::help::input_prompt
