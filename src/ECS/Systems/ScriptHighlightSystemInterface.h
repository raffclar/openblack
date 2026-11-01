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

#include <chrono>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/ScriptHighlightRules.h"
#include "ECS/Systems/SavedStateInterface.h"

namespace openblack::ecs::systems
{

/// The values of a highlight in a saved game
struct SavedHighlight
{
	uint32_t kind {0};
	glm::vec3 at {0.0f};
	/// The challenge or the text of a tip, and the category of a tip
	uint32_t text {0};
	uint32_t category {0};
	bool active {false};
	/// The height above the land that a script set, if a script set one
	std::optional<float> drawHeight;
};

/// The scrolls and signs scripts put up to mark challenges and give tips: how they stand, spin, glint and glow, start,
/// and answer the hand's tap; the beat they pulse to together, and the tips the player has read
class ScriptHighlightSystemInterface: public SavedStateInterface
{
public:
	virtual ~ScriptHighlightSystemInterface() = default;

	/// A highlight of a kind at a point, belonging to a challenge; none for a kind past the info table
	virtual entt::entity Create(uint32_t kind, glm::vec3 at, uint32_t challenge) = 0;
	/// A highlight of a saved game. It takes its saved values before its creation step, as a load does. So an active
	/// scroll shows its active model, but plays no sound and has no active effect. None for a kind past the info table.
	virtual entt::entity Restore(const SavedHighlight& saved) = 0;
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
	/// A frame drawn, of so many milliseconds of game time, from a camera: the highlights spin, scale, glow and glint.
	/// Their effects step no more often than 60 times a second, each step by all the game time since the last, and only
	/// in a frame that draws the land.
	virtual void UpdateFrame(float frameMilliseconds, float turnFraction, glm::vec3 camera,
	                         std::chrono::milliseconds frameRealTime, bool landDrawn) = 0;
	/// A new land: the beat starts again
	virtual void Reset() = 0;

	[[nodiscard]] virtual const script_highlights::Pulse& GetPulse() const = 0;
	[[nodiscard]] virtual const script_highlights::TipsRead& GetTipsRead() const = 0;
};

} // namespace openblack::ecs::systems
