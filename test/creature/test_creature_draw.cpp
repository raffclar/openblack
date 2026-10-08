/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the renderer draws of a creature and of its leash, made from the components alone: the bodies with their pose
// and shape, the eyes, the hair ribbons, the painted skins, and the leash's rope and shadow

#include <cstdint>

#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "3D/CreatureBody.h"
#include "Creature/CreatureHair.h"
#include "Creature/LeashRope.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/CreatureDraw.h"
#include "Graphics/LeashDraw.h"
#include "Graphics/RenderModes.h"
#include "Graphics/WorldTriangles.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::graphics;
using Appearance = creature::CreatureBody::Appearance;
using creature_draw::EntityInstances;

namespace
{
constexpr float k_Tolerance = 1e-5f;

size_t StorageCount(const Registry& registry)
{
	size_t storages = 0;
	registry.EachStorage([&storages](entt::id_type, const auto&) { ++storages; });
	return storages;
}

float Flat(glm::vec2 /*point*/)
{
	return 2.0f;
}

uint32_t AlphaOf(uint32_t abgr)
{
	return abgr >> 24u;
}

// A rope from a hand at (0, 20, 0) to a collar 30 along x, placed straight
leash_rope::Rope StraightRope()
{
	return leash_rope::Create({0.0f, 20.0f, 0.0f}, {30.0f, 20.0f, 0.0f}, 32.5f, 77.0f, {});
}

// A creature of a species with its mesh drawn in a row of the instances
entt::entity AddCreature(Registry& registry, EntityInstances& instances, CreatureType species, uint32_t row)
{
	const auto entity = registry.Create();
	const auto meshId = creature::GetIdFromType(species, Appearance::Base);
	registry.AssignState<Creature>(entity, Creature {.owner = PlayerNames::PLAYER_ONE, .species = species, .mind = 0});
	registry.AssignState<components::Mesh>(entity, components::Mesh {meshId, 0, 0});
	instances.emplace(entity, ecs::systems::RenderContext::EntityInstance {
	                              .meshId = meshId, .index = row, .morphWithTerrain = false, .receivesDynamicShadow = true});
	return entity;
}
} // namespace

// ---- The leash ----------------------------------------------------------------------------------------------------

TEST(LeashDraw, TheRopeTakesTheLightAtEachCornerAndIsOpaque)
{
	const auto rope = StraightRope();
	const glm::vec3 eye {15.0f, 60.0f, -40.0f};
	const auto draw =
	    leash_draw::Build(rope, eye, Flat, [](const glm::vec3& point) { return point.x < 15.0f ? 0x12336699u : 0x00102030u; });
	const auto ribbon = leash_rope::BuildRibbon(rope, eye, Flat);
	for (size_t i = 0; i < draw.rope.size(); ++i)
	{
		const auto& corner = draw.rope.at(i);
		EXPECT_EQ(corner.position, ribbon.rope.at(i).position);
		EXPECT_EQ(corner.uv, ribbon.rope.at(i).uv);
		// the light's own alpha is not used: the rope is fully opaque
		const uint32_t expected = corner.position.x < 15.0f ? 0xFF336699u : 0xFF102030u;
		EXPECT_EQ(corner.abgr, world_triangles::ToAbgr(expected));
		EXPECT_EQ(corner.specular, 0u);
	}
}

TEST(LeashDraw, TheShadowIsBlackFaintAndFadesOutAtBothEnds)
{
	const auto rope = StraightRope();
	const auto draw = leash_draw::Build(rope, {15.0f, 60.0f, -40.0f}, Flat, [](const glm::vec3&) { return 0xFFFFFFFFu; });
	for (size_t i = 0; i < draw.shadow.size(); ++i)
	{
		const auto& corner = draw.shadow.at(i);
		const bool atEnd = i < 2 || i >= draw.shadow.size() - 2;
		EXPECT_EQ(AlphaOf(corner.abgr), atEnd ? 0u : leash_rope::k_ShadowAlpha);
		EXPECT_EQ(corner.abgr & 0x00FFFFFFu, 0u);
		EXPECT_NEAR(corner.position.y, 2.0f + leash_rope::k_ShadowLift, k_Tolerance);
	}
}

TEST(LeashDraw, TheRopeIsSortedByItsMiddlePoint)
{
	const auto rope = StraightRope();
	const auto draw = leash_draw::Build(rope, {0.0f, 50.0f, 0.0f}, Flat, [](const glm::vec3&) { return 0u; });
	EXPECT_EQ(draw.middle, leash_rope::Point(rope, leash_rope::k_PointCount / 2));
}

TEST(LeashDraw, TheRopeWritesDepthAndTheShadowDoesNot)
{
	EXPECT_TRUE(render_modes::Desc(leash_draw::k_RopeMaterial.mode).zWrite);
	EXPECT_TRUE(render_modes::Desc(leash_draw::k_RopeMaterial.mode).alphaTest);
	EXPECT_FALSE(render_modes::Desc(leash_draw::k_ShadowMaterial.mode).zWrite);
}

TEST(LeashDraw, OnlyAWornRopeThatHasBeenPlacedIsDrawn)
{
	CreatureLeash leash;
	EXPECT_FALSE(leash_draw::IsDrawn(leash));
	leash.worn.emplace();
	EXPECT_FALSE(leash_draw::IsDrawn(leash));
	leash.worn->ropeStarted = true;
	EXPECT_TRUE(leash_draw::IsDrawn(leash));
}

TEST(LeashDraw, TheRopesAreThoseDrawnAndNoneWhileHidden)
{
	Registry registry;
	const auto led = registry.Create();
	registry.AssignState<CreatureLeash>(led);
	registry.Get<CreatureLeash>(led).worn.emplace();
	registry.Get<CreatureLeash>(led).worn->ropeStarted = true;
	const auto free = registry.Create();
	registry.AssignState<CreatureLeash>(free);

	const auto ropes = leash_draw::Ropes(std::as_const(registry), true);
	ASSERT_EQ(ropes.size(), 1u);
	EXPECT_EQ(ropes.front(), &registry.Get<CreatureLeash>(led).worn->rope);
	EXPECT_TRUE(leash_draw::Ropes(std::as_const(registry), false).empty());
}

TEST(LeashDraw, ALandWithoutACreatureDrawsNoRopeAndGainsNoStorage)
{
	Registry registry;
	const auto entity = registry.Create();
	registry.AssignState<Transform>(entity, Transform {glm::vec3(1.0f), glm::mat3(1.0f), glm::vec3(1.0f)});
	const auto before = StorageCount(registry);
	EXPECT_TRUE(leash_draw::Ropes(std::as_const(registry), true).empty());
	EXPECT_EQ(StorageCount(registry), before);
}

// ---- The bodies ---------------------------------------------------------------------------------------------------

TEST(CreatureDrawBodies, ALandWithoutACreatureHasNoBodyAndGainsNoStorage)
{
	Registry registry;
	const auto entity = registry.Create();
	registry.AssignState<Transform>(entity, Transform {glm::vec3(1.0f), glm::mat3(1.0f), glm::vec3(1.0f)});
	const auto before = StorageCount(registry);
	const EntityInstances instances;
	const auto bodies =
	    creature_draw::Bodies(std::as_const(registry), instances, [](entt::id_type) { return std::optional<size_t>(4); });
	EXPECT_TRUE(bodies.empty());
	EXPECT_EQ(StorageCount(registry), before);
}

TEST(CreatureDrawBodies, OnlyCreaturesWithARowAndALoadedMeshAreDrawn)
{
	Registry registry;
	EntityInstances instances;
	const auto drawn = AddCreature(registry, instances, CreatureType::Lion, 7);
	AddCreature(registry, instances, CreatureType::Tiger, 3);
	// a creature with no row this frame
	const auto unseen = registry.Create();
	registry.AssignState<Creature>(unseen,
	                               Creature {.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Lion, .mind = 0});
	registry.AssignState<components::Mesh>(
	    unseen, components::Mesh {creature::GetIdFromType(CreatureType::Lion, Appearance::Base), 0, 0});

	const auto lion = creature::GetIdFromType(CreatureType::Lion, Appearance::Base);
	const auto bodies = creature_draw::Bodies(std::as_const(registry), instances, [lion](entt::id_type id) {
		return id == lion ? std::optional<size_t>(4) : std::nullopt;
	});
	ASSERT_EQ(bodies.size(), 1u);
	EXPECT_EQ(bodies.front().entity, drawn);
	EXPECT_EQ(bodies.front().instance, 7u);
	EXPECT_EQ(bodies.front().mesh, lion);
	// no animation and no shape yet: the rest pose and the base mesh alone
	EXPECT_TRUE(bodies.front().bones.empty());
	EXPECT_FALSE(bodies.front().morph.has_value());
}

TEST(CreatureDrawBodies, TheBodyTakesItsPoseOnlyWhenItFitsTheMesh)
{
	Registry registry;
	EntityInstances instances;
	const auto entity = AddCreature(registry, instances, CreatureType::Lion, 0);
	registry.AssignState<CreatureAnimation>(entity);
	auto& animation = registry.Get<CreatureAnimation>(entity);
	animation.boneMatrices.assign(4, glm::mat4(2.0f));

	const auto bonesOf = [](size_t count) { return [count](entt::id_type) { return std::optional<size_t>(count); }; };
	auto bodies = creature_draw::Bodies(std::as_const(registry), instances, bonesOf(4));
	ASSERT_EQ(bodies.size(), 1u);
	ASSERT_EQ(bodies.front().bones.size(), 4u);
	EXPECT_EQ(bodies.front().bones.data(), animation.boneMatrices.data());

	bodies = creature_draw::Bodies(std::as_const(registry), instances, bonesOf(5));
	ASSERT_EQ(bodies.size(), 1u);
	EXPECT_TRUE(bodies.front().bones.empty());
}

TEST(CreatureDrawBodies, TheShapeIsBlendedTowardsTheLoadedMeshesByHowFarAlongEachAxis)
{
	Registry registry;
	EntityInstances instances;
	const auto entity = AddCreature(registry, instances, CreatureType::Lion, 0);
	registry.AssignState<CreatureMorph>(
	    entity, CreatureMorph {.shownFatness = 0.5f, .drawn = {.evilGood = 0.5f, .thinFat = -0.25f, .weakStrong = -1.0f}});

	const auto id = [](Appearance appearance) { return creature::GetIdFromType(CreatureType::Lion, appearance); };
	// the good and weak meshes are loaded, the thin one isn't
	const auto bodies = creature_draw::Bodies(std::as_const(registry), instances, [&id](entt::id_type mesh) {
		const bool loaded = mesh == id(Appearance::Base) || mesh == id(Appearance::Good) || mesh == id(Appearance::Weak);
		return loaded ? std::optional<size_t>(4) : std::nullopt;
	});
	ASSERT_EQ(bodies.size(), 1u);
	ASSERT_TRUE(bodies.front().morph.has_value());
	const auto& morph = *bodies.front().morph;
	EXPECT_EQ(morph.meshes.at(0), id(Appearance::Good));
	EXPECT_EQ(morph.meshes.at(1), id(Appearance::Base));
	EXPECT_EQ(morph.meshes.at(2), id(Appearance::Weak));
	EXPECT_EQ(morph.weights, glm::vec3(0.5f, 0.25f, 1.0f));
}

TEST(CreatureDrawBodies, ABodyIsFoundByItsRow)
{
	Registry registry;
	EntityInstances instances;
	const auto second = AddCreature(registry, instances, CreatureType::Lion, 9);
	const auto first = AddCreature(registry, instances, CreatureType::Lion, 2);
	const auto bodies =
	    creature_draw::Bodies(std::as_const(registry), instances, [](entt::id_type) { return std::optional<size_t>(1); });
	ASSERT_EQ(bodies.size(), 2u);
	ASSERT_NE(creature_draw::Find(bodies, 2), nullptr);
	EXPECT_EQ(creature_draw::Find(bodies, 2)->entity, first);
	ASSERT_NE(creature_draw::Find(bodies, 9), nullptr);
	EXPECT_EQ(creature_draw::Find(bodies, 9)->entity, second);
	EXPECT_EQ(creature_draw::Find(bodies, 5), nullptr);
}

// ---- The eyes -----------------------------------------------------------------------------------------------------

TEST(CreatureDrawEyes, EachEyeballComesBeforeItsEyelidInTheSkinsColour)
{
	CreatureEyes eyes;
	eyes.drawn.at(0) = {.eyeball = glm::mat4(1.0f), .eyelid = glm::mat4(2.0f)};
	eyes.drawn.at(1) = {.eyeball = glm::mat4(3.0f), .eyelid = std::nullopt};
	eyes.lidColour = {1.0f, 0.5f, 0.0f};
	const auto parts = creature_draw::Eyes(eyes, [](entt::id_type) { return true; });
	ASSERT_EQ(parts.size(), 3u);
	EXPECT_EQ(parts.at(0).mesh, CreatureEyes::k_EyeballMeshId);
	EXPECT_FALSE(parts.at(0).tint.has_value());
	EXPECT_EQ(parts.at(1).mesh, CreatureEyes::k_EyelidMeshId);
	EXPECT_EQ(parts.at(1).model, glm::mat4(2.0f));
	EXPECT_EQ(parts.at(1).tint, std::optional<uint32_t>(0xFFFF8000u));
	EXPECT_EQ(parts.at(2).model, glm::mat4(3.0f));
}

TEST(CreatureDrawEyes, AnEyeMeshNotLoadedIsLeftOut)
{
	CreatureEyes eyes;
	eyes.drawn.at(0) = {.eyeball = glm::mat4(1.0f), .eyelid = glm::mat4(2.0f)};
	const auto parts = creature_draw::Eyes(eyes, [](entt::id_type id) { return id == CreatureEyes::k_EyelidMeshId; });
	ASSERT_EQ(parts.size(), 1u);
	EXPECT_EQ(parts.front().mesh, CreatureEyes::k_EyelidMeshId);
}

TEST(CreatureDrawEyes, AnEyelidsRowIsTheBodysWithItsTint)
{
	glm::mat4 body(1.0f);
	body[3] = glm::vec4(10.0f, 20.0f, 30.0f, 1.0f);
	body[0].w = 0.25f;
	const glm::vec4 colours {0.0f, 7.0f, 0.0f, 0.0f};
	const auto eyeball = creature_draw::EyeRow(body, colours, std::nullopt);
	EXPECT_EQ(eyeball.at(0), body[0]);
	EXPECT_EQ(eyeball.at(3), body[3]);
	EXPECT_EQ(eyeball.at(4), colours);

	const auto eyelid = creature_draw::EyeRow(body, colours, 0xFF804020u);
	glm::vec4 expected = colours;
	argb_colour::PackInstanceTint(expected, 0xFF804020u);
	EXPECT_EQ(eyelid.at(4), expected);
	EXPECT_EQ(eyelid.at(0), body[0]);
}

// ---- The hair -----------------------------------------------------------------------------------------------------

TEST(CreatureDrawHair, EachStrandIsARibbonInItsTuftsLitColour)
{
	CreatureHair hair;
	CreatureHair::Group textured {.colour = {200, 100, 50}, .halfWidth = 0.5f, .textured = true};
	textured.strands.push_back({.positions = {{0, 0, 0}, {0, 1, 0}, {0, 2, 0}}, .velocities = {}});
	textured.strands.push_back({.positions = {{1, 0, 0}, {1, 1, 0}}, .velocities = {}});
	CreatureHair::Group plain {.colour = {10, 20, 30}, .halfWidth = 0.25f, .textured = false};
	plain.strands.push_back({.positions = {{5, 0, 0}, {5, 1, 0}}, .velocities = {}});
	// a strand of one point has no ribbon
	plain.strands.push_back({.positions = {{6, 0, 0}}, .velocities = {}});
	hair.groups = {textured, plain};

	const glm::vec3 eye {0.0f, 1.0f, -10.0f};
	const glm::ivec3 light {128, 255, 255};
	const glm::ivec3 added {10, 0, 0};
	const auto drawn = creature_draw::BuildHair(hair, eye, light, added);

	ASSERT_EQ(drawn.textured.vertices.size(), 10u);
	ASSERT_EQ(drawn.textured.indices.size(), 18u);
	ASSERT_EQ(drawn.plain.vertices.size(), 4u);
	ASSERT_EQ(drawn.plain.indices.size(), 6u);
	// the second strand's triangles start at its own first corner
	EXPECT_EQ(drawn.textured.indices.at(12), 6u);

	const auto colour = creature_hair::StrandColour(textured.colour, light, added);
	const uint32_t argb = 0xFF000000u | (static_cast<uint32_t>(colour.r) << 16u) | (static_cast<uint32_t>(colour.g) << 8u) |
	                      static_cast<uint32_t>(colour.b);
	EXPECT_EQ(drawn.textured.vertices.front().abgr, world_triangles::ToAbgr(argb));

	std::vector<creature_hair::RibbonVertex> corners(6);
	creature_hair::BuildRibbon(textured.strands.front().positions, eye, textured.halfWidth, corners);
	for (size_t i = 0; i < corners.size(); ++i)
	{
		EXPECT_EQ(drawn.textured.vertices.at(i).position, corners.at(i).position);
		EXPECT_EQ(drawn.textured.vertices.at(i).uv, corners.at(i).uv);
	}
}

// ---- The painted skins --------------------------------------------------------------------------------------------

namespace
{
struct FakeTexture
{
	int uploads {0};
	uint16_t firstTexel {0};
};

struct SkinsFixture
{
	Registry registry;
	creature_draw::PaintedSkins<FakeTexture> skins;
	int made {0};
	std::optional<int> textureLimit;

	void Update()
	{
		skins.Update(
		    std::as_const(registry),
		    [this]() -> std::unique_ptr<FakeTexture> {
			    if (textureLimit.has_value() && made >= *textureLimit)
			    {
				    return nullptr;
			    }
			    ++made;
			    return std::make_unique<FakeTexture>();
		    },
		    [](FakeTexture& texture, std::span<const uint16_t> texels) {
			    ++texture.uploads;
			    texture.firstTexel = texels.empty() ? 0 : texels.front();
		    });
	}

	entt::entity Paint(std::vector<CreatureSkin::Skin> painted, uint32_t revision)
	{
		const auto entity = registry.Create();
		registry.AssignState<CreatureSkin>(entity);
		auto& skin = registry.Get<CreatureSkin>(entity);
		skin.skins = std::move(painted);
		skin.revision = revision;
		return entity;
	}
};
} // namespace

TEST(CreatureDrawSkins, ASkinIsUploadedOnceUntilItIsPaintedAgain)
{
	SkinsFixture fixture;
	const auto entity = fixture.Paint({{.id = 11, .texels = {5}}, {.id = 12, .texels = {6}}}, 1);
	fixture.Update();
	fixture.Update();
	ASSERT_NE(fixture.skins.Find(entity, 11), nullptr);
	EXPECT_EQ(fixture.skins.Find(entity, 11)->uploads, 1);
	EXPECT_EQ(fixture.skins.Find(entity, 12)->firstTexel, 6u);
	EXPECT_EQ(fixture.skins.Find(entity, 13), nullptr);
	EXPECT_EQ(fixture.made, 2);

	auto& skin = fixture.registry.Get<CreatureSkin>(entity);
	skin.skins.at(0).texels = {9};
	skin.revision = 2;
	fixture.Update();
	EXPECT_EQ(fixture.skins.Find(entity, 11)->uploads, 2);
	EXPECT_EQ(fixture.skins.Find(entity, 11)->firstTexel, 9u);
	// the textures are kept, only painted again
	EXPECT_EQ(fixture.made, 2);
}

TEST(CreatureDrawSkins, ACreatureGoneTakesItsSkinsWithIt)
{
	SkinsFixture fixture;
	const auto gone = fixture.Paint({{.id = 11, .texels = {1}}}, 1);
	const auto kept = fixture.Paint({{.id = 11, .texels = {2}}}, 1);
	fixture.Update();
	EXPECT_EQ(fixture.skins.Size(), 2u);
	fixture.registry.Destroy(gone);
	fixture.Update();
	EXPECT_EQ(fixture.skins.Size(), 1u);
	EXPECT_EQ(fixture.skins.Find(gone, 11), nullptr);
	EXPECT_NE(fixture.skins.Find(kept, 11), nullptr);
}

TEST(CreatureDrawSkins, OutOfTexturesASkinKeepsTheSpeciesOwn)
{
	SkinsFixture fixture;
	fixture.textureLimit = 1;
	const auto entity = fixture.Paint({{.id = 11, .texels = {1}}, {.id = 12, .texels = {2}}}, 1);
	fixture.Update();
	EXPECT_NE(fixture.skins.Find(entity, 11), nullptr);
	EXPECT_EQ(fixture.skins.Find(entity, 12), nullptr);
}

TEST(CreatureDrawSkins, ALandWithoutACreatureHasNoSkinsAndGainsNoStorage)
{
	SkinsFixture fixture;
	const auto entity = fixture.registry.Create();
	fixture.registry.AssignState<Transform>(entity, Transform {glm::vec3(1.0f), glm::mat3(1.0f), glm::vec3(1.0f)});
	const auto before = StorageCount(fixture.registry);
	fixture.Update();
	EXPECT_EQ(fixture.skins.Size(), 0u);
	EXPECT_EQ(fixture.made, 0);
	EXPECT_EQ(StorageCount(fixture.registry), before);
}
