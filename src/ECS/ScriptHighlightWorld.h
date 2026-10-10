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
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/Sprite.h"
#include "ECS/ScriptHighlightRules.h"
#include "Enums.h"

namespace openblack
{
enum class MeshId : uint32_t;
}

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::script_highlights
{

/// A highlight kind's row of the info table: its models and effects
struct KindInfo
{
	MeshId normal;
	MeshId active;
	ParticleType glints;
	ParticleType activeEffect;
};

/// A model's box, about its own origin
struct ModelBox
{
	glm::vec3 centre {0.0f};
	glm::vec3 halfSize {0.0f};
};

/// What the highlights need of the game: its things, the info table, the models, the land and what stands on it, the
/// particle effects, the sounds and the help system. The game gives the real one; tests give a world of their own.
class ScriptHighlightWorldInterface
{
public:
	virtual ~ScriptHighlightWorldInterface() = default;

	[[nodiscard]] virtual Registry& Entities() = 0;
	/// A kind's row of the info table, none for a number past the table
	[[nodiscard]] virtual std::optional<KindInfo> InfoOf(uint32_t kind) const = 0;
	/// A model's box, none for a model the game doesn't have
	[[nodiscard]] virtual std::optional<ModelBox> BoxOf(MeshId mesh) const = 0;
	/// The land's height at a point across it
	[[nodiscard]] virtual float LandHeight(glm::vec2 point) const = 0;
	/// The other things in the map cell of a point, as a highlight there weighs standing on them
	[[nodiscard]] virtual std::vector<ThingBelow> ThingsBelow(entt::entity highlight, glm::vec3 point) const = 0;
	/// The game's turn, and its length
	[[nodiscard]] virtual uint32_t Turn() const = 0;
	[[nodiscard]] virtual float MillisecondsPerTurn() const = 0;
	/// The scripts' switch that shows the scrolls; the signs always show
	[[nodiscard]] virtual bool ScrollsDrawn() const = 0;

	/// A particle effect of a type at a point, at full strength; 0 for none
	virtual uint32_t StartEffect(ParticleType type, glm::vec3 at) = 0;
	/// The effect is handed the highlight as its target
	virtual void TargetEffect(uint32_t effect, entt::entity target) = 0;
	/// A frame's step of the effect, which is stepped as it is drawn
	virtual void StepEffect(uint32_t effect, float seconds) = 0;
	virtual void MoveEffect(uint32_t effect, glm::vec3 to) = 0;
	virtual void DeleteEffect(uint32_t effect) = 0;

	/// A sound of the game's own bank, for the player, not placed in the world, unless the game's sounds are off
	virtual void PlaySound(uint32_t sample) = 0;
	/// The glow's sprite, none when it can't be drawn
	[[nodiscard]] virtual std::optional<components::Sprite> GlowLook() const = 0;

	/// The help system hears of a tap
	virtual void HelpEvent(uint32_t event) = 0;
	/// A help script starts, unless another script holds the dialogue (a help script holding it is stopped first)
	virtual void StartHelpScript(std::string_view name) = 0;
	/// The help system's bubble shows a tip of a sign, or shows none
	virtual void ShowTip(entt::entity sign, uint32_t text, uint32_t category) = 0;
	virtual void HideTip() = 0;
	/// The sign whose tip the bubble shows, none while it is closed (it closes by itself once its sign is off screen)
	[[nodiscard]] virtual entt::entity TipShown() const = 0;
	/// A challenge's last message plays again, unless another script holds the dialogue (a help script holding it is
	/// stopped first)
	virtual void ReplayChallenge(uint32_t challenge) = 0;
};

} // namespace openblack::ecs::script_highlights
