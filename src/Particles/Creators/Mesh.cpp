/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "Mesh.h"

#include <cmath>

#include <algorithm>
#include <map>
#include <string>

#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/SkeletalPose.h"
#include "Camera/Camera.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysManager.h"
#include "Particles/PSysRegistry.h"
#include "Particles/SoundAction.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// MeshEnum: the MSH_* name in Data\AllMeshes.h (looked up as the sound actions' names), -1 if absent
int32_t MeshEnumValue(std::string_view name)
{
	const auto& names = EnumHeaderNames("AllMeshes.h").byName;
	const auto it = names.find(name);
	return it != names.end() ? it->second : -1;
}

/// A spell file's path (".\Data\Spells\Meshes\X.l3d") under the data folder ("Spells/Meshes/X.l3d")
std::string DataRelativePath(std::string path)
{
	std::replace(path.begin(), path.end(), '\\', '/');
	if (path.starts_with("./"))
	{
		path = path.substr(2);
	}
	if (path.size() > 5 && (path.starts_with("Data/") || path.starts_with("data/")))
	{
		path = path.substr(5);
	}
	return path;
}

/// A mesh file (".\Data\Spells\Meshes\X.l3d"), loaded once by its name
entt::id_type SharedMesh(std::string path)
{
	path = DataRelativePath(std::move(path));
	const auto id = entt::hashed_string(("psys/" + path).c_str()).value();
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return id;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (meshes.Contains(id))
	{
		return id;
	}
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		meshes.Load(id, resources::L3DLoader::FromDiskTag {},
		            fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / path));
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: mesh {}: {}", path, e.what());
	}
	return id;
}

/// With AnimEnum -1: the AnimFileName (".\Data\SPELLS\Anims\X.anm") loaded whole, then fixed up, when the file
/// exists. (openblack) once per path, kept by the animation manager; 0 when there is no file
entt::id_type SharedAnim(std::string path)
{
	path = DataRelativePath(std::move(path));
	if (path.empty() || path == "NULL_STRING")
	{
		return 0;
	}
	const auto id = entt::hashed_string(("psys/" + path).c_str()).value();
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return id;
	}
	auto& animations = Locator::resources::value().GetAnimations();
	if (animations.Contains(id))
	{
		return id;
	}
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		animations.Load(id, resources::L3DAnimLoader::FromDiskTag {},
		                fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / path));
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: animation {}: {}", path, e.what());
		return 0;
	}
	return id;
}

/// The clip of an animated mesh creator, if it is loaded. (openblack guard) without a clip (missing file) or with a
/// mesh without bones the atom is drawn in its rest pose; the original would read the null clip
const L3DAnim* CreatorClip(const MeshCreator& creator)
{
	if (!creator.animated || creator.animId == 0 || !Locator::resources::has_value())
	{
		return nullptr;
	}
	const auto& animations = Locator::resources::value().GetAnimations();
	return animations.Contains(creator.animId) ? &*animations.Handle(creator.animId) : nullptr;
}

std::unique_ptr<Creator> MakeMeshCreator(const Object& object)
{
	auto creator = std::make_unique<MeshCreator>();
	ReadCreatorProperties(object, *creator);
	creator->kind = Creator::Kind::Mesh;
	creator->animTextured = object.className == "ParticleMeshCreatorAnimTextured";
	// MeshEnum != -1 -> the mesh pack's MeshEnum (0 outside the pack), else the shared mesh file
	const int32_t meshEnum = MeshEnumValue(object.String("MeshEnum"));
	if (meshEnum >= 0)
	{
		creator->meshId = resources::HashIdentifier(static_cast<MeshId>(meshEnum));
	}
	else if (const auto file = object.String("MeshFileName"); !file.empty())
	{
		creator->meshId = SharedMesh(file);
	}
	creator->faceCamera = object.Bool("FaceCamera", false);
	creator->faceCameraSprite = object.Bool("FaceCameraSprite", false);
	// the base mesh creator's defaults: MeshEnum -1, HeightStretch 1, FaceCamera / FaceCameraSprite / the pulse 0
	creator->heightStretch = object.Float("HeightStretch", 1.0f);
	creator->scriptHighlightPulse = object.Bool("UseScriptHightlightPulse", false);
	// ParticleMeshCreator's defaults: MeshChangeMaterialProps 1, double-sided 1, the rest 0 (AnimTextured sets the
	// same). With MeshChangeMaterialProps every material of the mesh gets the creator's material properties (additive,
	// Z write, double-sided); without it the L3D materials are drawn as they are (opaque).
	creator->changeMaterialProps = object.Bool("MeshChangeMaterialProps", true);
	if (object.className == "ParticleAnimCreator")
	{
		// ParticleAnimCreator (defaults: UseAdditiveAlpha 0, Z 0, double-sided 1, change and alpha 1, no
		// MeshChangeMaterialProps property): the pack mesh and the mesh file both get the material properties
		creator->changeMaterialProps = true;
		// The clip (set on each particle's object) and the defaults: SpeedUpFactor 1, PlayAnim 0, RandomiseInitFrame 0.
		// Every spell file (SF_Forest, SF_Butterflies, SF_ButterfliesOnObject) names an AnimFileName and no AnimEnum.
		// (pending) AnimEnum (the animation pack, pack[0] out of range); the blend to MeshFileName1/2 (both needed)
		// between FrameToStartBlend and FrameToEndBlend: NULL_STRING in every file; UseSuperSortedPolys (0 everywhere),
		// UseDynamicLighting and NeverClip. UseGlobalAlpha (1 in every file) is honoured: Instance::globalAlpha
		creator->animated = true;
		creator->animId = SharedAnim(object.String("AnimFileName"));
		creator->speedUpFactor = object.Float("SpeedUpFactor", 1.0f);
		creator->animPlay = object.Bool("PlayAnim", false);
		creator->animRandomInitFrame = object.Bool("RandomiseInitFrame", false);
		if (creator->animId == 0)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: {} {}: no animation ({})", object.className, object.name,
			                   object.String("AnimFileName"));
		}
	}
	// UseGlobalAlpha: 0 by default for ParticleMeshCreator / AnimTextured, 1 for ParticleAnimCreator. The comment
	// above on UseGlobalAlpha: every spell file of a ParticleAnimCreator sets it to 1
	creator->useGlobalAlpha = object.Bool("UseGlobalAlpha", creator->animated);
	creator->additive = creator->changeMaterialProps && object.Bool("UseAdditiveAlpha", false);
	creator->writeDepth = object.Bool("MaterialUpdateZBuffer", false);
	creator->doubleSided = object.Bool("MaterialSetDoubleSided", true);
	creator->neverClip = object.Bool("NeverClip", false);
	// CastHumanShadow: ParticleMeshCreator's only; AnimTextured reads NeverClip in its place and ParticleAnimCreator has
	// none. Its shadow: MeshCreator::castHumanShadow, mesh_atoms::HumanShadows
	creator->castHumanShadow = !creator->animTextured && !creator->animated && object.Bool("CastHumanShadow", false);
	// DrawWithLandscapeColor: ParticleMeshCreator reads it (each particle keeps it as a flag its draw tests), and
	// ParticleMeshCreatorAnimTextured reads it too (its last property, default 0), into the same particle flag. So the
	// tornado funnel's DrawWithLandscapeColor=1 is honoured
	creator->drawWithLandscapeColour = object.Bool("DrawWithLandscapeColor", false);
	creator->drawCutByPlane = object.Bool("DrawCutByPlane", false);
	if (creator->animTextured)
	{
		// the defaults: 64 x 64, 1 frame at 1 fps, FrameRateMax 10, StretchY 1
		creator->textureWidth = std::max(1, object.Int("TextureWidth", 64));
		creator->textureHeight = std::max(1, object.Int("TextureHeight", 64));
		creator->slideU = object.Bool("SlideU", false);
		creator->slideV = object.Bool("SlideV", false);
		creator->randomiseInitFrame = object.Bool("RandomiseInitFrame", false);
		creator->randomiseFrameRate = object.Bool("RandomiseFrameRate", false);
		creator->frameRate = object.Float("FrameRate", 1.0f);
		creator->frameRateMax = object.Float("FrameRateMax", 10.0f);
		creator->numFrames = std::max(1, object.Int("NumFrames", 1));
		creator->playAnimation = object.Bool("PlayAnim", false);
		creator->initialOffsetFrac = object.Float("InitialOffsetFrac", 0.0f);
		creator->stretchY = object.Float("StretchY", 1.0f);
	}
	if (creator->meshId == 0 ||
	    (Locator::resources::has_value() && !Locator::resources::value().GetMeshes().Contains(creator->meshId)))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: {} {}: no mesh ({} / {})", object.className, object.name,
		                   object.String("MeshEnum"), object.String("MeshFileName"));
	}
	return creator;
}
} // namespace

float openblack::psys::AnimFrameRate(int32_t clipMs, float speedUpFactor) noexcept
{
	// (openblack guard) a clip of 0 ms: no rate, where the original divides by it
	if (clipMs <= 0)
	{
		return 0.0f;
	}
	return 1000.0f / static_cast<float>(clipMs) * speedUpFactor * 1000.0f;
}

int32_t openblack::psys::AnimCycleTime(int32_t clipMs, int frame) noexcept
{
	return clipMs * frame / k_AnimFrames;
}

void MeshCreator::InitAtom(Effect& effect, Atom& atom) const
{
	// The mesh is fetched at the creator's first particle, and with it the material properties rewrite every material
	// of the mesh: a pack mesh if MeshChangeMaterialProps (the same for AnimTextured), a mesh file if
	// MeshChangeMaterialProps when it is first loaded, ParticleAnimCreator's pack mesh (always on, no property) or its
	// mesh file (always). The pack meshes are changed in place, for every user, as in the original. (approximate) a mesh
	// file two creators load with different properties: each creator applies its own here, where the original kept the
	// first loader's
	if (!materialsSet)
	{
		materialsSet = true;
		if (changeMaterialProps && meshId != 0 && Locator::resources::has_value() &&
		    Locator::resources::value().GetMeshes().Contains(meshId))
		{
			// the material properties: additive, Z write, double-sided, change, alpha
			Locator::resources::value().GetMeshes().Handle(meshId)->SetMaterialProperties({.additive = additive,
			                                                                               .zWrite = writeDepth,
			                                                                               .doubleSided = doubleSided,
			                                                                               .change = true,
			                                                                               .alpha = materialAlpha});
		}
	}
	if (animated)
	{
		// ParticleAnimCreator: the particle's own animated object, then on the atom the rate from the clip's length
		// (AnimFrameRate), 1000 frames, PlayAnim and LoopAnim, and with RandomiseInitFrame a first frame of
		// Rand(1000)
		const auto* clip = CreatorClip(*this);
		atom.frameRate = AnimFrameRate(clip != nullptr ? clip->GetDurationMs() : 0, speedUpFactor);
		atom.playAnim = animPlay;
		if (animRandomInitFrame)
		{
			atom.frame = static_cast<float>(effect.Rand(k_AnimFrames));
		}
		return;
	}
	if (!animTextured)
	{
		// ParticleMeshCreator: a plain mesh particle. Its CastHumanShadow node is the shadow list's entry of the atom,
		// made when the shadow list first meets it in HumanShadows (Mesh.h, castHumanShadow)
		return;
	}
	// ParticleMeshCreatorAnimTextured
	float rate = frameRate;
	if (randomiseFrameRate && frameRateMax != frameRate)
	{
		rate = frameRate + effect.Random(frameRateMax - frameRate);
	}
	atom.frame = 0.0f;
	if (slideU || slideV)
	{
		// the slide runs over 1000 frames: the rate x 1000, and InitialOffsetFrac is the first frame (0..999)
		rate *= 1000.0f;
		if (initialOffsetFrac != 0.0f)
		{
			atom.frame = static_cast<float>(static_cast<int>(std::clamp(initialOffsetFrac * 1000.0f, 0.0f, 999.0f)));
		}
	}
	if (randomiseInitFrame)
	{
		// Rand(N), N = NumFrames or the slide's 1000
		atom.frame = static_cast<float>(effect.Rand(FramesPerAtom()));
	}
	atom.frameRate = rate;
	atom.playAnim = playAnimation;
	atom.stretch = stretchY;
}

glm::vec2 MeshCreator::UvOffset(int frame) const
{
	// frame_anim::AnimTexturedCell: cells of W x H pixels in rows of
	// 256 / W, or the slide over the 1000 frames
	return graphics::frame_anim::AnimTexturedCell(frame, {textureWidth, textureHeight, slideU, slideV, FramesPerAtom()});
}

bool mesh_atoms::Any()
{
	for (const auto& drawable : manager::Collect(Creator::Kind::Mesh))
	{
		for (const auto& atom : drawable.atoms)
		{
			const auto* creator = dynamic_cast<const MeshCreator*>(atom.creator);
			if (creator != nullptr && creator->meshId != 0)
			{
				return true;
			}
		}
	}
	return false;
}

namespace
{
/// What this module keeps between calls (Locator::particleSystem)
struct MeshAtomsState
{
	/// The shadow list of this frame (Mesh.h, castHumanShadow): refilled by every Collect, as the original empties it
	/// after each shadow pass and the draw pushes the drawn particles again
	std::vector<mesh_atoms::HumanShadow> humanShadows {};
};

MeshAtomsState& MeshAtomsData()
{
	return openblack::Locator::particleSystem::value().Module<MeshAtomsState>();
}
} // namespace

const std::vector<mesh_atoms::HumanShadow>& mesh_atoms::HumanShadows()
{
	return MeshAtomsData().humanShadows;
}

std::vector<mesh_atoms::Instance> mesh_atoms::Collect()
{
	auto& state = MeshAtomsData();
	std::vector<Instance> result;
	state.humanShadows.clear();
	const glm::vec3 cameraOrigin = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3(0.0f);
	const glm::vec3* camera = Locator::camera::has_value() ? &cameraOrigin : nullptr;
	for (const auto& drawable : manager::Collect(Creator::Kind::Mesh))
	{
		for (const auto& atom : drawable.atoms)
		{
			const auto* creator = dynamic_cast<const MeshCreator*>(atom.creator);
			if (creator == nullptr || creator->meshId == 0)
			{
				continue;
			}
			// the animated particle's draw: the whole frame drawn (0..999) gives the clip's cycle time, set on its
			// object, which the object's draw poses the bones at. Frame 0 is not drawn: the draw leaves before it
			const L3DAnim* clip = CreatorClip(*creator);
			int animFrame = 0;
			if (creator->animated)
			{
				animFrame = graphics::frame_anim::ParticleFrameIndex(atom.frame, creator->FramesPerAtom(), creator->loopAnim);
				if (animFrame == 0)
				{
					continue;
				}
			}
			// the PSR matrix (rotation x scale, the Y axis x the stretch): the original's rows r0, r1, r2 are the
			// columns here
			glm::mat3 axes = atom.rotation * atom.scale;
			axes[1] *= atom.stretch;
			// FaceCamera first, else FaceCameraSprite (no spell file sets it), which starts again from the identity x
			// the scale
			if (creator->faceCamera && camera != nullptr)
			{
				graphics::billboard::ParticleYaw(axes, atom.position, *camera, creator->heightStretch);
			}
			else if (creator->faceCameraSprite && camera != nullptr)
			{
				axes = graphics::billboard::FullSprite(atom.position, *camera, atom.scale);
			}
			glm::mat4 model(axes);
			model[3] = glm::vec4(atom.position, 1.0f);
			glm::vec2 uv(0.0f);
			if (creator->animTextured)
			{
				// the whole frame drawn: looped within N or clamped to its last
				uv = creator->UvOffset(
				    graphics::frame_anim::ParticleFrameIndex(atom.frame, creator->FramesPerAtom(), creator->loopAnim));
			}
			// UseScriptHightlightPulse (A x the script highlight's pulse): not ported
			const float alpha = std::clamp(atom.alpha / 255.0f, 0.0f, 1.0f);
			// (inferred: port routing) translucent, with the fading meshes, when additive or drawn with the global alpha
			// and not fully opaque (the routing is the port's; the modes are the original's). DrawCutByPlane: the
			// particle mesh, a static object, is drawn through the cutting path, which clips each triangle on the CPU
			// against the plane and lights it as the animated objects' cut does (docs/bw1-notes/rendering-objects.md,
			// "Cutting by the water plane"): Instance::cutByPlane, drawn by the renderer with sea_pass::CutAtoms
			// The particle's global alpha flag (Mesh.h globalAlpha): AnimTextured's and ParticleAnimCreator's
			// UseGlobalAlpha, never for ParticleMeshCreator. Without it the atom is drawn with its materials' own modes:
			// with the other meshes, its alpha (1 - [0][3]) only showing in the primitives that blend. The additive ones
			// are mode 13 / 12 in both tables (entries 12 and 13 are the same)
			const bool globalAlpha = creator->animTextured || creator->animated ? creator->useGlobalAlpha : false;
			const bool translucent = creator->additive || (globalAlpha && alpha < 1.0f);
			result.push_back({creator->meshId, model, alpha, uv, translucent, creator->additive, atom.colour,
			                  creator->drawWithLandscapeColour, atom.specular, globalAlpha});
			// the particle's cut flag from the creator's DrawCutByPlane
			result.back().cutByPlane = creator->drawCutByPlane;
			result.back().path = drawable.path;
			result.back().effect = drawable.effect;
			result.back().atom = atom.atom;
			// the draw pushes the particle's object on the shadow list; its matrix is the drawn one (after the
			// face-camera paths) and its scale the PSR's: the atom's drawn scale
			if (creator->castHumanShadow && atom.atom != nullptr)
			{
				state.humanShadows.push_back({atom.atom, creator->meshId, model, atom.scale});
			}
			// (openblack) no pose for an atom of alpha 0: the renderer does not draw it
			if (clip != nullptr && alpha > 0.0f && Locator::resources::has_value())
			{
				const auto& meshes = Locator::resources::value().GetMeshes();
				if (meshes.Contains(creator->meshId))
				{
					const auto mesh = meshes.Handle(creator->meshId);
					if (mesh->IsBoned())
					{
						graphics::ComputePose(*mesh, *clip, static_cast<float>(AnimCycleTime(clip->GetDurationMs(), animFrame)),
						                      result.back().pose);
					}
				}
			}
		}
	}
	return result;
}

void openblack::psys::RegisterMeshCreators()
{
	RegisterCreator("ParticleMeshCreator", MakeMeshCreator);
	RegisterCreator("ParticleMeshCreatorAnimTextured", MakeMeshCreator);
	RegisterCreator("ParticleAnimCreator", MakeMeshCreator); // the forest's butterflies and bats
}
