/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FireGraphic.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLight.h"
#include "3D/LandMorph.h"
#include "Audio/Audio.h"
#include "Common/GameRandom.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Life.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FireGraphicSystemInterface.h"
#include "ECS/Trees.h"
#include "ECS/Weather/Weather.h"
#include "FireEffect.h"
#include "FireGraphicData.h"
#include "FireObjectTraits.h"
#include "GameClock.h"
#include "Locator.h"
#include "Particles/PSysManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::fire;

namespace
{
constexpr float k_FlameLife = 4.3f;
constexpr float k_FlameFadeIn = 1.0f;
constexpr float k_PuffLife = 3.0f;
constexpr uint32_t k_BurstTurns = 30;

using graphic::Graphic;
using graphic::SpritePos;

/// The game's fire graphics; stops with a message when there are none (before the game or after it has gone)
systems::FireGraphicSystemInterface& Graphics()
{
	if (!Locator::fireGraphicSystem::has_value())
	{
		std::fputs("ecs::fire::graphic: no fire graphics in the locator (Locator::fireGraphicSystem)\n", stderr);
		std::abort();
	}
	return Locator::fireGraphicSystem::value();
}

/// The local random generator (the local seed)
using game_random::LocalFloatRand;
using game_random::LocalRand;

psys::Creator MakeCreator(const char* texture, bool additive, float originY, float stretch)
{
	psys::Creator creator;
	creator.kind = psys::Creator::Kind::Sprite;
	creator.className = "FireGraphic";
	creator.texture = texture;
	// The frame is the cell (the sprite flags' low 6 bits), 8 cells a row; (inferred) that S_Fire is drawn 8 x 8 like
	// S_SpriteSheet3. The flame cells (fmod(.., 32) + 32, frame_anim::FireCell) need 64 frames: ParticleFrameIndex then
	// keeps them as they are
	creator.spritesPerRow = 8;
	creator.numFrames = 64;
	creator.additive = additive;
	creator.originY = originY;
	creator.stretch = stretch;
	return creator;
}

/// The materials S_Fire (mode 13), S_SpriteSheet3 (13) and S_SpriteSheet3 (6); the shared sprite has size 1 and height
/// 2. Each flame sets the sprite's x origin to 0 and its y origin to height x size x -1 = -2 x size: the sprite draw
/// subtracts it, so the flame's local y runs from 0 to 4 x size on the screen's up and its base stays on the point. As a
/// PSys creator that is SpriteOriginY = -1 (oy = OriginY x size x stretch).
const psys::Creator& FlameCreator()
{
	static const auto creator = MakeCreator("S_Fire", true, -1.0f, 2.0f);
	return creator;
}
/// (approximate) The steam and smoke are drawn centred (origin 0). The original's steam and smoke draws never write the
/// origin of the shared sprite, so they keep the oy = -2 x size of the last flame drawn with it: a fire draws its
/// flames (newest first, so the oldest last), then its steam, then its smoke, and a fire with no flame takes the last
/// flame of the fire drawn before it. In the original they sit 2 x that size higher on the screen's up. Not ported:
/// psys::manager::DrawAtom has no per-atom origin, and the order of the fires between them is not read
const psys::Creator& SteamCreator()
{
	static const auto creator = MakeCreator("S_SpriteSheet3", true, 0.0f, 2.0f);
	return creator;
}
const psys::Creator& SmokeCreator()
{
	static const auto creator = MakeCreator("S_SpriteSheet3", false, 0.0f, 2.0f);
	return creator;
}

const graphics::L3DMesh* MeshOf(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.TryGet<const components::Mesh>(object);
	if (mesh == nullptr || !Locator::resources::has_value())
	{
		return nullptr;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return nullptr;
	}
	return &*meshes.Handle(mesh->id);
}

/// A random point of a random triangle of the mesh (the plain object path), in the mesh's own coordinates; trees x 0.5.
/// False without a mesh. (inferred: the original's sampling is not read step by step: the triangle choice, the a + b > 1
/// fold and the box-centre fallback are the port's)
bool LocalRandomFlamePosition(entt::entity object, glm::vec3& position, int& index)
{
	const auto* mesh = MeshOf(object);
	if (mesh == nullptr)
	{
		position = glm::vec3(0.0f);
		return false;
	}
	// The LOD 0 triangles (inferred: the original counts the drawn mesh's triangles)
	std::vector<std::array<glm::vec3, 3>> triangles;
	for (const auto& subMesh : mesh->GetSubMeshes())
	{
		if ((subMesh->GetFlags().lodMask & 1) == 0 && mesh->GetSubMeshes().size() > 1)
		{
			continue;
		}
		const auto& p = subMesh->GetCollisionPositions();
		const auto& indices = subMesh->GetCollisionIndices();
		for (size_t i = 0; i + 2 < indices.size(); i += 3)
		{
			if (indices[i] < p.size() && indices[i + 1] < p.size() && indices[i + 2] < p.size())
			{
				triangles.push_back({p[indices[i]], p[indices[i + 1]], p[indices[i + 2]]});
			}
		}
	}
	if (triangles.empty())
	{
		position = mesh->GetBoundingBox().Center();
		index = 0;
	}
	else
	{
		index = static_cast<int>(LocalRand(static_cast<int32_t>(triangles.size())));
		float a = LocalFloatRand(1.0f);
		float b = LocalFloatRand(1.0f);
		if (a + b > 1.0f)
		{
			a = 1.0f - a;
			b = 1.0f - b;
		}
		const auto& t = triangles[static_cast<size_t>(index)];
		position = t[0] + (t[1] - t[0]) * a + (t[2] - t[0]) * b;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AnyOf<components::Tree, components::DeadTree>(object))
	{
		position *= 0.5f; // any kind of tree
	}
	return true;
}

/// The object's matrix x the local point
glm::vec3 WorldFlamePosition(entt::entity object, const glm::vec3& local)
{
	// The matrix is the object's drawn one: where it is drawn
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.AllOf<components::Transform>(object))
	{
		return local;
	}
	return glm::vec3(DrawnModel(registry, object) * glm::vec4(local, 1.0f));
}

/// The burning object's position, the turn's: the fire's sound
glm::vec3 ObjectPosition(entt::entity object)
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const components::Transform>(object);
	return transform != nullptr ? transform->position : glm::vec3(0.0f);
}

/// Where the burning object is drawn (its drawn matrix's translation): the flames, the steam, the smoke and the light
glm::vec3 DrawnObjectPosition(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.AllOf<components::Transform>(object) ? DrawnPosition(registry, object) : glm::vec3(0.0f);
}

/// The flames
void UpdateFlames(Graphic& graphic, const FireEffect& fire, float dt)
{
	const float fraction = fire.FireFraction();
	if (fraction != 0.0f)
	{
		const auto maximum = static_cast<float>(graphic.maxFlames);
		graphic.flameAccumulator += maximum / k_FlameLife * fraction * dt;
		if ((fire.flags & FireEffect::k_VeryHot) != 0 && !graphic.veryHotDone)
		{
			graphic.veryHotDone = true;
			graphic.flameAccumulator += maximum;
		}
		if ((fire.flags & FireEffect::k_JustIgnited) != 0)
		{
			graphic.flameAccumulator += static_cast<float>((graphic.maxFlames + 1) / 2);
		}
	}
	while (static_cast<float>(graphic.flameCount) < graphic.flameAccumulator)
	{
		++graphic.flameCount;
		SpritePos flame;
		if (LocalRandomFlamePosition(graphic.object, flame.position, flame.index))
		{
			graphic.flames.push_front(flame);
		}
	}
	const float fadeOut = 1.0f / (k_FlameLife - k_FlameFadeIn);
	for (auto it = graphic.flames.begin(); it != graphic.flames.end();)
	{
		it->age += dt;
		if (it->age > k_FlameLife)
		{
			it = graphic.flames.erase(it);
			continue;
		}
		it->scale = (fraction + 1.0f) * 0.5f * graphic.localScale;
		float alpha = it->age < k_FlameFadeIn ? (1.0f / k_FlameFadeIn) * it->age : 1.0f - (it->age - k_FlameFadeIn) * fadeOut;
		alpha = std::clamp(alpha, 0.0f, 1.0f) * 250.0f;
		it->alpha = static_cast<uint8_t>(std::clamp(alpha, 0.0f, 255.0f));
		++it;
	}
}

/// The puffs of the steam (while cooled, white) and the smoke (once it went out, grey): 4 a second for 30 turns, grow
/// from 0.2 to 2.6 x their scale and fade from `alpha0` in 3 s, drift with half the wind and rise
void UpdatePuffs(std::list<SpritePos>& puffs, float dt, float alpha0, float rise, const glm::vec3& at)
{
	// The wind at the object (ECS/Weather)
	const glm::vec3 wind = weather::GetWindAt(at, true);
	const float inverseLife = 1.0f / k_PuffLife;
	for (auto it = puffs.begin(); it != puffs.end();)
	{
		it->age += dt;
		if (it->age > k_PuffLife)
		{
			it = puffs.erase(it);
			continue;
		}
		const float t = inverseLife * it->age;
		// From 0.2 to 2.6 of the base scale, for steam and smoke alike
		it->scale = ((2.6f - 0.2f) * t + 0.2f) * it->baseScale;
		it->alpha = static_cast<uint8_t>(std::lrint((0.0f - alpha0) * t + alpha0)); // rounded to nearest
		// Eases towards half the wind at 0.1 a second
		const glm::vec3 pull = (wind * 0.5f - it->velocity) * 0.1f * dt;
		it->velocity += pull;
		it->velocity.y += rise * dt;
		it->position += it->velocity * (dt * it->baseScale);
		++it;
	}
}

SpritePos NewPuff(const Graphic& graphic, const glm::vec3& world, int index)
{
	SpritePos puff {
	    .position = world,
	    .index = index,
	    .baseScale = graphic.scaleMultiplier * graphic.localScale,
	};
	const float x = LocalFloatRand(2.0f) - 1.0f;
	const float z = LocalFloatRand(2.0f) - 1.0f;
	puff.velocity = glm::vec3(x * 1.0f, 0.0f, z * 1.0f);
	return puff;
}

void UpdateSteam(Graphic& graphic, const FireEffect& fire, float dt)
{
	if (graphic.steamStart == 0)
	{
		if ((fire.flags & FireEffect::k_Cooling) != 0 && fire.temperature > 75.0f &&
		    fire.temperature > graphic.steamTemperature)
		{
			graphic.steamStart = game_clock::Turn();
			graphic.steamCount = 0;
			graphic.steamAccumulator = 0.0f;
			graphic.steamTemperature = fire.temperature;
			// The sizzle: InGame sample 0x35 (G_Steam_01), owned by the fire graphic, 3D, track 0, at the burning
			// object's position. (inferred) the point the original copies is the burning object's position.
			if (graphic.soundOwner == 0)
			{
				graphic.soundOwner = audio::NewObjectId();
			}
			audio::PlayOptions options;
			options.sample = {audio::Bank(audio::SfxBank::InGame), 0x35};
			options.owner = audio::Owner::Object(graphic.soundOwner);
			options.is3D = true;
			options.track = false;
			options.position = ObjectPosition(graphic.object);
			audio::PlaySoundEffect(options);
		}
	}
	else if (game_clock::Turn() > graphic.steamStart + k_BurstTurns)
	{
		graphic.steamStart = 0;
	}
	else
	{
		graphic.steamAccumulator += 4.0f * dt;
		while (static_cast<float>(graphic.steamCount) < graphic.steamAccumulator)
		{
			++graphic.steamCount;
			glm::vec3 local;
			int index = 0;
			if (LocalRandomFlamePosition(graphic.object, local, index))
			{
				graphic.steam.push_front(NewPuff(graphic, WorldFlamePosition(graphic.object, local), index));
			}
		}
	}
	UpdatePuffs(graphic.steam, dt, 100.0f, 1.0f, DrawnObjectPosition(graphic.object));
}

void UpdateSmoke(Graphic& graphic, const FireEffect& fire, float dt)
{
	if (graphic.smokeStart == 0)
	{
		if ((fire.flags & FireEffect::k_JustExtinguished) != 0)
		{
			graphic.smokeStart = game_clock::Turn();
			graphic.smokeCount = 0;
			graphic.smokeAccumulator = 0.0f;
			LocalRandomFlamePosition(graphic.object, graphic.smokeLocal, graphic.smokeIndex);
		}
	}
	else if (game_clock::Turn() > graphic.smokeStart + k_BurstTurns)
	{
		graphic.smokeStart = 0;
	}
	else
	{
		graphic.smokeAccumulator += 4.0f * dt;
		while (static_cast<float>(graphic.smokeCount) < graphic.smokeAccumulator)
		{
			++graphic.smokeCount;
			graphic.smoke.push_front(
			    NewPuff(graphic, WorldFlamePosition(graphic.object, graphic.smokeLocal), graphic.smokeIndex));
		}
	}
	UpdatePuffs(graphic.smoke, dt, 180.0f, 2.0f, DrawnObjectPosition(graphic.object));
}

/// The fire draw through the renderer's PSys sprite path: one Z object per fire at the object
void CollectFires(std::vector<psys::manager::Drawable>& out)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& [id, graphic] : Graphics().All())
	{
		if (!registry.AllOf<components::Transform>(graphic->object))
		{
			continue;
		}
		psys::manager::Drawable drawable {DrawnPosition(registry, graphic->object), {}};
		// Flames, orange 0xFF713C, cell = int(fmod(-25 age, 32) + 32) (frame_anim::FireCell; cell 32 at age 0)
		// Flag bit 0: the flames follow the land like the morphed object (land_morph::Raised): H0 at the drawn object's
		// position, y = (H(flame) - H0) + y
		const auto ground = land_morph::CurrentAltitude();
		const float rockGround = (graphic->flags & 1) != 0 ? ground(glm::vec2(drawable.origin.x, drawable.origin.z)) : 0.0f;
		for (const auto& flame : graphic->flames)
		{
			auto position = WorldFlamePosition(graphic->object, flame.position);
			if ((graphic->flags & 1) != 0)
			{
				position.y = land_morph::Raised(ground, position, rockGround);
			}
			const auto frame = static_cast<float>(graphics::frame_anim::FireCell(flame.age));
			drawable.atoms.push_back({&FlameCreator(),
			                          position,
			                          glm::mat3(1.0f),
			                          flame.scale * graphic->scaleMultiplier,
			                          2.0f,
			                          static_cast<float>(flame.alpha),
			                          frame,
			                          {0xFF, 0x71, 0x3C}});
		}
		// White steam (additive), grey smoke (alpha 6), cell = int(fmod(25 age, 32)) (frame_anim::SteamCell)
		for (const auto& puff : graphic->steam)
		{
			const auto frame = static_cast<float>(graphics::frame_anim::SteamCell(puff.age));
			drawable.atoms.push_back({&SteamCreator(),
			                          puff.position,
			                          glm::mat3(1.0f),
			                          puff.scale,
			                          2.0f,
			                          static_cast<float>(puff.alpha),
			                          frame,
			                          {0xFF, 0xFF, 0xFF}});
		}
		for (const auto& puff : graphic->smoke) // grey 0x707070
		{
			const auto frame = static_cast<float>(graphics::frame_anim::SteamCell(puff.age));
			drawable.atoms.push_back({&SmokeCreator(),
			                          puff.position,
			                          glm::mat3(1.0f),
			                          puff.scale,
			                          2.0f,
			                          static_cast<float>(puff.alpha),
			                          frame,
			                          {0x70, 0x70, 0x70}});
		}
		if (!drawable.atoms.empty())
		{
			out.push_back(std::move(drawable));
		}
	}
}
} // namespace

uint8_t graphic::InitialFlags(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	// The fire graphic starts with bits 1-4 on; bit 0 = the 3D object is morphed with the land (openblack's
	// MorphWithTerrain); then the object's fire drawing flags rewrite bits 1-4
	uint8_t flags = 0x1E;
	if (registry.AllOf<components::MorphWithTerrain>(object))
	{
		flags |= 1;
	}
	// A fireball: only its steam flag, packed as bit 3
	if (registry.AllOf<components::MagicFireBall>(object))
	{
		flags = static_cast<uint8_t>((flags & 0xE1) | 0x08);
	}
	return flags;
}

void graphic::Create(FireEffect& fire)
{
	if (MeshOf(fire.object) == nullptr)
	{
		return; // no 3D object
	}
	if (!Graphics().SourceAdded())
	{
		Graphics().SetSourceAdded();
		psys::manager::AddDrawableSource(&CollectFires);
	}
	auto graphic = std::make_unique<Graphic>();
	graphic->object = fire.object;
	auto& registry = Locator::entitiesRegistry::value();
	graphic->flags = InitialFlags(fire.object);
	// The most flames: trees 2; else max(2D radius, height) < 3 ? 2 : 7
	const float radius = traits::Radius(fire.object);
	const float height = traits::Height(fire.object);
	const float largest = radius > height ? radius : height;
	const bool tree = registry.AnyOf<components::Tree, components::DeadTree>(fire.object);
	graphic->maxFlames = tree ? 2 : (largest < 3.0f ? 2 : 7);
	// The flames' local scale: trees 0.2 x height, the rest 0.5 x height (impressive objects and citadel
	// parts 0.3, not ported)
	graphic->localScale = (tree ? 0.2f : 0.5f) * height;
	// S_LMFireBall (6, 3 bytes, 1 frame) loaded once, given when the object is a multi-cell fixed object and its 2D
	// radius is over 2
	if (fire::traits::IsMultiCellStatic(fire.object) && radius > 2.0f)
	{
		graphic->lightMap = land_light::LoadBitmapFile("Spells/LightMaps/S_LMFireBall.raw", 6, 3, 1, 1);
	}
	Graphics().Set(fire.id, std::move(graphic));
}

void graphic::Destroy(FireEffect& fire)
{
	Graphics().Erase(fire.id);
}

void graphic::Update(float seconds)
{
	for (auto& [id, graphic] : Graphics().All())
	{
		const auto* fire = Get(id);
		if (fire == nullptr || fire->object == entt::null)
		{
			continue;
		}
		// (approximate: the original updates a fire graphic only when it is drawn, with a catch-up after 10 turns; here
		// every fire is updated, so off-screen fires keep their flames and puffs)
		if ((graphic->flags & 2) != 0)
		{
			UpdateFlames(*graphic, *fire, seconds);
		}
		if ((graphic->flags & 8) != 0)
		{
			UpdateSteam(*graphic, *fire, seconds);
		}
		if ((graphic->flags & 4) != 0)
		{
			UpdateSmoke(*graphic, *fire, seconds);
		}
		// With flag bit 4 and the light map, alpha = 0.6 x the fire fraction x (1 + 0.2 x noise(0.6 x (turn + fraction) +
		// (this & 0xFFFF))) x (1 - charring), only above 0, at most 1; stamped into the land light map at the object
		// (+ (10, 0, 10), centred, mode 1). (inferred) the stamp is at the object's position; (approximate) the noise is
		// CharringGlow's two sines (the original's smooth noise not ported), the turn fraction is left out and the fire's
		// id stands in for `this & 0xFFFF` (the fire graphic's pointer)
		if ((graphic->flags & 0x10) != 0 && graphic->lightMap)
		{
			const float x = 0.6f * static_cast<float>(game_clock::Turn()) + static_cast<float>(fire->id & 0xFFFF);
			const float noise = std::sin(x * 1.7f) * 0.6f + std::sin(x * 3.1f + 1.3f) * 0.4f;
			const float alpha = 0.6f * fire->FireFraction() * (1.0f + 0.2f * noise) * (1.0f - fire->charring);
			if (alpha > 0.0f)
			{
				// The translation of the drawn matrix: where the object is drawn
				land_light::AddStamp(DrawnObjectPosition(graphic->object) + glm::vec3(10.0f, 0.0f, 10.0f),
				                     graphics::frame_anim::FrameTexels(*graphic->lightMap, 0), graphic->lightMap->pitch, true,
				                     std::min(alpha, 1.0f), 1);
			}
		}
	}
}

void graphic::SetTurn(uint32_t turn)
{
	// OPENBLACK_FIRE_TRACE: what each burning object is drawing, so that the flames can be checked without a screenshot
	if (turn % 20 != 0 || !TraceEnabled())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& [id, graphic] : Graphics().All())
	{
		const auto* transform = registry.TryGet<const components::Transform>(graphic->object);
		const glm::vec3 position = transform != nullptr ? transform->position : glm::vec3(0.0f);
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Fire: graphic of fire {} object {} at ({:.1f}, {:.1f}, {:.1f}): {} of {} flames (first scale "
		                   "{:.2f} alpha {}), {} steam, {} smoke",
		                   id, static_cast<int>(graphic->object), position.x, position.y, position.z, graphic->flames.size(),
		                   graphic->maxFlames, graphic->flames.empty() ? 0.0f : graphic->flames.front().scale,
		                   graphic->flames.empty() ? 0 : graphic->flames.front().alpha, graphic->steam.size(),
		                   graphic->smoke.size());
	}
}

void graphic::Clear()
{
	Graphics().Clear();
}

std::optional<glm::u8vec3> graphic::TreeDrawColour(entt::entity object)
{
	const auto* fire = Find(object);
	if (fire == nullptr)
	{
		return std::nullopt;
	}
	// Drawn instead of the plain brightness tint: 50 by default, above 0.9 life max(50, 255 - (1 - life) x 2550)
	// (truncated), then the unsigned min with the frame's tree brightness (ecs::TreeBrightness, the same value an
	// unburnt tree is multiplied by); the colour x that >> 8
	const float life = life::LifeOf(object);
	uint32_t grey = 50;
	if (life > 0.9f)
	{
		const float value = 255.0f - (1.0f - life) * 2550.0f;
		grey = value < 50.0f ? 50u : static_cast<uint32_t>(value);
	}
	grey = std::min(grey, static_cast<uint32_t>(ecs::TreeBrightness()));
	const auto g = static_cast<uint8_t>(grey);
	// TODO(render): the original also forces the ALPHAREF of the tree's alpha tested primitives while it is drawn: the
	// render mode override = min(254, 230 + heat x 25 x (1 / 255)) truncated toward zero, heat = 255 if T > 1.5 Tc
	// else (T - Tc) x 255 / (0.5 Tc) truncated toward zero, reset after drawing; the render modes read it as ALPHAREF
	// (render_modes::AlphaRef `forced`), so the foliage thins out. Not ported: the trees are instanced and fs_object
	// takes ALPHAREF per draw (u_skyAlphaThreshold.y), so a burning tree would need its own draw call
	return glm::u8vec3(g, g, g);
}

uint8_t graphic::CharringGrey(const FireEffect& fire)
{
	// k = (charring x 255 truncated toward zero) & 0xFF, then each channel ((unsigned)(-175 k) >> 8) - 1 = 255 - ceil(175 k /
	// 256): 255 at k = 0, 80 at k = 255
	const int charring = static_cast<int>(fire.charring * 255.0f) & 0xFF;
	return static_cast<uint8_t>(255 - (charring * 175 + 255) / 256);
}

glm::u8vec3 graphic::CharringGlow(const FireEffect& fire, float turnTime)
{
	// A smooth noise at 0.6 x (turn + fraction) + (address & 0xFFFF) (inferred, the id instead of the address)
	const float x = 0.6f * turnTime + static_cast<float>(fire.id & 0xFFFF);
	// (approximate: two sines instead of the original's noise; the formula and its coefficients are the port's)
	const float noise = std::sin(x * 1.7f) * 0.6f + std::sin(x * 3.1f + 1.3f) * 0.4f;
	float value = 0.001f * fire.temperature * (1.0f + 0.2f * noise);
	value = std::clamp(value, 0.0f, 1.0f);
	const int c = static_cast<int>((1.0f - fire.charring) * value * 255.0f) & 0xFF;
	return glm::u8vec3(static_cast<uint8_t>(c * 180 / 256), static_cast<uint8_t>(c * 60 / 256), 0);
}
