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

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Help/TipBubble.h"

namespace openblack::ecs::systems
{

/// The help's bubble over the "did you know" sign the player tapped last, showing its tip. It stays while the sign is
/// on screen and goes at once when it isn't, when the sign goes, or when the sign is tapped again. A script's cut
/// scene hides it without closing it.
class TipBubbleSystemInterface
{
public:
	virtual ~TipBubbleSystemInterface() = default;

	/// The bubble shows a sign's tip, from the top
	virtual void Show(entt::entity sign, uint32_t text) = 0;
	virtual void Hide() = 0;
	/// The sign the bubble shows the tip of, none while closed
	[[nodiscard]] virtual entt::entity GetSign() const = 0;
	[[nodiscard]] virtual uint32_t GetText() const = 0;

	/// Each game turn: kept up while its sign is on screen, else closed
	virtual void ProcessTurn() = 0;
	/// Each frame, with the frame's game milliseconds: its time to show runs down while it is up
	virtual void UpdateFrame(float gameMilliseconds) = 0;
	/// The pointer is over the bubble: it keeps showing
	virtual void Hover() = 0;
	/// Drawn this frame with the pointer inside it: how far along the view it stands, for the interface's pick to weigh
	virtual void SetUnderPointer(std::optional<float> depth) = 0;
	[[nodiscard]] virtual std::optional<float> GetUnderPointer() const = 0;
	/// Whether the hand was on the bubble, the pick giving it the cursor before anything under it, at the last frame
	/// drawn
	virtual void SetUnderHand(bool underHand) = 0;
	[[nodiscard]] virtual bool IsUnderHand() const = 0;
	/// The Action button pressed with an empty hand on the bubble grips it, and its words follow the hand until the
	/// button is let go
	virtual void Grip() = 0;
	virtual void LetGo() = 0;
	[[nodiscard]] virtual bool IsGripped() const = 0;

	/// Whether it is up and not hidden by a script's cut scene
	[[nodiscard]] virtual bool IsUp() const = 0;
	/// What is left of its time to show, its alpha being at most 1
	[[nodiscard]] virtual float GetDisplayTime() const = 0;
	/// The point over the sign it points at, none while closed
	[[nodiscard]] virtual std::optional<glm::vec3> GetAnchor() const = 0;
	[[nodiscard]] virtual help::tip_bubble::Scroll& GetScroll() = 0;
	[[nodiscard]] virtual const help::tip_bubble::Scroll& GetScroll() const = 0;

	/// As a new land opens
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
