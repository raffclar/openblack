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

#include <array>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/TattooMenu.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "Graphics/CreatureReflection.h"

namespace openblack::graphics
{
class L3DMesh;
class Texture2D;

/// What the creatures' pass draws, as the steps of the frame left the creatures before the draw. The renderer fills it on
/// the game's thread. The pass reads only this for the creatures, not the world.
struct DrawnCreatures
{
	/// The meshes that blend a creature's body to its shape, how much of each, and the skins that it shows
	struct Shape
	{
		std::array<const L3DMesh*, 3> meshes {};
		glm::vec3 weights {0.0f};
		std::span<const std::pair<uint32_t, const Texture2D*>> skins;
		bool blendSeams {true};
	};

	/// How far a creature fizzed or froze, and where the static over its skin is
	struct Spells
	{
		float fizz {0.0f};
		float freeze {0.0f};
		glm::vec2 staticScroll {0.0f};
	};

	/// A creature that the frame draws, or a copy of a creature that the frame draws in its own pose
	struct Creature
	{
		entt::entity entity {entt::null};
		uint32_t instance {0};
		/// The thing that gives the pose, the eyes and the hair of a copy. The copy is a thing of the temple's rooms.
		entt::entity posedBy {entt::null};
		std::optional<glm::vec3> light;
		/// The bones of its pose, or those of its mesh when it has no pose of its own. After the fill, they are the copy
		/// of the bones that the texture of the posed bones holds.
		std::span<const glm::mat4> bones {};
		/// Where its bones start in the texture of the posed bones
		std::optional<uint32_t> firstBone {};

		/// The pass draws the creature. It does not when its mesh is not loaded or its poser is not valid.
		bool drawn {false};
		entt::id_type mesh {0};
		std::optional<Shape> shape;
		/// The word that tells whether the body shows the environment map. A copy has none.
		uint32_t environmentWord {0};
		std::optional<Spells> spells;
		std::optional<ecs::components::CreatureEyes> eyes;
		std::optional<ecs::components::CreatureHair> hair;
		/// Where the hair hangs from: the body where the frame draws it
		glm::vec3 hairOrigin {0.0f};
		std::optional<ecs::components::CreatureHandGlows> handGlows;
		/// Where the creature or the copy is, when it has a place
		std::optional<glm::vec3> position;
		/// What the game knows when it decides to mirror the body in the sea, except the height over the water. It is
		/// empty when the game cannot mirror the body.
		std::optional<creature_reflection::Situation> mirror;

		/// Whether the sea mirrors the body when the body stands this high over the water
		[[nodiscard]] bool MirroredAt(float height) const
		{
			if (!mirror.has_value())
			{
				return false;
			}
			auto situation = *mirror;
			situation.height = height;
			return creature_reflection::Shown(situation);
		}
	};

	/// The body of a creature that the interface shows over itself, moved to its place before the interface's camera
	struct InterfaceBody
	{
		entt::id_type mesh {0};
		std::vector<glm::mat4> bones;
		std::optional<uint32_t> firstBone;
		std::optional<Shape> shape;
		std::optional<ecs::components::CreatureEyes> eyes;
	};

	/// The creatures that the interface shows over itself in this frame
	struct Interface
	{
		glm::mat4 view {1.0f};
		glm::mat4 onTarget {1.0f};
		glm::vec2 viewOrigin {0.0f};
		glm::vec2 viewSize {0.0f};
		/// The place of each body, in the order of the bodies
		std::vector<glm::mat4> models;
		std::vector<InterfaceBody> bodies;
	};

	/// The creature that the main menu's tattoo editor shows in this frame
	struct Menu
	{
		glm::mat4 view {1.0f};
		glm::mat4 onTarget {1.0f};
		glm::vec2 viewOrigin {0.0f};
		glm::vec2 viewSize {0.0f};
		glm::mat4 model {1.0f};
		entt::id_type mesh {0};
		std::vector<glm::mat4> bones;
		std::optional<Shape> shape;
		std::optional<ecs::components::CreatureEyes> eyes;
		std::optional<ecs::components::CreatureHair> hair;
		glm::vec3 position {0.0f};
		/// The parts of the creature in the order of the draw
		std::vector<tattoo_menu::Layer> layers;
	};

	std::vector<Creature> creatures;
	/// The interface's creatures. They are empty when the interface does not show creatures in this frame.
	std::optional<Interface> interfaceCreatures;
	/// The tattoo editor's creature. It is empty when the editor does not show the creature in this frame.
	std::optional<Menu> menuCreature;
	/// The game shows the creatures' hair
	bool hairShown {false};
	/// Where the static is that all the creatures' eyes share while they fizz, when the game has it
	std::optional<glm::vec2> eyeStaticScroll;
};

} // namespace openblack::graphics
