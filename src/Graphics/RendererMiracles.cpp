/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How the miracles look outside their effects, as the renderer draws them: each one-shot globe added over what is
// behind it with its glint running, the miracle spinning inside it, and the rings round an extreme one; and the bands
// flying onto the hand that holds a miracle and the bracelets it wears.

#define LOCATOR_IMPLEMENTATIONS

#include <cstring>

#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "3D/L3DMesh.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandMiracleFx.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/GamePlayers.h"
#include "ECS/PlayerColour.h"
#include "ECS/Registry.h"
#include "ECS/RewardOpening.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/AlphaTest.h"
#include "Graphics/InstanceDesc.h"
#include "Graphics/MeshDetail.h"
#include "Graphics/ObjectDrawWay.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Graphics/ViewFrustum.h"
#include "Graphics/ZSort.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/MiracleVisuals.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Visual/TexelDepth.h"

using namespace openblack;
using namespace openblack::graphics;
namespace visuals = openblack::magic::visuals;

namespace
{
/// The colour of the player at this computer
uint32_t OwnColour()
{
	const auto own = Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
	return ecs::game_players::Colour(own);
}
constexpr float k_ByteMax = 255.0f;
/// The freeze phial's creature takes its spell in this way
constexpr int k_FreezeReceiveType = 0;
/// The beam explosion's seed is added over what is behind it without writing depth
constexpr auto k_AddedSeed = SpellSeedType::BeamExplosion;

glm::vec3 ColourOf(uint32_t rgb)
{
	return glm::vec3(static_cast<float>((rgb >> 16u) & 0xFFu), static_cast<float>((rgb >> 8u) & 0xFFu),
	                 static_cast<float>(rgb & 0xFFu)) /
	       k_ByteMax;
}

/// What blends in the world, tested against its depth
constexpr uint64_t k_BlendedState = BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA;
/// Added over what is behind by its texture's alpha times the alpha it is tinted with
constexpr uint64_t k_AddedState = k_BlendedState | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);
} // namespace

Renderer::SeedModelLooks Renderer::FindSeedModelLooks() const
{
	const auto& textures = Locator::resources::value().GetTextures();
	const auto envmap = entt::hashed_string("raw/envmap").value();
	const auto ice = entt::hashed_string("raw/S_IceEnvMap").value();
	return {
	    .object = _shaderManager->GetShader("Object"),
	    .environment = _shaderManager->GetShader("ObjectEnvironment"),
	    .environmentMap = textures.Contains(envmap) ? &*textures.Handle(envmap) : nullptr,
	    .iceMap = textures.Contains(ice) ? &*textures.Handle(ice) : nullptr,
	};
}

void Renderer::DrawSeedModel(const DrawSceneDesc& desc, const SeedModelDraw& shown, const view_frustum::Frustum* seenBy,
                             DrawEncoder* encoder, const SeedModelLooks* looks) const
{
	const bool reflection = desc.viewId == RenderPass::Reflection;
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& info = Locator::infoConstants::value();
	const auto eye = desc.camera->GetOrigin();
	const auto found = looks != nullptr ? *looks : FindSeedModelLooks();
	const auto* environment = found.environmentMap;
	const auto* iceMap = found.iceMap;
	const auto* objectProgram = found.object;
	const auto* environmentProgram = found.environment;
	const auto translucent = TranslucentView(desc.viewId);
	const auto& seed = magic::GetSpellSeedInfo(info, shown.seed);

	// The seed's model, for the seeds that show one, spinning at its mesh height above the middle: food and the phials
	// shining with the environment map, the beam added over what is behind it, a phial running through its texture and
	// drawn as its creature takes its spell
	const auto meshId = magic::GetSeedModel(info, shown.seed, shown.ownerAlignment);
	const auto seedMesh = meshId.has_value() ? resources::HashIdentifier(*meshId) : entt::id_type {0};
	if (meshId.has_value() && meshes.Contains(seedMesh))
	{
		const bool phial = visuals::IsPhialSeed(shown.seed);
		int receiveType = -1;
		if (phial)
		{
			if (const auto* spell = magic::GetMagicInfoAs<GMagicCreatureSpellInfo>(info, seed.magicTypes[0]))
			{
				receiveType = static_cast<int>(spell->creatureReceiveSpellType);
			}
		}
		const float pulse = visuals::PhialPulse(shown.phialPhase);
		// A seed that is not a phial takes the light of the land, unless it is drawn at once. A seed drawn at once has the
		// alpha that it is drawn at. A phial is lit and faded in the same way when it is drawn at once.
		const uint8_t seedAlpha = shown.unlitWhite ? shown.phialAlpha : std::numeric_limits<uint8_t>::max();
		const std::array<visuals::PhialDraw, 1> wholeDraw {{{.size = 1.0f, .alpha = seedAlpha, .landLit = !shown.unlitWhite}}};
		const auto phialDraws = phial ? visuals::PhialDraws(receiveType, shown.phialPhase, pulse, shown.phialAlpha)
		                              : std::vector<visuals::PhialDraw> {};
		const auto draws =
		    phial ? std::span<const visuals::PhialDraw>(phialDraws) : std::span<const visuals::PhialDraw>(wholeDraw);
		const auto shape = phial ? visuals::PhialScale(receiveType, shown.phialPhase) : glm::vec3(1.0f);
		const bool environmentMapped = environment != nullptr && (phial || shown.seed == SpellSeedType::Food);
		const bool added = shown.seed == k_AddedSeed;
		const auto mesh = meshes.Handle(seedMesh);
		const auto at = shown.middle + glm::vec3(0.0f, seed.meshHeight * shown.scale, 0.0f);
		// A model of several bones can move its parts out of its box, so the view test uses only a model of no bones. A
		// draw out of the view puts no pixel on the screen.
		const auto box = mesh->GetBoundingBox();
		const bool testsView = seenBy != nullptr && !mesh->IsBoned();
		const bool sortedWhole = object_draw::SortedWhole(mesh->GetFlags());
		const float halfDiagonal = mesh_detail::HalfDiagonal(box.Size());
		const auto forward = desc.camera->GetForward();
		const float modelDetail = Locator::config::value().detail.modelDetail;
		for (size_t index = 0; index < draws.size(); ++index)
		{
			const auto& draw = draws[index];
			const float objectScale = seed.scale * shown.scale * draw.size;
			const auto model = glm::translate(glm::mat4(1.0f), at) * glm::eulerAngleY(shown.spin) *
			                   glm::scale(glm::mat4(1.0f), objectScale * shape);
			if (testsView && !view_frustum::SeesBox(*seenBy, model, box.minima, box.maxima))
			{
				continue;
			}
			// A phial draws each copy before its last at once. Its last draw, and the draw of any other seed, goes the
			// way of the seed.
			const auto way = index + 1 == draws.size() ? shown.way : object_draw::Way::AtOnce;
			uint8_t alpha = draw.alpha;
			auto detailLevel = mesh_detail::LevelBit(mesh_detail::Mesh::High);
			// The warm-up draws each model at no size, which no cull may cut
			if (!_warmingUp && way == object_draw::Way::AtOnce && object_draw::FarCulled(halfDiagonal, objectScale, at, eye))
			{
				continue;
			}
			if (!_warmingUp && way == object_draw::Way::Banded)
			{
				// The bands go by the middle of the model's box. The last draw of a phial and a seed of less than full
				// alpha keep their own alpha in the fade.
				const auto centre = glm::vec3(model * glm::vec4(box.Center(), 1.0f));
				const float reach = mesh_detail::Reach(0.0f, mesh_detail::ScaledRadius(box.Size(), objectScale), modelDetail);
				const auto banded =
				    object_draw::DrawBanded(mesh_detail::Choose(mesh_detail::ViewDepth(centre, eye, forward), reach, true),
				                            draw.alpha, phial || draw.alpha != std::numeric_limits<uint8_t>::max());
				if (!banded.has_value())
				{
					continue;
				}
				alpha = banded->alpha;
				detailLevel = mesh_detail::LevelBit(banded->mesh);
			}
			const bool amongSorted = object_draw::AmongSorted(way, sortedWhole);
			// A model of several bones (the flocks' birds and wolves) has each placed by its rest matrix
			std::vector<glm::mat4> bones;
			if (mesh->IsBoned() && !mesh->GetBoneMatrices().empty())
			{
				bones.reserve(mesh->GetBoneMatrices().size());
				std::ranges::transform(mesh->GetBoneMatrices(), std::back_inserter(bones),
				                       [&model](const glm::mat4& bone) { return model * bone; });
			}
			L3DMeshSubmitDesc submitDesc = {};
			submitDesc.encoder = encoder;
			submitDesc.program = environmentMapped ? environmentProgram : objectProgram;
			submitDesc.environment = environmentMapped ? environment : nullptr;
			submitDesc.modelMatrices = bones.empty() ? &model : bones.data();
			submitDesc.matrixCount = bones.empty() ? 1 : static_cast<uint8_t>(std::min<size_t>(bones.size(), UINT8_MAX));
			submitDesc.mirrored = reflection;
			submitDesc.useMaterialCulling = true;
			// A phial's skin is stepped through its frames, but slides only where its materials let it and never while
			// the phial shines with the environment map, so its liquid shows its first frame
			submitDesc.uvOffset = phial ? visuals::PhialUvOffset(shown.phialFrame) : glm::vec2(0.0f);
			submitDesc.uvOffsetByMaterial = phial;
			submitDesc.sortDepth = object_draw::BlendDepth(way, sortedWhole, at, eye);
			submitDesc.detailLevel = detailLevel;
			const float alphaShare = static_cast<float>(alpha) / k_ByteMax;
			if (added)
			{
				// Added by its own alpha without writing depth, lit as the rest
				submitDesc.viewId = translucent;
				submitDesc.state = k_AddedState | BGFX_STATE_WRITE_A;
			}
			else if (!draw.landLit)
			{
				// Lit in white by the sun with none of the land's colour, at its alpha: blended by its materials over
				// what is behind, or as they say when whole
				submitDesc.viewId = (alpha < 255 || amongSorted) ? translucent : desc.viewId;
				submitDesc.state = k_BlendedState | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z;
				submitDesc.useMaterialBlending = true;
				submitDesc.objectLook = L3DMeshSubmitDesc::ObjectLook {.colour = glm::vec3(k_ByteMax), .alpha = alphaShare};
				submitDesc.landColourAdded = false;
			}
			else if (alpha < 255)
			{
				// Lit, faded by its alpha
				submitDesc.viewId = translucent;
				submitDesc.state = k_BlendedState | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_FACTOR, BGFX_STATE_BLEND_INV_FACTOR);
				submitDesc.rgba = static_cast<uint32_t>(alpha) * 0x01010101u;
			}
			else
			{
				// Whole: with the solid models, or among the sorted things when its mesh is sorted whole
				submitDesc.viewId = amongSorted ? translucent : desc.viewId;
				submitDesc.state = k_BlendedState | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z;
				submitDesc.useMaterialBlending = true;
			}
			// A frozen phial is tinted icy by its pulse, and its ice shines over it
			const bool frozen = phial && receiveType == k_FreezeReceiveType;
			if (frozen)
			{
				submitDesc.tint = glm::vec4(ColourOf(visuals::FreezeTint(pulse)), 0.0f);
			}
			DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
			if (frozen && iceMap != nullptr)
			{
				submitDesc.viewId = translucent;
				submitDesc.program = environmentProgram;
				submitDesc.environment = iceMap;
				submitDesc.environmentOnlyAlpha = std::max(std::nearbyint(pulse * k_ByteMax), 1.0f) / k_ByteMax;
				submitDesc.useMaterialBlending = false;
				submitDesc.state = k_AddedState;
				submitDesc.tint = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
				DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
			}
		}
	}
}

void Renderer::DrawCaveSeeds(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !Locator::temple::has_value() || !Locator::temple::value().Active() ||
	    !Locator::infoConstants::has_value())
	{
		return;
	}
	// The seeds are the player's at this computer: theirs is the alignment that makes the flock's seed a bat. Each is
	// drawn at its full size and colour, the land's light not on it.
	const auto own = Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
	const float alignment =
	    Locator::alignmentSystem::has_value() ? Locator::alignmentSystem::value().GetPlayerAlignment(own) : 0.0f;
	for (const auto& seed : Locator::temple::value().GetCaveSeeds())
	{
		DrawSeedModel(desc, {.seed = seed.seed,
		                     .middle = seed.point,
		                     .scale = 1.0f,
		                     .spin = seed.spin,
		                     .phialPhase = seed.phialPhase,
		                     .phialFrame = seed.phialFrame,
		                     .phialAlpha = std::numeric_limits<uint8_t>::max(),
		                     .ownerAlignment = alignment,
		                     .way = object_draw::Way::AtOnce});
	}
}

void Renderer::DrawMapSpellSeeds(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !Locator::temple::has_value() || !Locator::temple::value().Active() ||
	    !Locator::infoConstants::has_value())
	{
		return;
	}
	// The map draws each seed at a tenth of its size, as the creature's room draws its seeds
	for (const auto& seed : Locator::temple::value().GetMapSpellSeeds())
	{
		DrawSeedModel(desc, {.seed = seed.seed,
		                     .middle = seed.middle,
		                     .scale = TempleMapSpells::k_Scale,
		                     .spin = seed.spin,
		                     .phialPhase = seed.phialPhase,
		                     .phialFrame = seed.phialFrame,
		                     .phialAlpha = std::numeric_limits<uint8_t>::max(),
		                     .ownerAlignment = seed.ownerAlignment,
		                     .way = object_draw::Way::AtOnce});
	}
}

void Renderer::DrawGlobes(const DrawSceneDesc& desc) const
{
	const bool reflection = desc.viewId == RenderPass::Reflection;
	if ((desc.viewId != RenderPass::Main && !reflection) ||
	    (Locator::temple::has_value() && Locator::temple::value().Active()) || !Locator::infoConstants::has_value())
	{
		return;
	}
	using ecs::components::OneOffSpellSeed;
	using ecs::components::Transform;
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto eye = desc.camera->GetOrigin();
	const auto* objectProgram = _shaderManager->GetShader("Object");
	const auto translucent = TranslucentPassOf(desc.viewId);
	const bool haveBubble = meshes.Contains(OneOffSpellSeed::k_MeshId.value());
	const bool haveRing = meshes.Contains(OneOffSpellSeed::k_RingMeshId.value());
	// A globe nobody owns shows its rings in the colour of the player at this computer
	const auto ringColour = ColourOf(OwnColour());

	// Of the globes on the land's blocks only those that the land's draw list draws show
	const auto* drawList = Locator::objectDrawListSystem::has_value() ? &Locator::objectDrawListSystem::value() : nullptr;
	const auto drawGlobe = [&](entt::entity entity, const OneOffSpellSeed& globe, const Transform& transform) {
		if (drawList != nullptr && !drawList->Drawn(desc.entities, entity))
		{
			return;
		}
		const float scale = transform.scale.x * visuals::k_GlobeSeedScale;

		// The miracle's seed spinning inside, below the middle, for the seeds that show one. Nobody owns a globe, and
		// nobody's alignment of 0 is evil enough for the flock's bat.
		DrawSeedModel(desc, {.seed = globe.seedType,
		                     .middle = globe.middle,
		                     .scale = scale,
		                     .spin = globe.spin,
		                     .phialPhase = globe.phialPhase,
		                     .phialFrame = globe.phialFrame,
		                     .phialAlpha = visuals::k_GlobeAlpha,
		                     .ownerAlignment = 0.0f});

		// The globe, added at 150 of 255 by its texture's alpha and writing its depth, turned to face the camera about its
		// middle, its glint running over its dome. Its materials are those that the game changes when it makes the globe. It is
		// drawn where it is, round its seed; only where it comes among the blended things is taken from its middle pushed
		// towards the camera by its radius, so that it comes after what is inside it. The sort uses the key of a placed model.
		if (haveBubble)
		{
			const auto bubble = meshes.Handle(OneOffSpellSeed::k_MeshId.value());
			const auto halfExtent = bubble->GetBoundingBox().Size() * 0.5f;
			const float radius = std::max(halfExtent.x, halfExtent.z) * transform.scale.x;
			glm::mat4 model = globe.drawnModel.value_or(glm::mat4(1.0f));
			if (!globe.drawnModel.has_value())
			{
				model = glm::mat4(transform.rotation * glm::mat3(glm::scale(glm::mat4(1.0f), transform.scale)));
				model[3] = glm::vec4(transform.position, 1.0f);
			}
			L3DMeshSubmitDesc submitDesc = {};
			submitDesc.viewId = translucent;
			submitDesc.program = objectProgram;
			submitDesc.modelMatrices = &model;
			submitDesc.matrixCount = 1;
			submitDesc.mirrored = reflection;
			submitDesc.useMaterialCulling = true;
			submitDesc.useMaterialBlending = true;
			submitDesc.changedMaterials = visuals::k_GlobeMaterials;
			submitDesc.state = k_BlendedState | BGFX_STATE_WRITE_Z;
			submitDesc.tint = glm::vec4(1.0f, 1.0f, 1.0f, static_cast<float>(visuals::k_GlobeAlpha) / k_ByteMax);
			submitDesc.uvOffset = visuals::GlintUvOffset(globe.glintFrame);
			submitDesc.sortDepth = zsort::ModelDepth(visuals::GlobeSortPoint(globe.middle, eye, radius), eye);
			DrawMesh(*bubble, submitDesc, std::numeric_limits<uint8_t>::max());
		}

		// An extreme miracle's rings, in the player's colour with a grey specular
		if (haveRing)
		{
			DrawPowerUpRings(desc, {.mesh = &*meshes.Handle(OneOffSpellSeed::k_RingMeshId.value()),
			                        .rings = visuals::RingCount(globe.powerUp),
			                        .spin = globe.ringSpin,
			                        .middle = globe.middle,
			                        .scale = scale,
			                        .colour = ringColour,
			                        .alpha = visuals::RingAlpha(visuals::k_GlobeAlpha),
			                        .way = object_draw::Way::Banded,
			                        .mirrored = false});
		}
	};
	desc.entities.Each<const OneOffSpellSeed, const Transform>(drawGlobe);
}

void Renderer::DrawRewardSeeds(const DrawSceneDesc& desc) const
{
	const bool reflection = desc.viewId == RenderPass::Reflection;
	if ((desc.viewId != RenderPass::Main && !reflection) ||
	    (Locator::temple::has_value() && Locator::temple::value().Active()) || !Locator::infoConstants::has_value())
	{
		return;
	}
	using ecs::components::Reward;
	using ecs::components::RewardSeed;
	// A town's new miracle spins above its opened chest, smaller than on an icon, lit by the land, in the colours of
	// the chest's player
	desc.entities.Each<const RewardSeed, const Reward>([&](const RewardSeed& shown, const Reward& chest) {
		const auto player = chest.player.value_or(PlayerNames::NEUTRAL);
		const float alignment = Locator::alignmentSystem::has_value() && player != PlayerNames::NEUTRAL
		                            ? Locator::alignmentSystem::value().GetPlayerAlignment(player)
		                            : 0.0f;
		DrawSeedModel(desc, {.seed = shown.seed,
		                     .middle = shown.middle,
		                     .scale = ecs::reward::k_TownSeedSize,
		                     .spin = shown.spin,
		                     .phialPhase = shown.phialPhase,
		                     .phialFrame = shown.phialFrame,
		                     .phialAlpha = ecs::reward::TownSeedAlpha(chest.turnsLeft),
		                     .ownerAlignment = alignment});
	});
}

void Renderer::DrawIconSeeds(const DrawSceneDesc& desc) const
{
	const bool reflection = desc.viewId == RenderPass::Reflection;
	if ((desc.viewId != RenderPass::Main && !reflection) ||
	    (Locator::temple::has_value() && Locator::temple::value().Active()) || !Locator::infoConstants::has_value())
	{
		return;
	}
	using ecs::components::IconSeed;
	const auto& registry = desc.entities;
	const auto& meshes = Locator::resources::value().GetMeshes();
	const bool haveRing = meshes.Contains(ecs::components::OneOffSpellSeed::k_RingMeshId.value());
	// The view of this pass, as the pass sets it. The function does not draw the seeds and the rings out of it.
	const auto seenBy = view_frustum::FromViewProjection(desc.camera->GetProjectionMatrix() *
	                                                     desc.camera->GetViewMatrix(Camera::Interpolation::Current));
	// A warm-up draws the model of each kind of seed at no size, for a good and an evil player, as the first seeds of
	// each kind draw them
	if (_warmingUp)
	{
		for (int32_t seed = 0; seed <= static_cast<int32_t>(SpellSeedType::BeamExplosion); ++seed)
		{
			for (const float alignment : {-1.0f, 1.0f})
			{
				DrawSeedModel(desc, {.seed = static_cast<SpellSeedType>(seed),
				                     .middle = glm::vec3(0.0f),
				                     .scale = 0.0f,
				                     .spin = 0.0f,
				                     .phialPhase = 0.0f,
				                     .phialFrame = 0.0f,
				                     .phialAlpha = std::numeric_limits<uint8_t>::max(),
				                     .ownerAlignment = alignment});
			}
		}
	}
	// Each icon with the player it is for, found on the game's thread. The models that the icons show are loaded here,
	// so that the threads that draw them only read them.
	struct Icon
	{
		const IconSeed* shown;
		PlayerNames player;
		float alignment;
	};
	std::vector<Icon> icons;
	const auto& info = Locator::infoConstants::value();
	// The alignment of each player is found once for all of its icons, kept by the number of the player
	std::array<std::optional<float>, std::numeric_limits<std::underlying_type_t<PlayerNames>>::max() + 1> alignments {};
	const auto alignmentOf = [&alignments](PlayerNames player) {
		if (!Locator::alignmentSystem::has_value() || player == PlayerNames::NEUTRAL)
		{
			return 0.0f;
		}
		auto& kept = alignments.at(static_cast<size_t>(player));
		if (!kept.has_value())
		{
			kept = Locator::alignmentSystem::value().GetPlayerAlignment(player);
		}
		return *kept;
	};
	const auto* iconSystem = Locator::spellIconSystem::has_value() ? &Locator::spellIconSystem::value() : nullptr;
	registry.Each<const IconSeed>([&](entt::entity icon, const IconSeed& shown) {
		// The seed of an icon at a worship site that is not built does not show
		if (iconSystem != nullptr && !iconSystem->Shown(icon))
		{
			return;
		}
		const auto player = ecs::world_objects::IconPlayer(registry, icon);
		const float alignment = alignmentOf(player);
		icons.push_back({.shown = &shown, .player = player, .alignment = alignment});
		if (const auto model = magic::GetSeedModel(info, shown.seed, alignment); model.has_value())
		{
			static_cast<void>(meshes.Find(resources::HashIdentifier(*model)));
		}
	});
	const auto& textures = Locator::resources::value().GetTextures();
	static_cast<void>(textures.Find(entt::hashed_string("raw/envmap").value()));
	static_cast<void>(textures.Find(entt::hashed_string("raw/S_IceEnvMap").value()));
	const auto* ring = haveRing ? meshes.Find(ecs::components::OneOffSpellSeed::k_RingMeshId.value()) : nullptr;
	const auto looks = FindSeedModelLooks();
	EncodeInOrder(icons.size(), [&](size_t index, DrawEncoder& to) {
		const auto& [shown, player, alignment] = icons[index];
		DrawSeedModel(desc,
		              {.seed = shown->seed,
		               .middle = shown->middle,
		               .scale = 1.0f,
		               .spin = shown->spin,
		               .phialPhase = shown->phialPhase,
		               .phialFrame = shown->phialFrame,
		               .phialAlpha = std::numeric_limits<uint8_t>::max(),
		               .ownerAlignment = alignment},
		              &seenBy, &to, &looks);
		// A power-up level shown has its rings, in the player's colour
		if (ring != nullptr)
		{
			DrawPowerUpRings(desc,
			                 {.mesh = ring,
			                  .rings = magic::visuals::RingCount(shown->powerUp),
			                  .spin = shown->ringSpin,
			                  .middle = shown->middle,
			                  .scale = 1.0f,
			                  .colour = ColourOf(ecs::game_players::Colour(player)),
			                  .alpha = magic::visuals::RingAlpha(std::numeric_limits<uint8_t>::max()),
			                  .way = object_draw::Way::Banded,
			                  .mirrored = reflection},
			                 &seenBy, &to);
		}
	});
}

void Renderer::DrawPowerUpRings(const DrawSceneDesc& desc, const PowerUpRingsDraw& shown, const view_frustum::Frustum* seenBy,
                                DrawEncoder* encoder) const
{
	if (shown.mesh == nullptr || shown.rings <= 0)
	{
		return;
	}
	const auto eye = desc.camera->GetOrigin();
	const auto box = shown.mesh->GetBoundingBox();
	const bool sortedWhole = object_draw::SortedWhole(shown.mesh->GetFlags());
	// Each ring stands at the seed's point, at its share of the seed's size
	const float ringScale = visuals::k_RingScale * shown.scale;
	const bool farCulled = object_draw::FarCulled(mesh_detail::HalfDiagonal(box.Size()), ringScale, shown.middle, eye);
	const auto* objectProgram = _shaderManager->GetShader("Object");
	const auto translucent = TranslucentView(desc.viewId);
	for (int i = 0; i < shown.rings; ++i)
	{
		const auto model = visuals::RingModel(i, shown.spin, shown.middle, shown.scale, eye);
		if (seenBy != nullptr && !shown.mesh->IsBoned() && !view_frustum::SeesBox(*seenBy, model, box.minima, box.maxima))
		{
			continue;
		}
		// Added without writing depth, with a grey specular. A draw at once comes after the solid models and before
		// every sorted thing.
		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.encoder = encoder;
		submitDesc.viewId = translucent;
		submitDesc.program = objectProgram;
		submitDesc.modelMatrices = &model;
		submitDesc.matrixCount = 1;
		submitDesc.mirrored = shown.mirrored;
		submitDesc.state = k_AddedState;
		submitDesc.tint = glm::vec4(shown.colour, static_cast<float>(shown.alpha) / k_ByteMax);
		submitDesc.lightAdd = ColourOf(visuals::k_RingSpecular);
		submitDesc.sortDepth = zsort::k_AfterSolidBeforeSorted;
		if (!farCulled)
		{
			DrawMesh(*shown.mesh, submitDesc, std::numeric_limits<uint8_t>::max());
		}
		// The second draw of the last ring goes through the object draw when the object draw draws the seed. Its fade
		// gives the ring its alpha in place of the ring's own alpha.
		if (i + 1 != shown.rings || shown.way != object_draw::Way::Banded)
		{
			if (!farCulled)
			{
				DrawMesh(*shown.mesh, submitDesc, std::numeric_limits<uint8_t>::max());
			}
			continue;
		}
		const auto centre = glm::vec3(model * glm::vec4(box.Center(), 1.0f));
		const float reach = mesh_detail::Reach(0.0f, mesh_detail::ScaledRadius(box.Size(), ringScale),
		                                       Locator::config::value().detail.modelDetail);
		const auto banded = object_draw::DrawBanded(
		    mesh_detail::Choose(mesh_detail::ViewDepth(centre, eye, desc.camera->GetForward()), reach, true), shown.alpha,
		    false);
		if (!banded.has_value())
		{
			continue;
		}
		submitDesc.detailLevel = mesh_detail::LevelBit(banded->mesh);
		submitDesc.tint.a = static_cast<float>(banded->alpha) / k_ByteMax;
		submitDesc.sortDepth = object_draw::BlendDepth(shown.way, sortedWhole, shown.middle, eye);
		DrawMesh(*shown.mesh, submitDesc, std::numeric_limits<uint8_t>::max());
	}
}

namespace
{
/// The hand that the frame draws: the left hand of the god of this computer. Only this hand shows its miracles.
entt::entity DrawnHand()
{
	using ecs::systems::HandSystemInterface;
	if (!Locator::handSystem::has_value())
	{
		return entt::null;
	}
	return Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
}

/// The hand that holds the miracles, its look and where it is drawn: its matrix and each of its bones' in the world
struct HandPose
{
	const ecs::components::HandMiracleFx* fx {nullptr};
	glm::mat4 model {1.0f};
	std::vector<glm::mat4> bones;
};

std::optional<HandPose> PoseOfHand(const ecs::Registry& registry)
{
	using ecs::components::Hand;
	using ecs::components::HandMiracleFx;
	using ecs::components::Transform;
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(Hand::k_MeshId))
	{
		return std::nullopt;
	}
	std::optional<HandPose> found;
	const auto drawnHand = DrawnHand();
	registry.Each<const Hand, const HandMiracleFx, const Transform>([&](entt::entity entity, const Hand& hand,
	                                                                    const HandMiracleFx& fx, const Transform& transform) {
		if (found.has_value() || entity != drawnHand)
		{
			return;
		}
		// Placed as the world's models are
		const auto model = glm::translate(glm::mat4(1.0f), transform.position) * glm::mat4(transform.rotation) *
		                   glm::scale(glm::mat4(1.0f), transform.scale);
		const auto mesh = meshes.Handle(Hand::k_MeshId);
		const auto& rest = mesh->GetBoneMatrices();
		const auto& bones = hand.boneMatrices.size() == rest.size() ? hand.boneMatrices : rest;
		HandPose pose {.fx = &fx, .model = model, .bones = {}};
		pose.bones.reserve(bones.size());
		std::ranges::transform(bones, std::back_inserter(pose.bones), [&model](const glm::mat4& bone) { return model * bone; });
		found = std::move(pose);
	});
	return found;
}
} // namespace

const Texture2D* Renderer::HandFlowTexture() const
{
	// The flow's sheet is white where it is used, its alpha the flow; the game draws the two as one
	if (!_handFlowLoaded && Locator::filesystem::has_value())
	{
		_handFlowLoaded = true;
		constexpr uint16_t k_Size = 256;
		constexpr size_t k_Pixels = static_cast<size_t>(k_Size) * k_Size;
		auto& fileSystem = Locator::filesystem::value();
		const auto colourPath = fileSystem.GetPath<filesystem::Path::Textures>() / "S_Hand_Flow.raw";
		const auto alphaPath = fileSystem.GetPath<filesystem::Path::Textures>() / "S_Hand_Flowa.raw";
		if (fileSystem.Exists(colourPath) && fileSystem.Exists(alphaPath))
		{
			const auto colour = fileSystem.ReadAll(colourPath);
			const auto alpha = fileSystem.ReadAll(alphaPath);
			if (colour.size() == k_Pixels * 3 && alpha.size() == k_Pixels)
			{
				const auto* memory = bgfx::alloc(static_cast<uint32_t>(k_Pixels * 4));
				for (size_t i = 0; i < k_Pixels; ++i)
				{
					memory->data[(i * 4) + 0] = colour[(i * 3) + 0];
					memory->data[(i * 4) + 1] = colour[(i * 3) + 1];
					memory->data[(i * 4) + 2] = colour[(i * 3) + 2];
					memory->data[(i * 4) + 3] = alpha[i];
				}
				// Kept four bits a channel, as the game keeps every texture it loads with an alpha
				texel_depth::Reduce(std::span(memory->data, memory->size), texel_depth::Depth::FourBits);
				_handFlowTexture = std::make_unique<Texture2D>("HandFlow");
				_handFlowTexture->CreateWithinFrame(k_Size, k_Size, 1, TextureFormat::RGBA8, Wrapping::Repeat, Filter::Linear,
				                                    memory);
				// After a restore of the graphics device the renderer reads the files again and makes a new texture
				_handFlowTexture->SetTexelsGivenAgain();
			}
		}
	}
	return _handFlowTexture.get();
}

std::optional<Renderer::HandGlowLook> Renderer::HandGlowOf(const DrawSceneDesc& desc) const
{
	using ecs::components::Hand;
	using ecs::components::HandMiracleFx;
	if (!desc.drawHand || !desc.drawHandEffects || desc.viewId == RenderPass::Reflection)
	{
		return std::nullopt;
	}
	const auto* flow = HandFlowTexture();
	if (flow == nullptr)
	{
		return std::nullopt;
	}
	std::optional<HandGlowLook> look;
	const auto drawnHand = DrawnHand();
	desc.entities.Each<const Hand, const HandMiracleFx>([&](entt::entity entity, const Hand&, const HandMiracleFx& fx) {
		if (look.has_value() || !fx.glowing || entity != drawnHand)
		{
			return;
		}
		// The flow's sheet, an eighth of it at a time, running backwards over the hand in its player's colour at four
		// fifths
		const auto rgb = OwnColour();
		look = HandGlowLook {
		    .flow = flow,
		    .uvOffset = visuals::GlowUvOffset(fx.glowFrame),
		    .uvScale = visuals::k_GlowCellSize,
		    .colour = ColourOf(rgb) * k_ByteMax,
		    .alpha = visuals::GlowAlpha(),
		};
	});
	return look;
}

void Renderer::DrawTribalPower(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !Locator::miracleFxSystem::has_value())
	{
		return;
	}
	const auto& fx = Locator::miracleFxSystem::value();
	const auto* texture = fx.GetTextTexture();
	const auto vertices = fx.GetTribalPowerText();
	if (vertices.empty() || texture == nullptr)
	{
		return;
	}
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto count = static_cast<uint32_t>(vertices.size());
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), count * sizeof(OrientedTextVertex));

	// The game's text in the world: the glyphs blended by their coverage, tested against the depth of what is drawn,
	// seen from both sides
	const auto* shader = _shaderManager->GetShader("Text3D");
	const auto model = glm::mat4(1.0f);
	bgfx::setTransform(glm::value_ptr(model));
	bgfx::setVertexBuffer(0, &buffer);
	shader->SetTextureSampler("s_texture", 0, *texture);
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA |
	               BGFX_STATE_BLEND_ALPHA);
	// The game draws all text with the alpha test of the font pages: the faintest pixels of the glyphs do not show
	const auto alphaTest = alpha_test::Uniform(alpha_test::k_FontPageReference);
	shader->SetUniformValue("u_alphaTestRef", alphaTest.data());
	shader->Submit(static_cast<bgfx::ViewId>(desc.viewId));
}

void Renderer::DrawHandMiracleBands(const DrawSceneDesc& desc) const
{
	using ecs::components::Hand;
	using ecs::components::HandMiracleFx;
	using ecs::components::OneOffSpellSeed;
	if (desc.viewId != RenderPass::Main || !desc.drawHand || !desc.drawHandEffects || !Locator::rendereringSystem::has_value())
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(OneOffSpellSeed::k_RingMeshId.value()) || !meshes.Contains(Hand::k_MeshId))
	{
		return;
	}
	const auto pose = PoseOfHand(desc.entities);
	if (!pose.has_value() || (pose->fx->bands.bracelets.empty() && pose->fx->bands.flying.empty()))
	{
		return;
	}
	const auto* fx = pose->fx;
	// The hand's root, where the bands go round it, and the camera's frame they fly from
	const glm::mat4 root = pose->bones.empty() ? pose->model : pose->bones[0];
	const auto& camera = *desc.camera;
	glm::mat4 cameraFrame {glm::vec4(camera.GetRight(), 0.0f), glm::vec4(camera.GetUp(), 0.0f),
	                       glm::vec4(camera.GetForward(), 0.0f), glm::vec4(camera.GetOrigin(), 1.0f)};
	const auto atCamera = cameraFrame * visuals::BandAtCamera(camera.GetNearClip());
	const auto colour = ColourOf(OwnColour());
	const auto ring = meshes.Handle(OneOffSpellSeed::k_RingMeshId.value());
	const auto* program = _shaderManager->GetShader("Object");
	const auto translucent = TranslucentPassOf(desc.viewId);
	const auto draw = [&](const visuals::HandBand& band) {
		const auto pose = visuals::PoseOf(band);
		if (!pose.shown || pose.alpha == 0)
		{
			return;
		}
		// On the hand once there; on the way a bracelet eases every part of its matrix towards the hand without its spin,
		// and a flying band keeps the camera's turn while it shrinks and moves to the spinning band's place
		const auto onHand = root * visuals::BandOnHand(band);
		glm::mat4 model = onHand;
		if (pose.t < 1.0f)
		{
			model = band.kind == visuals::BandKind::Bracelet
			            ? visuals::BandBlended(atCamera, root * visuals::BandOnHandUnspun(band), pose.t)
			            : visuals::BandBetween(atCamera, onHand, pose.t);
		}
		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.viewId = translucent;
		submitDesc.program = program;
		submitDesc.modelMatrices = &model;
		submitDesc.matrixCount = 1;
		submitDesc.state = k_AddedState;
		submitDesc.tint = glm::vec4(colour, std::max(static_cast<float>(pose.alpha) / k_ByteMax, 1.0f / k_ByteMax));
		submitDesc.sortDepth = zsort::Depth(glm::vec3(model[3]), camera.GetOrigin());
		DrawMesh(*ring, submitDesc, std::numeric_limits<uint8_t>::max());
	};
	std::ranges::for_each(fx->bands.bracelets, draw);
	std::ranges::for_each(fx->bands.flying, draw);
}
