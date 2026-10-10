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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{

/// The totems on the town centres (components::VillageTotem): they rise with the share of their town's people who
/// worship, which the player sets by taking hold of one with the hand and sliding it up or down
class VillageTotemSystemInterface
{
public:
	virtual ~VillageTotemSystemInterface() = default;

	/// A totem joins its town's player, when it is made and when the town changes hands: the player's icon goes on
	/// top, it turns to face the town's worship site, and the town's worship share is set to none
	virtual void AddToPlayer(entt::entity totem) = 0;
	/// A town's share of its people at worship is set: always none without a worship site; otherwise its totem eases
	/// there
	virtual void SetTownShare(entt::entity town, float share) = 0;
	/// Each frame: the totems ease on by the game's milliseconds, sound the bell as they arrive, and stand at their
	/// share, or at the share the hand holds one at
	virtual void Update(float gameMilliseconds) = 0;

	/// The totem a picked plinth or icon belongs to
	[[nodiscard]] virtual std::optional<entt::entity> TotemOf(entt::entity picked) const = 0;
	/// The hand takes hold of a totem: only one of the player's own towns, standing built, with the player's temple and
	/// the town's worship site built. Whether it took hold.
	virtual bool Grip(entt::entity totem, PlayerNames player) = 0;
	/// The mouse moved up the screen by some pixels this frame while the hand holds a totem: the hand slides it
	virtual void Slide(float upPixels, float screenHeight) = 0;
	/// The hand lets go: the town's share is set to where it was left
	virtual void LetGo() = 0;
	/// The totem the hand holds, if any
	[[nodiscard]] virtual std::optional<entt::entity> GetGripped() const = 0;
	/// How the hand holds a totem: where it is, how far it tips, and how far it closes round the icon
	struct HandHold
	{
		glm::vec3 position {0.0f};
		float tilt {0.0f};
		float closure {0.0f};
	};
	[[nodiscard]] virtual std::optional<HandHold> GetHandHold() const = 0;
};

} // namespace openblack::ecs::systems
