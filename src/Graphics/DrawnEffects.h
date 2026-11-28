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

#include <map>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/InfluenceCircle.h"
#include "3D/Lightning.h"
#include "3D/OrientedText.h"
#include "3D/Rain.h"
#include "3D/Snowfall.h"
#include "3D/WaterRings.h"
#include "Animals/FishShoal.h"
#include "Creature/LeashRope.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/DestructionGhost.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Transform.h"
#include "ECS/HighDetailRules.h"
#include "ECS/IntroRules.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/CloudSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/VillagerEyes.h"
#include "Enums.h"
#include "Gestures/GestureHints.h"
#include "Particles/ParticleDrawFrame.h"
#include "Particles/ParticleSymbolLayout.h"

namespace openblack::graphics
{
class Texture2D;

/// What the effects of the frame draw, as the steps of the frame made it before the draw. The renderer fills it on the
/// game's thread. The passes read only this for these effects, not the world.
struct DrawnEffects
{
	/// A corner of a flat effect on the land: where it is, its place on the texture and its colour, a byte each of red,
	/// green, blue and alpha from the lowest
	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		uint32_t colour;
	};

	/// What the symbols of belief of one village centre draw in this frame
	struct TownBelief
	{
		entt::entity centre {entt::null};
		particles::draw::Frame frame;
		/// The run of the symbols in `townBeliefSymbols`, when the effect draws only symbols. Then the frame is clear.
		std::optional<uint32_t> symbolRun;
		/// The lowest and the highest corners of a box that holds every corner of the frame's sprites. It is empty when
		/// the frame has more than sprites.
		std::optional<std::pair<glm::vec3, glm::vec3>> spriteBox;
	};

	/// The glow shell over the heart of a temple that the game destroys
	struct CreedShell
	{
		particles::draw::ShellDraw shell;
		/// The centre of the heart
		glm::vec3 position;
		glm::mat4 model;
	};

	/// The collar of a leash post
	struct LeashCollar
	{
		glm::vec3 position;
		glm::mat4 model;
		float scroll;
		LeashType type;
	};

	/// A mist of the land or a cloud of the sky, and where it is
	struct PlacedMist
	{
		ecs::components::Mist mist;
		glm::vec3 position;
	};

	/// The eyes of a villager that a script draws in high detail
	struct VillagerEyes
	{
		ecs::high_detail_rules::Face face;
		/// The mesh of the villager, whose head skin the lids show
		entt::id_type mesh;
		ecs::villager_eyes::DrawnEyes eyes;
	};

	/// A miracle's globe that the draw list of the land shows
	struct Globe
	{
		ecs::components::OneOffSpellSeed globe;
		ecs::components::Transform transform;
	};

	/// A town's new miracle over its opened chest
	struct RewardSeed
	{
		ecs::components::RewardSeed shown;
		/// The alignment of the chest's player, or 0 when nobody owns it
		float alignment;
		int32_t turnsLeft;
	};

	/// The seed of an icon with the player that it is for
	struct IconSeed
	{
		ecs::components::IconSeed shown;
		PlayerNames player;
		float alignment;
		/// The colour of the player, which the rings of a power-up show
		uint32_t colour;
	};

	/// A run of the vertices of the read-out over one house, and its place among what blends
	struct KnockRow
	{
		uint32_t firstVertex;
		uint32_t vertexCount;
		uint32_t depth;
	};

	/// A column of light: where it stands and its colour and alpha
	struct LightColumn
	{
		glm::vec3 position;
		glm::vec4 tint;
	};

	/// A village centre in the order of the draw, with its column when its column shows
	struct TownCentre
	{
		entt::entity centre {entt::null};
		std::optional<LightColumn> column;
	};

	/// What the particle effects draw in this frame
	particles::draw::Frame particles;
	/// The frames of the village centres' symbols of belief. Only the first `townBeliefCount` are of this frame. The
	/// others keep their memory for the next frames.
	std::vector<TownBelief> townBeliefs;
	size_t townBeliefCount {0};
	/// The symbols of belief of the centres whose effects draw only symbols, all in one layout, with all their sprites
	particles::draw::SymbolLayout townBeliefSymbols;
	/// The blobs of shadow under the feet of the villagers and of the animals, for the main view
	std::vector<Vertex> groundBlobs;
	/// The creatures' footprints on the land, for the main view
	std::vector<Vertex> footprints;
	/// The ropes of the creatures' leashes that the frame shows, in the order of the draw
	std::vector<leash_rope::Rope> leashes;
	std::vector<CreedShell> creedShells;
	std::vector<LeashCollar> leashCollars;
	/// The rings on the water
	std::vector<water_rings::Ring> waterRings;
	/// The clouds of the storms
	std::vector<ecs::systems::StormCloud> stormClouds;
	/// The magic shields' domes, between their last two turns, when the game has magic shields
	std::vector<ecs::systems::MagicShieldSystemInterface::DomeDraw> shieldDomes;
	bool shieldsShown {false};
	/// The ambient wind, which moves the sea's texture
	glm::vec2 ambientWind {0.0f};
	/// The smokes of the chimneys that are not out, in the order of the registry
	std::vector<ecs::components::ChimneySmoke> chimneySmokes;
	/// The mists of the land, in the order of the registry
	std::vector<PlacedMist> landMists;
	/// The clouds of the sky, in the order of the drawn registry. There are none when the clouds do not show.
	std::vector<PlacedMist> skyClouds;
	/// The eyes of the villagers in high detail, in the order of the registry
	std::vector<VillagerEyes> villagerEyes;
	/// The flashes of lightning of the storms that show, in the order of the registry
	std::vector<lightning::Flash> lightningFlashes;
	/// The shoals of the fish farms that show, each with how opaque it is
	std::vector<std::pair<fish_shoal::Shoal, uint8_t>> farmShoals;
	/// The shoals of the fish puzzles whose fish are not caught. Each shows by its distance from the view.
	std::vector<fish_shoal::Shoal> puzzleShoals;
	/// The points of the floats of the nets of the fish puzzles
	std::vector<glm::vec3> puzzleFloats;
	/// The globes of the miracles, in the order of the registry
	std::vector<Globe> globes;
	/// The new miracles over the opened chests, in the order of the registry
	std::vector<RewardSeed> rewardSeeds;
	/// The seeds of the icons that show, in the order of the registry
	std::vector<IconSeed> iconSeeds;
	/// The little people over the houses of a knocked town, and the markers of the houses with a resident in a hand, for
	/// the main view
	std::vector<Vertex> knockVertices;
	std::vector<KnockRow> knockRows;
	/// The village centres, the newest first, as the game draws their columns and their symbols of belief
	std::vector<TownCentre> townCentres;
	/// The same centres only, for the draw of the symbols of belief
	std::vector<entt::entity> townCentreEntities;
	/// The game shows the borders of the players' influence, and the columns of light with them
	bool influenceShown {false};
	/// The circles of the borders that show, when the border shows from the camera's height or on the temple's map
	std::vector<influence::Circle> borderCircles;
	/// How far the texture of the border moved
	glm::vec2 borderScroll {0.0f};
	/// The ripples that the hand makes when it goes across a border
	std::vector<influence::Ripple> borderRipples;
	/// The columns over the temples that show
	std::vector<LightColumn> templeColumns;
	/// The beams of the started scrolls that add a colour in this frame. The beams fade by their distance bands.
	std::vector<LightColumn> scrollBeams;
	/// The pulse models of the charging icons
	std::vector<ecs::systems::SpellIconSystemInterface::ChargePulse> iconPulses;
	/// The ghosts of the buildings that the game destroyed, when the game has explosions
	std::vector<ecs::components::DestructionGhost> destructionGhosts;
	bool ghostsShown {false};
	/// The walls under each vortex that write the depth only
	std::vector<glm::mat4> vortexWalls;
	/// The blocks of rain that the main view shows, from the far blocks to the near blocks
	std::vector<rain::Tile> rainTiles;
	/// A copy of the streaks that each block of rain shows, as many as the block with the most streaks
	std::vector<rain::Streak> rainStreaks;
	/// How high the streaks reach over the ground
	float rainHeight {0.0f};
	/// The quarters of the blocks with falling snow that the main view shows, from the far quarters to the near quarters
	std::vector<snowfall::Tile> snowTiles;
	/// A copy of the flakes that each quarter shows, as many as the quarter with the most flakes
	std::vector<snowfall::Flake> snowFlakes;
	/// How far the beat of the highlights of the scrolls is, from 0 to 1, when the game has the highlights. The models
	/// of the particles that face the camera beat with it.
	std::optional<float> highlightPulseShare;
	/// The words over the land while the game pauses, and their texture, when the game has them
	std::vector<OrientedTextVertex> pauseText;
	const Texture2D* pauseTextTexture {nullptr};
	/// The words of the tribal power over the land, and their texture, when the game has them
	std::vector<OrientedTextVertex> tribalPowerText;
	const Texture2D* tribalPowerTexture {nullptr};
	/// What the light of the game's start draws, while there is a light
	std::optional<ecs::intro_rules::light::Frame> introLight;
	/// The point by which the light of the game's start sorts among the things that blend
	glm::vec3 introLightSortPoint {0.0f};
	/// The advisors and the trails of their smoke, as the advisors drew them in the steps of the frame
	std::vector<ecs::systems::AdvisorSystemInterface::Draw> advisorDraws;
	std::vector<ecs::systems::AdvisorSystemInterface::TrailVertex> advisorTrails;
	/// The row of the hints of the gestures, when the game has gestures
	std::optional<gesture_hints::Row> gestureHints;
	/// What the effect of the holder of each pictured seed of the row draws, by miracle, for the hints that show
	std::map<MagicType, particles::draw::Frame> hintEffects;

	[[nodiscard]] std::span<const TownBelief> TownBeliefs() const
	{
		return std::span<const TownBelief>(townBeliefs).first(townBeliefCount);
	}
};

} // namespace openblack::graphics
