/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS 1

#include "DesignedWaterfallSystem.h"

#include <string>
#include <string_view>

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLightFrame.h"
#include "3D/SnowCover.h"
#include "Common/X87Angle.h"
#include "ECS/Components/DrawImportance.h"
#include "ECS/Components/GroundPrint.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/NeverFades.h"
#include "ECS/Components/SlidingTexture.h"
#include "ECS/Components/SnowLevel.h"
#include "ECS/Components/Transform.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Particles/DrawnEffectSteps.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/SoundId.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
/// The model's id in the mesh cache, loaded from the misc folder the first time it is needed
std::optional<entt::id_type> LoadModel(std::string_view file)
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return std::nullopt;
	}
	const auto id = resources::HashIdentifier("misc/" + std::string(file));
	auto& meshes = Locator::resources::value().GetMeshes();
	if (meshes.Contains(id))
	{
		return id;
	}
	auto& fileSystem = Locator::filesystem::value();
	const auto path = fileSystem.GetPath<filesystem::Path::Misc>() / file;
	if (!fileSystem.Exists(path))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Model {} not found", path.generic_string());
		return std::nullopt;
	}
	meshes.Load(id, resources::L3DLoader::FromDiskTag {}, path);
	return id;
}

/// The place of a model of the fourth land: on the land under its place
glm::vec3 PlaceOf(const designed_waterfall::Scenery& scenery)
{
	const float ground = Locator::terrainSystem::has_value()
	                         ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(scenery.x, scenery.z))
	                         : 0.0f;
	return {scenery.x, ground, scenery.z};
}

/// Whether the drawn registry has the model that the system made. The drawn registry loses its things as the land
/// clears, and a new thing can then get the same entity, thus the model must also be the same.
bool Shown(entt::entity entity, entt::id_type mesh)
{
	if (!Locator::drawnRegistry::has_value() || entity == entt::null)
	{
		return false;
	}
	const auto& drawn = Locator::drawnRegistry::value();
	const auto* shown = drawn.Valid(entity) ? drawn.TryGet<const Mesh>(entity) : nullptr;
	return shown != nullptr && shown->id == mesh;
}

/// Makes a model of the fourth land in the drawn registry. The model goes onto the shape of the land: each point of it
/// moves up by how much higher the land is under the point than under the origin of the model. It never fades out
/// with the distance.
entt::entity MakeScenery(const designed_waterfall::Scenery& scenery, entt::id_type mesh, bool print)
{
	auto& drawn = Locator::drawnRegistry::value();
	const auto entity = drawn.Create();
	drawn.Assign<Transform>(entity, PlaceOf(scenery), x87::GameUprightTurn(scenery.turn), glm::vec3(scenery.scale));
	drawn.Assign<Mesh>(entity, mesh, static_cast<int8_t>(0), static_cast<int8_t>(-1));
	drawn.Assign<DrawImportance>(entity, DrawImportance {.importance = designed_waterfall::k_SceneryImportance});
	drawn.Assign<NeverFades>(entity);
	drawn.Assign<MorphWithTerrain>(entity);
	drawn.Assign<SnowLevel>(entity);
	if (print)
	{
		drawn.Assign<GroundPrint>(entity);
	}
	drawn.SetDirty();
	return entity;
}

/// The model shows the snow where it stands at once, with no slow change
void ShowSnow(entt::entity entity, glm::vec3 place)
{
	if (!Locator::snowSystem::has_value())
	{
		return;
	}
	const float depth = Locator::snowSystem::value().GetDepth(glm::vec2(place.x, place.z));
	Locator::drawnRegistry::value().Get<SnowLevel>(entity).level =
	    snow_cover::ObjectLevel(depth, snow_cover::k_ObjectRate, snow_cover::k_ObjectRate);
}
} // namespace

void DesignedWaterfallSystem::Reset()
{
	// The things of the land go, and the sound of the waterfall goes with them. The frames keep the model.
	RemoveSound();
	designed_waterfall::LandCleared(_presence);
	_pace = {};
}

void DesignedWaterfallSystem::UpdateFrame(int32_t landNumber, std::chrono::milliseconds frameRealTime,
                                          std::chrono::milliseconds frameGameTime, bool landDrawn)
{
	const auto step = particles::drawn_steps::StepTime(_pace, frameRealTime, frameGameTime, landDrawn);
	if (!landDrawn)
	{
		return;
	}
	const auto changes =
	    designed_waterfall::Arrive(_presence, landNumber, {.model = ModelShown(), .dinosaur = DinosaurShown()});
	if (changes.removeModel)
	{
		RemoveModel();
	}
	if (changes.removeSound)
	{
		RemoveSound();
	}
	if (changes.removeDinosaur)
	{
		RemoveDinosaur();
	}
	if (changes.makeModel)
	{
		MakeModel(landNumber);
	}
	if (changes.makeSound)
	{
		MakeSound(landNumber);
	}
	if (changes.makeDinosaur)
	{
		MakeDinosaur(changes.makePrint);
	}
	if (landNumber == designed_waterfall::k_SceneryLand)
	{
		ShowScenery();
		return;
	}
	if (landNumber != designed_waterfall::k_Land)
	{
		return;
	}
	if (step.has_value())
	{
		const auto milliseconds = static_cast<int32_t>(step->count());
		_slide = designed_waterfall::Slide(_slide, milliseconds);
		// One ring of spray at most each step, and none when the water has as many rings as it can have
		if (designed_waterfall::StepSpray(_sprayClock, milliseconds) && Locator::waterRingSystem::has_value())
		{
			static_cast<void>(Locator::waterRingSystem::value().Add(designed_waterfall::Spray(FrameLandLight(255))));
		}
	}
	ShowModel();
}

std::optional<uint32_t> DesignedWaterfallSystem::GetSound() const
{
	return _sound != 0 ? std::optional(_sound) : std::nullopt;
}

bool DesignedWaterfallSystem::HasDinosaurPrint() const
{
	return DinosaurShown() && Locator::drawnRegistry::value().AllOf<GroundPrint>(_dinosaur);
}

bool DesignedWaterfallSystem::ModelShown() const
{
	return Shown(_model, _modelMesh);
}

bool DesignedWaterfallSystem::DinosaurShown() const
{
	return Shown(_dinosaur, _dinosaurMesh);
}

void DesignedWaterfallSystem::MakeModel(int32_t landNumber)
{
	_model = entt::null;
	_modelMesh = 0;
	const auto file = designed_waterfall::ModelFileOf(landNumber);
	const auto model = file.has_value() ? LoadModel(*file) : std::nullopt;
	if (!model.has_value() || !Locator::drawnRegistry::has_value())
	{
		return;
	}
	_modelMesh = *model;
	if (landNumber == designed_waterfall::k_SceneryLand)
	{
		_model = MakeScenery(designed_waterfall::k_Ark, *model, false);
		return;
	}
	auto& drawn = Locator::drawnRegistry::value();
	_model = drawn.Create();
	drawn.Assign<Transform>(_model, designed_waterfall::k_Position, x87::GameUprightTurn(designed_waterfall::k_Turn),
	                        glm::vec3(designed_waterfall::k_Scale));
	drawn.Assign<Mesh>(_model, *model, static_cast<int8_t>(0), static_cast<int8_t>(-1));
	drawn.Assign<DrawImportance>(_model, DrawImportance {.importance = designed_waterfall::k_Importance});
	drawn.Assign<SnowLevel>(_model);
	drawn.Assign<SlidingTexture>(_model, SlidingTexture {.offset = glm::vec2(0.0f, _slide)});
	drawn.SetDirty();
}

void DesignedWaterfallSystem::RemoveModel()
{
	if (ModelShown())
	{
		auto& drawn = Locator::drawnRegistry::value();
		drawn.Destroy(_model);
		drawn.SetDirty();
	}
	_model = entt::null;
	_modelMesh = 0;
}

void DesignedWaterfallSystem::MakeDinosaur(bool print)
{
	_dinosaur = entt::null;
	_dinosaurMesh = 0;
	const auto model = LoadModel(designed_waterfall::k_Dinosaur.modelFile);
	if (!model.has_value() || !Locator::drawnRegistry::has_value())
	{
		return;
	}
	_dinosaurMesh = *model;
	_dinosaur = MakeScenery(designed_waterfall::k_Dinosaur, *model, print);
}

void DesignedWaterfallSystem::RemoveDinosaur()
{
	if (DinosaurShown())
	{
		auto& drawn = Locator::drawnRegistry::value();
		drawn.Destroy(_dinosaur);
		drawn.SetDirty();
	}
	_dinosaur = entt::null;
	_dinosaurMesh = 0;
}

void DesignedWaterfallSystem::MakeSound(int32_t landNumber)
{
	if (!Locator::soundTagSystem::has_value())
	{
		return;
	}
	// The sound of the ark comes from its place on the land
	const auto place =
	    landNumber == designed_waterfall::k_SceneryLand ? PlaceOf(designed_waterfall::k_Ark) : designed_waterfall::k_Position;
	_sound = Locator::soundTagSystem::value().KeepPointSound(static_cast<entt::id_type>(audio::SoundId::G_WaterFlow), place);
}

void DesignedWaterfallSystem::RemoveSound()
{
	if (_sound != 0 && Locator::soundTagSystem::has_value())
	{
		Locator::soundTagSystem::value().EndPointSound(_sound);
	}
	_sound = 0;
}

void DesignedWaterfallSystem::ShowModel() const
{
	if (!ModelShown() || !Locator::drawnRegistry::value().AllOf<SlidingTexture>(_model))
	{
		return;
	}
	Locator::drawnRegistry::value().Get<SlidingTexture>(_model).offset = glm::vec2(0.0f, _slide);
	ShowSnow(_model, designed_waterfall::k_Position);
}

void DesignedWaterfallSystem::ShowScenery() const
{
	for (const auto entity : {ModelShown() ? _model : entt::null, DinosaurShown() ? _dinosaur : entt::null})
	{
		if (entity != entt::null)
		{
			ShowSnow(entity, Locator::drawnRegistry::value().Get<Transform>(entity).position);
		}
	}
}
