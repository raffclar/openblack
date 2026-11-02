/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "DesignedWaterfallSystem.h"

#include <string>

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandLightFrame.h"
#include "3D/SnowCover.h"
#include "Common/X87Angle.h"
#include "ECS/Components/DrawImportance.h"
#include "ECS/Components/Mesh.h"
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
std::optional<entt::id_type> LoadModel()
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return std::nullopt;
	}
	const auto id = resources::HashIdentifier("misc/" + std::string(designed_waterfall::k_ModelFile));
	auto& meshes = Locator::resources::value().GetMeshes();
	if (meshes.Contains(id))
	{
		return id;
	}
	auto& fileSystem = Locator::filesystem::value();
	const auto path = fileSystem.GetPath<filesystem::Path::Misc>() / designed_waterfall::k_ModelFile;
	if (!fileSystem.Exists(path))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Model {} not found", path.generic_string());
		return std::nullopt;
	}
	meshes.Load(id, resources::L3DLoader::FromDiskTag {}, path);
	return id;
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
	const auto changes = designed_waterfall::Arrive(_presence, landNumber, ModelShown());
	if (changes.removeModel)
	{
		RemoveModel();
	}
	if (changes.removeSound)
	{
		RemoveSound();
	}
	if (changes.makeModel)
	{
		MakeModel();
	}
	if (changes.makeSound)
	{
		MakeSound();
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

bool DesignedWaterfallSystem::ModelShown() const
{
	if (!Locator::drawnRegistry::has_value() || _model == entt::null)
	{
		return false;
	}
	const auto& drawn = Locator::drawnRegistry::value();
	return drawn.Valid(_model) && drawn.AllOf<SlidingTexture>(_model);
}

void DesignedWaterfallSystem::MakeModel()
{
	_model = entt::null;
	const auto model = LoadModel();
	if (!model.has_value() || !Locator::drawnRegistry::has_value())
	{
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
}

void DesignedWaterfallSystem::MakeSound()
{
	if (Locator::soundTagSystem::has_value())
	{
		_sound = Locator::soundTagSystem::value().KeepPointSound(static_cast<entt::id_type>(audio::SoundId::G_WaterFlow),
		                                                         designed_waterfall::k_Position);
	}
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
	if (!ModelShown())
	{
		return;
	}
	auto& drawn = Locator::drawnRegistry::value();
	drawn.Get<SlidingTexture>(_model).offset = glm::vec2(0.0f, _slide);
	// It shows the snow where it stands at once, with no slow change
	if (Locator::snowSystem::has_value())
	{
		const float depth = Locator::snowSystem::value().GetDepth(
		    glm::vec2(designed_waterfall::k_Position.x, designed_waterfall::k_Position.z));
		drawn.Get<SnowLevel>(_model).level = snow_cover::ObjectLevel(depth, snow_cover::k_ObjectRate, snow_cover::k_ObjectRate);
	}
}
