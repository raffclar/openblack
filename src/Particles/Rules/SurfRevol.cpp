/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SurfRevol.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <memory>
#include <numbers>
#include <unordered_map>

#include "3D/FrameAnim.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandMorph.h" // (and glm::inverse, through glm/mat4x4.hpp)
#include "ECS/Systems/ParticleSystemInterface.h"
#include "Locator.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysManager.h"
#include "Particles/PSysRegistry.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// The per-collection data and the draw object (a mesh with rotating UVs) of the one atom it makes. It is the atom's
/// creator, so the draw finds its mesh.
struct SurfRevolCreator final: Creator
{
	SurfMesh mesh;
	std::vector<glm::vec2> savedUVs;
	std::vector<glm::vec3> savedPositions;
	/// The draw object's UV scroll clock: the rule adds the step to its `destination` and GameUpdate shifts it into
	/// `current` with the one before in `previous`, so the draw interpolates between two steps. `period` is
	/// TextureWidth / 256, TextureHeight / 256
	graphics::frame_anim::RotatingUvClock uv;
	bool raiseAboveLandscape {false};
	bool clampToLandscape {false}; ///< off by default; set from the rule's ClampToLandscape
	bool doubleSided {false};
};

/// What this module keeps between calls (Locator::particleSystem)
struct SurfRevolState
{
	SurfRevolState() = default;
	// move only: it owns the creators (a container of unique_ptr still claims to be copyable)
	SurfRevolState(const SurfRevolState&) = delete;
	SurfRevolState& operator=(const SurfRevolState&) = delete;
	SurfRevolState(SurfRevolState&&) noexcept = default;
	SurfRevolState& operator=(SurfRevolState&&) noexcept = default;
	~SurfRevolState() = default;

	/// The creators, by the effect that owns them (freed once the effect is gone)
	std::unordered_map<uint32_t, std::vector<std::unique_ptr<SurfRevolCreator>>> creators {};
};

SurfRevolState& SurfRevolData()
{
	return openblack::Locator::particleSystem::value().Module<SurfRevolState>();
}

/// The twists' wobble frequency: no property sets it, always 1
constexpr float k_WobbleFrequency = 1.0f;

uint32_t Pack(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
{
	return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

/// A positive float truncated to a byte
uint8_t ToByte(float value)
{
	return static_cast<uint8_t>(static_cast<int>(value));
}

/// When the surface is made, with the atom's local frame (its rotation x baseScale x ruleScale, Y also x the stretch)
/// in the collection's hierarchy: the mesh goes to the world, is cut along the land's cells and raised there
/// (land_morph::RaiseAboveLandscape) and comes back with the inverse, so the draw moves the raised mesh with the atom
/// (and scales it with a later UR_ChangeScale, as the original's local deltas do).
/// (approximate) The frame is the one Collect draws with (rotation x baseScale x ruleScale, no stretch): the
/// original's drawn matrix is taken as the same one, and Collect leaves the stretch out. Both users have stretch 1
/// (SF_TeleportVortex, SF_SpellDispenserVortex: no StretchVertically, no rule writes it).
void RaiseAboveLandscape(const Effect& effect, const Collection& collection, const Atom& atom, SurfMesh& mesh)
{
	const glm::mat3 frame = atom.rotation * (atom.baseScale * atom.ruleScale);
	land_morph::Primitive primitive;
	primitive.positions.reserve(mesh.positions.size());
	for (const auto& p : mesh.positions)
	{
		primitive.positions.push_back(effect.LocalToGlobal(collection, atom.position + frame * p));
	}
	primitive.uvs = mesh.uvs;
	primitive.diffuse = mesh.colours;
	primitive.specular = mesh.speculars;
	primitive.indices.assign(mesh.indices.begin(), mesh.indices.end());
	const auto origin = effect.LocalToGlobal(collection, atom.position); // the frame's position, x and z
	land_morph::RaiseAboveLandscape(land_morph::CurrentAltitude(), std::span(&primitive, 1), glm::vec2(origin.x, origin.z));
	if (primitive.positions.size() > 0xFFFFu)
	{
		return; // (port guard) the surface's indices are 16 bits; the two users stay far below
	}
	const auto back = glm::inverse(frame);
	mesh.positions.clear();
	for (const auto& p : primitive.positions)
	{
		mesh.positions.push_back(back * (effect.GlobalToLocal(collection, p) - atom.position));
	}
	mesh.uvs = std::move(primitive.uvs);
	mesh.colours = std::move(primitive.diffuse);
	mesh.speculars = std::move(primitive.specular);
	mesh.indices.assign(primitive.indices.begin(), primitive.indices.end());
}

class SurfRevol final: public Modifier
{
public:
	/// the defaults are the original's
	explicit SurfRevol(const Object& object)
	    : nextGroups(object.Array("NextGroups"))
	    , texture(TextureBaseName(object.String("TextureFileName")))
	    , additive(object.Bool("UseAdditiveAlpha", false))
	    , writeDepth(object.Bool("MaterialUpdateZBuffer", false))
	    , doubleSided(object.Bool("MaterialSetDoubleSided", true))
	    , textureHeight(object.Int("TextureHeight", 256))
	    , textureWidth(object.Int("TextureWidth", 256))
	    , speedU(object.Float("SpeedU", 0.1f))
	    , speedV(object.Float("SpeedV", 0.1f))
	    , numU(object.Int("NumU", 10))
	    , numV(object.Int("NumV", 10))
	    , fadeAlphas(object.Bool("FadeAlphas", false))
	    , changeSpecColour(object.Bool("ChangeSpecColor", true))
	    , maxUVChange(object.Float("MaxUVChange", 1.0f))
	    , maxVertexChange(object.Float("MaxVertexChange", 1.0f))
	    , raiseAboveLandscape(object.Bool("DoRaiseAboveLandscape", false))
	    , clampToLandscape(object.Bool("ClampToLandscape", false))
	    , alphaFadeIn(object.Float("AlphaFadeIn", 0.4f))
	    , alphaFadeOut(object.Float("AlphaFadeOut", 0.4f))
	    , scale(object.Float("Scale", 1.0f))
	    , functionIndex(object.Int("FunctionIndex", 0))
	    , colour {static_cast<uint8_t>(object.Int("ColorR", 255)), static_cast<uint8_t>(object.Int("ColorG", 255)),
	              static_cast<uint8_t>(object.Int("ColorB", 255)), static_cast<uint8_t>(object.Int("ColorA", 255))}
	{
		// ClampToLandscape is drawn in Collect. UseLighting (the normals), UseSphere and MaterialUseTextureAlpha are not
		// used here: the pool is drawn unlit with the texture's alpha.
		// HeightAboveLandscape and RaiseAboveLandscapeRadius are defined but nothing reads them.
	}

	[[nodiscard]] bool Creates() const override { return true; } // an atom create rule

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		auto* creator = CreatorOf(collection);
		if (slot.first)
		{
			slot.first = false;
			creator = MakeSurface(effect, collection);
		}
		if (creator == nullptr)
		{
			return false;
		}
		// The size: the unit-radius mesh goes through the atom's drawn matrix only (each vertex times the atom's drawn
		// position / scale / rotation). The atoms' post-update already puts the hierarchy into it (parent x local,
		// scale = base x rule x the parent's), which Effect::PostUpdate does into atom.current.scale. So the radius is
		// this atom's 1 x Scale x the group-2 parent's InitialScale x UR_ChangeScale: 6 m for SF_SpellDispenserVortex
		// (6 x 1), 10 m for SF_TeleportVortex (2 x 5); the parent's scale must not be applied a second time.
		// !DoRaiseAboveLandscape: the twists breathe with sin(fmod(collection age x k_WobbleFrequency, 2 pi))
		if (!raiseAboveLandscape)
		{
			const float amount =
			    std::sin(std::fmod(effect.CollectionAge(collection) * k_WobbleFrequency, 2.0f * std::numbers::pi_v<float>));
			if (maxVertexChange != 0.0f)
			{
				surf_revol::TwistVertices(creator->mesh, creator->savedPositions, maxVertexChange, amount);
			}
			if (maxUVChange != 0.0f)
			{
				surf_revol::TwistUVs(creator->mesh, creator->savedUVs, maxUVChange, amount);
			}
		}
		// the UV scroll: destination += dt x (SpeedU, SpeedV) (dt, the step). Nothing wraps it here: GameUpdate keeps
		// the pair within two periods and hands it on (it runs once a step per atom, at the end of the atoms'
		// post-update - after the rules - so here, right after the step, is the same place), and the draw takes the
		// period off the interpolated value
		const float dt = effect.GetDt();
		creator->uv.destination += glm::vec2(dt * speedU, dt * speedV);
		creator->uv.GameUpdate();
		return true;
	}

private:
	SurfRevolCreator* CreatorOf(const Collection& collection) const
	{
		for (const auto& atom : collection.atoms)
		{
			if (auto* creator = dynamic_cast<const SurfRevolCreator*>(atom->creator); creator != nullptr)
			{
				return const_cast<SurfRevolCreator*>(creator);
			}
		}
		return nullptr;
	}

	/// The first step of the collection: material, atom, mesh
	SurfRevolCreator* MakeSurface(Effect& effect, Collection& collection) const
	{
		const auto id = manager::IdOf(&effect);
		auto owned = std::make_unique<SurfRevolCreator>();
		auto* creator = owned.get();
		creator->kind = Creator::Kind::Other;
		creator->className = "ZR_SurfRevol";
		creator->texture = texture;
		creator->additive = additive; // a mode 6 material with the texture and these properties
		creator->writeDepth = writeDepth;
		creator->doubleSided = doubleSided;
		creator->raiseAboveLandscape = raiseAboveLandscape;
		creator->clampToLandscape = clampToLandscape;
		creator->initialScale = 1.0f;
		creator->r = colour[0];
		creator->g = colour[1];
		creator->b = colour[2];
		creator->a = colour[3];
		SurfRevolData().creators[id].push_back(std::move(owned));
		// the atom and its draw object, into the collection with NextGroups; scale x= Scale; colour = ColorA/R/G/B;
		// specular = SpecColorR/G/B
		auto& atom = effect.NewAtom(collection, creator, nextGroups);
		atom.baseScale *= scale;
		atom.colour = {colour[0], colour[1], colour[2], colour[3]};
		// ChangeSpecColor: the effect's player colour, else 0xFFFFFFFF
		uint32_t player = 0xFFFFFFFFu;
		if (changeSpecColour && effect.GetPlayer() >= 0)
		{
			player = surf_revol::PlayerColour(effect.GetPlayer());
		}
		creator->mesh =
		    surf_revol::Build(numU, numV, functionIndex, fadeAlphas, alphaFadeIn, alphaFadeOut, changeSpecColour, player);
		creator->uv.period = glm::vec2(static_cast<float>(textureWidth) / 256.0f, static_cast<float>(textureHeight) / 256.0f);
		surf_revol::ScaleUVs(creator->mesh, creator->uv.period.x, creator->uv.period.y);
		creator->savedUVs = creator->mesh.uvs;
		creator->savedPositions = creator->mesh.positions;
		// DoRaiseAboveLandscape: the twists once with amount 1, then the mesh is cut and draped over the land, once
		if (raiseAboveLandscape)
		{
			if (maxVertexChange != 0.0f)
			{
				surf_revol::TwistVertices(creator->mesh, creator->savedPositions, maxVertexChange, 1.0f);
			}
			if (maxUVChange != 0.0f)
			{
				surf_revol::TwistUVs(creator->mesh, creator->savedUVs, maxUVChange, 1.0f);
			}
			RaiseAboveLandscape(effect, collection, atom, creator->mesh);
		}
		return creator;
	}

	std::vector<int> nextGroups;
	std::string texture;
	bool additive;
	bool writeDepth;
	bool doubleSided;
	int textureHeight;
	int textureWidth;
	float speedU;
	float speedV;
	int numU;
	int numV;
	bool fadeAlphas;
	bool changeSpecColour;
	float maxUVChange;
	float maxVertexChange;
	bool raiseAboveLandscape;
	bool clampToLandscape;
	float alphaFadeIn;
	float alphaFadeOut;
	float scale;
	int functionIndex;
	std::array<uint8_t, 4> colour;
};
} // namespace

glm::vec2 surf_revol::Profile(int functionIndex, float t)
{
	switch (static_cast<SurfProfile>(functionIndex))
	{
	case SurfProfile::Funnel:
		return {t, (std::sqrt(t) - 1.0f) * 3.0f};
	case SurfProfile::FunnelSpout:
		return {1.5f * t, (std::sqrt(2.0f * t) - 1.0f) * 3.0f};
	case SurfProfile::FunnelParab:
		return {t, (t * t - 1.0f) * 3.0f};
	case SurfProfile::Disk:
	default:
		return {t, 0.0f};
	}
}

SurfMesh surf_revol::Build(int numU, int numV, int functionIndex, bool fadeAlphas, float fadeIn, float fadeOut,
                           bool changeSpecColour, uint32_t playerColour)
{
	SurfMesh mesh {
	    .numU = std::max(numU, 2), // (port guard) at least two rows each way; the ctor defaults are 10
	    .numV = std::max(numV, 2),
	};
	const float du = 1.0f / static_cast<float>(mesh.numU - 1);
	const float dv = 1.0f / static_cast<float>(mesh.numV - 1);
	const auto count = static_cast<size_t>(mesh.numU * mesh.numV);
	mesh.positions.reserve(count);
	mesh.uvs.reserve(count);
	mesh.colours.reserve(count);
	mesh.speculars.reserve(count);
	for (int j = 0; j < mesh.numV; ++j)
	{
		const float t = static_cast<float>(j) * dv;
		const auto profile = Profile(functionIndex, t);
		uint32_t colour = 0xFFFFFFFFu;
		uint32_t specular = 0;
		if (fadeAlphas)
		{
			float rgb = 1.0f;
			float alpha = 1.0f;
			if (t < fadeIn)
			{
				rgb = t < 0.0f ? 0.0f : t / fadeIn;
			}
			else
			{
				const float limit = 1.0f - fadeOut;
				if (t > limit)
				{
					alpha = t > 1.0f ? 0.0f : 1.0f - (t - limit) / (1.0f - limit);
				}
			}
			const uint8_t c = ToByte(rgb * 255.0f);
			colour = Pack(ToByte(alpha * 255.0f), c, c, c);
			if (changeSpecColour)
			{
				// the player colour's channels x (255 (1 - rgb)) >> 8, its own alpha
				const uint32_t f = ToByte((1.0f - rgb) * 255.0f);
				const uint32_t red = (((playerColour & 0xFF0000u) * f) & 0xFF0000FFu) >> 8;
				const uint32_t green = (((playerColour & 0xFF00u) * f) & 0xFF0000u) >> 8;
				const uint32_t blue = (((playerColour & 0xFFu) * f) & 0xFF00u) >> 8;
				specular = ((red | green | blue) & 0x00FFFFFFu) | (playerColour & 0xFF000000u);
			}
		}
		for (int i = 0; i < mesh.numU; ++i)
		{
			const float u = static_cast<float>(i) * du;
			const float angle = u * 6.28319f; // 2 pi, as the original rounds it
			// the Y rotation rows (c, 0, s), (0, 1, 0), (-s, 0, c) applied to (r, y, 0)
			mesh.positions.emplace_back(profile.x * std::cos(angle), profile.y, profile.x * std::sin(angle));
			mesh.uvs.emplace_back(u, t);
			mesh.colours.push_back(colour);
			mesh.speculars.push_back(specular);
		}
	}
	for (int j = 0; j + 1 < mesh.numV; ++j)
	{
		const int b = j * mesh.numU;
		for (int i = 0; i + 1 < mesh.numU; ++i)
		{
			const auto U = mesh.numU;
			for (const int k : {b + U + i, b + i, b + U + 1 + i, b + U + 1 + i, b + i, b + i + 1})
			{
				mesh.indices.push_back(static_cast<uint16_t>(k));
			}
		}
	}
	return mesh;
}

void surf_revol::ScaleUVs(SurfMesh& mesh, float u, float v)
{
	for (auto& uv : mesh.uvs)
	{
		uv.x *= u;
		uv.y *= v;
	}
}

void surf_revol::TwistUVs(SurfMesh& mesh, const std::vector<glm::vec2>& original, float maxUVChange, float amount)
{
	if (original.size() != mesh.uvs.size() || mesh.numV < 2)
	{
		return;
	}
	size_t k = 0;
	for (int j = 0; j < mesh.numV; ++j)
	{
		const float f = 1.0f - static_cast<float>(j) / static_cast<float>(mesh.numV - 1);
		const float change = f * f * maxUVChange * amount;
		for (int i = 0; i < mesh.numU; ++i, ++k)
		{
			mesh.uvs[k].x = original[k].x + change;
		}
	}
}

void surf_revol::TwistVertices(SurfMesh& mesh, const std::vector<glm::vec3>& original, float maxVertexChange, float amount)
{
	if (original.size() != mesh.positions.size() || mesh.numV < 2)
	{
		return;
	}
	size_t k = 0;
	for (int j = 0; j < mesh.numV; ++j)
	{
		const float angle = static_cast<float>(j) / static_cast<float>(mesh.numV - 1) * maxVertexChange * amount;
		const float c = std::cos(angle);
		const float s = std::sin(angle);
		for (int i = 0; i < mesh.numU; ++i, ++k)
		{
			const auto& p = original[k];
			mesh.positions[k] = glm::vec3(c * p.x - s * p.z, p.y, c * p.z + s * p.x);
		}
	}
}

uint32_t surf_revol::PlayerColour(int player)
{
	// the seven player colours and the neutral one; the original swaps them by land for the story's opponents (land 1:
	// player 1 -> 6; land 2: 1 -> 4, 2 -> 5; land 3: 1 -> 5; lands 4, 5: 1 -> 6). (inferred) The port's PlayerNames
	// are the player numbers; the land remap is not applied (the town belief sprites do the same).
	static constexpr std::array<uint32_t, 8> k_Colours = {0xFFFF4646, 0xFF47FF54, 0xFFE347FF, 0xFF47F9FF,
	                                                      0xFFFFFD47, 0xFF4777FF, 0xFFFFA247, 0xFF000000};
	return k_Colours[static_cast<size_t>(std::clamp(player, 0, 7))] | 0xFF000000u; // alpha always 0xFF
}

void surf_revol::PruneCreators()
{
	auto& state = SurfRevolData();
	// the creators of the effects that are gone go with them
	for (auto it = state.creators.begin(); it != state.creators.end();)
	{
		it = manager::Find(it->first) == nullptr ? state.creators.erase(it) : std::next(it);
	}
}

std::vector<surf_revol::Surface> surf_revol::Collect(const manager::FrameInputs& inputs)
{
	std::vector<Surface> result;
	const auto ground = land_morph::CurrentAltitude();
	for (const auto& drawable : manager::Collect(Creator::Kind::Other, inputs))
	{
		for (const auto& atom : drawable.atoms)
		{
			const auto* creator = dynamic_cast<const SurfRevolCreator*>(atom.creator);
			if (creator == nullptr || creator->mesh.positions.empty())
			{
				continue;
			}
			Surface surface {
			    .texture = creator->texture,
			    .additive = creator->additive,
			    .writeDepth = creator->writeDepth,
			    .doubleSided = creator->doubleSided,
			    .indices = creator->mesh.indices,
			    .origin = drawable.origin, // the key of the effect's Z object
			    .path = drawable.path,
			    .effect = drawable.effect,
			    .atom = atom.atom,
			};
			const float scale = atom.scale; // the drawn scale, hierarchy included
			const float centreLand = creator->clampToLandscape ? ground(glm::vec2(atom.position.x, atom.position.z)) : 0.0f;
			const float atomAlpha = std::clamp(atom.alpha, 0.0f, 255.0f) / 255.0f;
			const auto& mesh = creator->mesh;
			// the rotating UVs (frame_anim::RotatingUvClock): lerp(previous -> current) with the draw fraction of the
			// step, then the period taken off while above it
			const auto uvOffset = creator->uv.Interpolated(drawable.t);
			surface.vertices.reserve(mesh.positions.size());
			for (size_t k = 0; k < mesh.positions.size(); ++k)
			{
				glm::vec3 p = atom.position + atom.rotation * (mesh.positions[k] * scale);
				// ClampToLandscape: every vertex of every primitive, every draw and without the cut,
				// y = (H(v) - H(the drawn matrix's position)) + y (land_morph::Bake)
				if (creator->clampToLandscape)
				{
					p.y = land_morph::Raised(ground, p, centreLand);
				}
				const uint32_t argb = mesh.colours[k];
				const float a = static_cast<float>(argb >> 24) * atomAlpha;
				const auto channel = [&](int shift, uint8_t atomChannel) {
					return static_cast<uint32_t>(static_cast<float>((argb >> shift) & 0xFFu) * static_cast<float>(atomChannel) /
					                             255.0f);
				};
				const uint32_t alpha = static_cast<uint32_t>(std::clamp(a, 0.0f, 255.0f));
				const uint32_t abgr = (alpha << 24) | (channel(0, atom.colour[2]) << 16) | (channel(8, atom.colour[1]) << 8) |
				                      channel(16, atom.colour[0]);
				const uint32_t spec = mesh.speculars[k];
				const uint32_t specAbgr = (alpha << 24) | ((spec & 0xFFu) << 16) | (spec & 0xFF00u) | ((spec >> 16) & 0xFFu);
				const glm::vec2 uv = mesh.uvs[k] + uvOffset;
				surface.vertices.push_back({p, uv, abgr, specAbgr});
			}
			result.push_back(std::move(surface));
		}
	}
	return result;
}

void openblack::psys::RegisterSurfRevolRules()
{
	RegisterModifier("ZR_SurfRevol", MakeModifierOf<SurfRevol>);
}
