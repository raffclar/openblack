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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/ScriptHighlightRules.h"

namespace openblack::ecs::systems
{

/// The scrolls and signs scripts put up to mark challenges and give tips: how they stand, spin, glint and glow, start,
/// and answer the hand's tap; the beat they pulse to together, and the tips the player has read
class ScriptHighlightSystemInterface
{
public:
	virtual ~ScriptHighlightSystemInterface() = default;

	/// A highlight of a kind at a point, belonging to a challenge; none for a kind past the info table
	virtual entt::entity Create(uint32_t kind, glm::vec3 at, uint32_t challenge) = 0;
	/// A script gives a highlight its text and category
	virtual void SetProperties(entt::entity highlight, uint32_t text, uint32_t category) = 0;
	/// It starts or stops: its model, its active effect and, as a scroll starts, its sound
	virtual void SetActive(entt::entity highlight, bool active) = 0;
	/// A script sets the height it stands at above the land
	virtual void SetDrawHeight(entt::entity highlight, float height) = 0;
	/// The hand taps it; false when the tap does nothing
	virtual bool Tap(entt::entity highlight, bool byThisPlayer) = 0;

	/// A turn: the beat moves on, and each highlight finds the height it stands at; a sign whose tip has been read starts
	virtual void ProcessTurn() = 0;
	/// A frame drawn, of so many milliseconds of game time, from a camera: the highlights spin, scale, glow and glint
	virtual void UpdateFrame(float frameMilliseconds, float turnFraction, glm::vec3 camera) = 0;
	/// A new land: the beat starts again
	virtual void Reset() = 0;

	[[nodiscard]] virtual const script_highlights::Pulse& GetPulse() const = 0;
	[[nodiscard]] virtual const script_highlights::TipsRead& GetTipsRead() const = 0;
};

} // namespace openblack::ecs::systems
