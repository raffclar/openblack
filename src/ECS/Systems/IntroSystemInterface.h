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

#include "ECS/IntroRules.h"

namespace openblack::ecs::systems
{

/// The opening's arrival of the god and the boy's rescue, which the opening's script plays: the light that falls from
/// the sky onto the boy in the sea, the camera chasing it, and the god's hand lifting him out and setting him down
/// beside his parents, the boy held in its grip
class IntroSystemInterface
{
public:
	/// Where the camera is drawn from and looks at while it chases the light
	struct CameraView
	{
		glm::vec3 origin;
		glm::vec3 focus;
	};

	/// What the inspector shows
	struct State
	{
		/// None, the light falling (0), the hand lifting the boy (12) or setting him down (4)
		int32_t stage {-1};
		std::optional<intro_rules::light::State> light;
		std::optional<glm::vec3> lightHead;
		int32_t lightElapsed {0};
		bool cameraChasing {false};
		entt::entity hand {entt::null};
		int32_t handClipTime {0};
		bool handPlaying {false};
		bool holdingBoy {false};
		std::optional<glm::vec3> grip;
		bool liftFinished {false};
	};

	virtual ~IntroSystemInterface() = default;

	/// A script plays one of the opening's specials: 0 the light falls, 1 the camera chases it, 2 the camera is the
	/// script's again, 4 the hand stands ready to set the boy down, 5 it sets him down holding him, and again to let him
	/// go. The others aren't this system's.
	virtual void Play(int32_t special) = 0;
	/// All of it goes at once: the light, the hand, the chasing camera
	virtual void ReleaseAll() = 0;
	/// Whether the lifting hand's clip has come round, which it never does as it plays once
	[[nodiscard]] virtual bool HasLiftFinished() const = 0;

	/// Each drawn frame, by its game milliseconds: the light falls or fades, the hand's clip plays on and those that
	/// follow the hand are drawn in its grip
	virtual void Update(uint32_t milliseconds) = 0;

	/// The camera's view while it chases the light, which is drawn in place of the script's
	[[nodiscard]] virtual std::optional<CameraView> GetCameraView() const = 0;
	/// What the light draws this frame, none while there is no light
	[[nodiscard]] virtual const intro_rules::light::Frame* GetLightFrame() const = 0;
	/// Where the light is sorted among what blends: its head
	[[nodiscard]] virtual glm::vec3 GetLightSortPoint() const = 0;

	[[nodiscard]] virtual State GetState() const = 0;
};

} // namespace openblack::ecs::systems
