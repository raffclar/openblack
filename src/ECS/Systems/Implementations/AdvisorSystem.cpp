/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "AdvisorSystem.h"

#include <cstdint>

#include <algorithm>
#include <stdexcept>
#include <string>

#include <glm/matrix.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Help/AdvisorVoices.h"
#include "Help/SpiritPose.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::help::spirits;

namespace
{
/// The advisors' files, the good one's first
constexpr std::array<std::string_view, k_Dudes> k_Files {"MarkGood.Hd", "MarkEvil.Hd"};

/// The camera as the advisors see it this frame
SpiritView ViewOf(const Camera& camera, Screen screen)
{
	SpiritView view;
	const glm::mat4 viewMatrix = camera.GetViewMatrix(Camera::Interpolation::Current);
	const glm::mat4 toWorld = glm::inverse(viewMatrix);
	view.eye = glm::vec3(toWorld[3]);
	view.right = glm::vec3(toWorld[0]);
	view.up = glm::vec3(toWorld[1]);
	const glm::mat4& projection = camera.GetProjectionMatrix();
	// The view looks down whichever of its z axis's ends the projection puts in front
	view.forward = glm::vec3(toWorld[2]) * projection[2][3];
	view.halfExtent = {1.0f / projection[0][0], 1.0f / projection[1][1]};
	view.nearClip = camera.GetNearClip();
	view.viewProjection = projection * viewMatrix;
	view.screen = screen;
	return view;
}

/// The colour channels of a sprite's ARGB
uint32_t WithAlpha(uint32_t alpha, uint32_t rgb)
{
	return (alpha << 24u) + rgb;
}
} // namespace

AdvisorSystem::AdvisorSystem()
{
	MakeController();
}

AdvisorSystem::~AdvisorSystem() = default;

void AdvisorSystem::Load()
{
	auto& fileSystem = Locator::filesystem::value();
	auto& models = Locator::resources::value().GetAdvisorModels();
	const auto directory = fileSystem.GetPath<filesystem::Path::Data>() / "HelpSprite";
	bool loaded = true;
	for (int dude = 0; dude < k_Dudes; ++dude)
	{
		const auto id = ModelId(dude);
		const auto path = directory / k_Files.at(static_cast<size_t>(dude));
		if (!models.Contains(id))
		{
			try
			{
				if (!fileSystem.Exists(path))
				{
					throw std::runtime_error("missing");
				}
				models.Load(id, resources::AdvisorModelLoader::FromBufferTag {}, path.string(), fileSystem.ReadAll(path));
			}
			catch (const std::exception& error)
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The advisor {} can't be loaded: {}", path.string(), error.what());
				loaded = false;
				continue;
			}
		}
		_models.at(static_cast<size_t>(dude)) = models.Handle(id).handle();
	}
	_loaded = loaded;
	MakeController();
}

void AdvisorSystem::Reset()
{
	MakeController();
	_haloClockMs = 0;
	_draws.clear();
	_trails.clear();
}

const AdvisorModel* AdvisorSystem::GetModel(int advisor) const
{
	return _models.at(static_cast<size_t>(advisor)).get();
}

void AdvisorSystem::MakeController()
{
	const auto dataOf = [this](int dude) -> const DudeData& {
		const auto& model = _models.at(static_cast<size_t>(dude));
		return model ? model->data : _emptyData.at(static_cast<size_t>(dude));
	};
	_controller =
	    std::make_unique<AdvisorSpiritController>(dataOf(k_GoodDude), dataOf(k_EvilDude), MakeQueries(), _view.screen);
}

std::vector<glm::mat4> AdvisorSystem::PoseSoFar(int advisor) const
{
	const auto& model = _models.at(static_cast<size_t>(advisor));
	if (!model)
	{
		return {};
	}
	const AdvisorSpirit& dude = _controller->Dude(advisor);
	return EvaluatePose(model->rig, model->data, dude.Layers(), ModelMatrix(dude.Rows(), dude.Position()));
}

Queries AdvisorSystem::MakeQueries()
{
	Queries queries;
	queries.localRand = [](int32_t n) { return Locator::gameRandom::value().LocalRand(n); };
	queries.localFloatRand = [](float x) { return Locator::gameRandom::value().LocalFloatRand(x); };
	queries.random = [](float a, float b) { return Locator::gameRandom::value().CrtRandom(a, b); };

	// The advisors talk with the voices the scripts' dialogue gives them, their mouths moved by the sound
	queries.isTalking = [](int dude) {
		return Locator::helpTextSystem::has_value() && Locator::helpTextSystem::value().GetVoices().IsTalking(dude);
	};
	queries.talkedRecently = [](int dude) {
		return Locator::helpTextSystem::has_value() && Locator::helpTextSystem::value().GetVoices().TalkingOrJustStopped(dude);
	};
	queries.sayActive = [](int dude) {
		return Locator::helpTextSystem::has_value() && Locator::helpTextSystem::value().GetVoices().IsActive(dude);
	};
	queries.updateSentence = [](int dude) {
		if (Locator::helpTextSystem::has_value())
		{
			Locator::helpTextSystem::value().GetVoices().UpdateSaySentence(dude);
		}
	};
	queries.lipSync = [](int dude, float dt) -> std::optional<LipSyncFrame> {
		if (!Locator::helpTextSystem::has_value())
		{
			return std::nullopt;
		}
		return Locator::helpTextSystem::value().GetVoices().ApplyLipSync(dude, dt);
	};

	queries.pointFromScreen = [this](glm::vec2 pixel, float depth) { return PointFromScreen(_view, pixel, depth); };
	queries.worldToPixel = [this](const glm::vec3& point, bool force) { return WorldToPixel(_view, point, force); };
	queries.projectPoint = [this](const glm::vec3& point) { return ProjectPoint(_view, point); };
	queries.nearClip = [this]() { return _view.nearClip; };
	queries.cameraAxes = [this]() { return glm::mat3(_view.right, _view.up, _view.forward); };
	queries.cameraPosition = [this]() { return _view.eye; };
	queries.altitude = [](float x, float z) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
	};
	queries.headAngles = [this](int dude, const glm::mat3&, const glm::vec3&, const glm::vec3& target) {
		const auto& model = _models.at(static_cast<size_t>(dude));
		if (!model)
		{
			return std::optional<glm::vec2>(glm::vec2(0.0f));
		}
		return std::optional<glm::vec2>(HeadAngles(PoseSoFar(dude), model->data.faceBones, target));
	};
	queries.fingertip = [this](int dude, const glm::mat3&, const glm::vec3& position) {
		const auto& model = _models.at(static_cast<size_t>(dude));
		if (!model)
		{
			return position;
		}
		return Fingertip(PoseSoFar(dude), model->data, position);
	};
	queries.object = [](uint32_t object) -> std::optional<ObjectInfo> {
		if (object == 0 || !Locator::entitiesRegistry::has_value())
		{
			return std::nullopt;
		}
		const auto entity = static_cast<entt::entity>(object);
		auto& registry = Locator::entitiesRegistry::value();
		if (!registry.Valid(entity))
		{
			return std::nullopt;
		}
		const auto* transform = registry.TryGet<components::Transform>(entity);
		if (transform == nullptr)
		{
			return std::nullopt;
		}
		ObjectInfo info {.position = transform->position, .height = 0.0f};
		// An object stands as tall as its mesh's box, scaled
		if (const auto* mesh = registry.TryGet<components::Mesh>(entity); mesh != nullptr)
		{
			const auto& meshes = Locator::resources::value().GetMeshes();
			if (meshes.Contains(mesh->id))
			{
				info.height = meshes.Handle(mesh->id)->GetBoundingBox().Size().y * transform->scale.y;
			}
		}
		return info;
	};
	return queries;
}

void AdvisorSystem::Update(const Frame& frame)
{
	if (frame.camera == nullptr)
	{
		return;
	}
	const Screen screen {.width = frame.screen.x, .height = frame.screen.y};
	_view = ViewOf(*frame.camera, screen);
	// The advisors' homes are worked out on the screen they start on: set up before the screen was known, or with both
	// at home when its size changes, they are set up again on it
	const auto& known = _controller->GetScreen();
	if ((known.width != screen.width || known.height != screen.height) &&
	    _controller->State(k_GoodDude) == ControlState::Home && _controller->State(k_EvilDude) == ControlState::Home)
	{
		MakeController();
	}

	FrameInput input;
	input.dt = static_cast<float>(frame.frameMs) * 0.001f;
	input.trailDt = static_cast<float>(static_cast<int32_t>(frame.frameMs)) * 0.01f;
	input.frameMs = frame.stepMs;
	input.screen = screen;
	input.mouse = frame.mouse;
	input.tickMs = frame.tickMs;
	input.wideScreen = frame.wideScreen;
	_controller->Update(input);

	MakeDraws(frame.stepMs);
	MakeTrails();
}

void AdvisorSystem::ProcessTurn()
{
	_controller->ProcessTurn();
}

void AdvisorSystem::MakeDraws(int32_t stepMs)
{
	_draws.clear();
	for (int d = 0; d < k_Dudes; ++d)
	{
		if (_controller->State(d) == ControlState::Home)
		{
			continue;
		}
		const AdvisorSpirit& dude = _controller->Dude(d);
		const auto& model = _models.at(static_cast<size_t>(d));
		Draw draw;
		draw.advisor = d;
		draw.inWorld = dude.InWorld();
		draw.nearScreen = draw.inWorld < 0.5f;
		draw.alpha = static_cast<uint8_t>(std::clamp(dude.AlphaByte(), 0, 255));
		draw.bones = PoseSoFar(d);
		// The land lights it by how far it is out in the world
		draw.worldShade = static_cast<uint8_t>(static_cast<int32_t>(Smooth(draw.inWorld) * 255.0f));
		draw.landLightPoint = dude.LightPosition();
		if (draw.nearScreen)
		{
			// Near the screen, the light starts from beside the camera, at the screen's left edge halfway down, ten
			// units behind it
			draw.nearLight = PointFromScreen(_view, glm::vec2(0.0f, static_cast<float>(_view.screen.HalfHeight())), -10.0f);
		}
		if (const auto pupils = dude.Pupils(); pupils && model)
		{
			draw.pupils = std::array {Pupil {.bone = model->data.faceBones[0], .scale = (*pupils)[0]},
			                          Pupil {.bone = model->data.faceBones[3], .scale = (*pupils)[1]}};
			draw.pupilCentre = model->data.pupilCentre;
			draw.pupilDownAlongZ = d == k_EvilDude;
		}

		// The halo and the puff stand about the root bone, in the world's axes
		const glm::vec3 anchor = draw.bones.empty() ? dude.Position() : glm::vec3(draw.bones.front()[3]);
		const auto inFront = [this](const glm::vec3& position) {
			return glm::dot(position - _view.eye, _view.forward) > _view.nearClip;
		};
		if (model && model->file.haloScale != 0.0f && draw.alpha != 0)
		{
			// The halo's clock moves on only as a halo is drawn
			_haloClockMs += stepMs;
			const glm::vec3 offset(model->file.haloOffset[0], model->file.haloOffset[1], model->file.haloOffset[2]);
			const float size = model->data.scale * model->file.haloScale * dude.ModelScale() * 100.0f;
			const Sprite halo {.position = anchor + offset,
			                   .halfWidth = size,
			                   .halfHeight = size * 0.3f,
			                   .angle = 0.0f,
			                   .cell = static_cast<uint8_t>((_haloClockMs / 200) & 15),
			                   .argb = WithAlpha(draw.alpha, 0xFFFFFF)};
			if (inFront(halo.position))
			{
				draw.sprites.push_back(halo);
			}
		}
		if (const auto& particles = dude.PuffParticles(); particles && dude.PuffRunning())
		{
			// The root bone in the hover's space, as far off the screen as it may be
			const glm::vec2 hover = dude.WorldToHover(anchor, true).value_or(dude.Hover());
			const float fade = dude.PuffFade();
			// The evil advisor's smoke is a dark red
			const uint32_t mask = d == k_EvilDude ? 0x7F1F1Fu : 0xFFFFFFu;
			for (size_t i = 0; i < particles->size(); ++i)
			{
				const PuffParticle& p = particles->at(i);
				if (p.drawAlpha <= 0)
				{
					continue;
				}
				float size = ((p.sizeBase - p.k * p.age * fade * 0.5f) + 1.0f) * 0.12f;
				if (size < 0.0001f)
				{
					size = 0.0001f;
				}
				const Sprite smoke {.position = dude.HoverTo3D(hover.x + p.velocity.x * 0.1f, hover.y + p.drawVelocityY * 0.1f,
				                                               -dude.DepthChannel().GetValue() - 0.6f, false),
				                    .halfWidth = size,
				                    .halfHeight = size,
				                    .angle = p.spin * p.age + static_cast<float>(i),
				                    .cell = static_cast<uint8_t>(static_cast<int32_t>(p.age * 8.0f) & 15),
				                    .argb = WithAlpha(static_cast<uint32_t>((p.drawAlpha * 2) / 4), p.grey & mask)};
				if (inFront(smoke.position))
				{
					draw.sprites.push_back(smoke);
				}
			}
		}
		_draws.push_back(std::move(draw));
	}
}

void AdvisorSystem::MakeTrails()
{
	_trails.clear();
	const auto indices = TrailIndices();
	for (int d = 0; d < k_Dudes; ++d)
	{
		const AdvisorSpirit& dude = _controller->Dude(d);
		// Only an advisor out and wholly near the screen trails its rainbow
		if (_controller->State(d) == ControlState::Home || dude.InWorld() != 0.0f)
		{
			continue;
		}
		const auto strip = dude.TrailStrip();
		std::array<TrailVertex, strip.size()> vertices {};
		for (size_t i = 0; i < strip.size(); ++i)
		{
			const auto& v = strip.at(i);
			vertices.at(i) = {.position = dude.HoverTo3D(v.hover.x, v.hover.y, v.hover.z, false), .uv = v.uv, .argb = v.argb};
		}
		for (const auto index : indices)
		{
			_trails.push_back(vertices.at(index));
		}
	}
}
