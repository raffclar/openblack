/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TempleInterior.h"

#include <cmath>
#include <cstdio>

#include <algorithm>
#include <array>
#include <ranges>
#include <unordered_map>

#include <fmt/format.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/CameraPath.h"
#include "3D/CreatureCaveEffects.h"
#include "3D/CreatureCaveTrophies.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/TempleScrolls.h"
#include "3D/TempleSigns.h"
#include "3D/TempleSounds.h"
#include "Audio/Audio.h"
#include "Camera/Camera.h"
#include "Camera/TempleCameraModel.h"
#include "Common/EventManager.h"
#include "Creature/CreatureCave.h"
#include "ECS/Archetypes/GlowArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/Implementations/RenderingSystem.h"
#include "ECS/Systems/Implementations/RenderingSystemTemple.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "Gui/GameInterface.h"
#include "Gui/GameMenu.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "Video/FallingSpellVideo.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;

using Indoors = TempleRoom;
const std::unordered_multimap<Indoors, std::string_view> k_TempleInteriorParts {
    {Indoors::Challenge, "challenge_l3d"},
    // {"challengelo_l3d", Indoors::ChallengeLO},
    {Indoors::Challenge, "challengedome_l3d"},
    {Indoors::Challenge, "challengefloor_l3d"},
    // {"challengefloorlo_l3d", Indoors::ChallengeFloorLO},
    {
        Indoors::CreatureCave,
        "creature_l3d",
    },
    // {"creaturelo_l3d", Indoors::CreatureCaveLO},
    {Indoors::CreatureCave, "creaturewater_l3d"},
    // {"creaturewaterlo_l3d", Indoors::CreatureCaveWaterLO},
    {Indoors::Credits, "credits_l3d"},
    // {"creditslo_l3d", Indoors::CreditsLO},
    {Indoors::Credits, "creditsdome_l3d"},
    {Indoors::Credits, "creditsfloor_l3d"},
    // {"creditsfloorlo_l3d", Indoors::CreditsFloorLO},
    {Indoors::Main, "main_l3d"},
    // {"mainlo_l3d", Indoors::MainLO},
    {Indoors::Main, "mainfloor_l3d"},
    // {"mainfloorlo_l3d", Indoors::MainFloorLO},
    {Indoors::Main, "mainwater_l3d"},
    // {"mainwaterlo_l3d", Indoors::MainWaterLO},
    // {"movement_l3d", Indoors::Movement}, // Navmesh
    {Indoors::Multi, "multi_l3d"},
    // {"multilo_l3d", Indoors::MultiLO},
    {Indoors::Multi, "multidome_l3d"},
    {Indoors::Multi, "multifloor_l3d"},
    // {"multifloorlo_l3d", Indoors::MultiFloorLO},
    {Indoors::Options, "options_l3d"},
    // {"optionslo_l3d", Indoors::OptionsLO},
    {Indoors::Options, "optionsdome_l3d"},
    {Indoors::Options, "optionsfloor_l3d"},
    // {"optionsfloorlo_l3d", Indoors::OptionsFloorLO},
    {Indoors::SaveGame, "savegame_l3d"},
    // {"savegamelo_l3d", Indoors::SaveGameLO},
    {Indoors::SaveGame, "savegamedome_l3d"},
    {Indoors::SaveGame, "savegamefloor_l3d"},
    // {"savegamefloorlo_l3d", Indoors::SaveGameFloorLO},
};

const std::unordered_map<Indoors, std::string_view> k_TempleInteriorGlows {
    {Indoors::Challenge, "challenge"},   //
    {Indoors::CreatureCave, "creature"}, //
    {Indoors::Credits, "credits"},       //
    {Indoors::Main, "main"},             //
    {Indoors::Multi, "multi"},           //
    {Indoors::Options, "options"},       //
    {Indoors::SaveGame, "savegame"},     //
};

inline void addRoomToRegistry(std::string_view assetName, Indoors templeRoom, glm::vec3 position, glm::mat3 rotation,
                              glm::vec3 scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const entt::id_type meshId = entt::hashed_string(fmt::format("temple/interior/{}", assetName).c_str()).value();
	// "<room>_l3d" is the room itself and "<room>floor_l3d" its floor
	const auto roomName = k_TempleInteriorGlows.at(templeRoom);
	auto mesh = ecs::components::TempleInteriorMesh::Other;
	if (assetName == fmt::format("{}_l3d", roomName))
	{
		mesh = ecs::components::TempleInteriorMesh::Room;
	}
	else if (assetName == fmt::format("{}floor_l3d", roomName))
	{
		mesh = ecs::components::TempleInteriorMesh::Floor;
	}
	else if (assetName == fmt::format("{}water_l3d", roomName) && templeRoom == Indoors::CreatureCave)
	{
		mesh = ecs::components::TempleInteriorMesh::Water;
	}
	else if (assetName == fmt::format("{}water_l3d", roomName) && templeRoom == Indoors::Main)
	{
		mesh = ecs::components::TempleInteriorMesh::Pool;
	}
	auto entity = registry.Create();
	registry.Assign<ecs::components::TempleInteriorPart>(entity, templeRoom, mesh);
	registry.Assign<ecs::components::Transform>(entity, position, rotation, scale);
	registry.Assign<ecs::components::Mesh>(entity, meshId, static_cast<int8_t>(0), static_cast<int8_t>(0));
}

inline void addGlowsToRegistry(Indoors templeRoom)
{
	const auto& glowManager = Locator::resources::value().GetGlows();
	const auto glowId =
	    entt::hashed_string(fmt::format("temple/interior/glow/{}", k_TempleInteriorGlows.at(templeRoom)).c_str()).value();
	const auto glows = glowManager.Handle(glowId);
	for (const auto& glow : glows->emitters)
	{
		ecs::archetypes::GlowArchetype::Create(glow, templeRoom);
	}
}

namespace
{
/// How far through its swing the main room's door is before the room behind it is drawn
constexpr float k_MainRoomOpenSwing = 0.01f;

/// The creature's room slides the waterfall's texture through this much of a slide each millisecond, and the slide
/// across ten of the texture
constexpr float k_WaterfallSlidePerMillisecond = 2.1e-5f;
constexpr float k_WaterfallSlideLength = -10.0f;
/// The colour the temple fades to before the player leaves it for the island, and out of as they arrive
constexpr uint32_t k_White = 0x00FFFFFF;

/// Plays one of the temple's sounds, which its doors, buttons, scrolls and camera ask for
void PlayTempleSound(const temple_sounds::Options& options)
{
	audio::PlayOptions play;
	static_cast<temple_sounds::Options&>(play) = options;
	audio::PlaySoundEffect(play);
}

/// The temple's fade: a colour over the whole screen, which it shares with the falling spell's film and which goes to
/// the screen's fade every frame while the player is in the temple
video::TempleFade& Fade()
{
	return video::GetFallingSpell().GetTempleFade();
}

/// Covers the screen by an amount and fades it away, in the colour it has
void FadeFrom(float amount)
{
	auto& fade = Fade();
	fade.current = amount;
	fade.target = 0.0f;
}

/// Covers the screen by an amount of a colour and fades it away
void FadeFrom(float amount, uint32_t rgb)
{
	FadeFrom(amount);
	Fade().rgb = rgb;
	Fade().done = 0;
}

/// Fades a colour over the screen, then back away
void FadeThrough(uint32_t rgb)
{
	auto& fade = Fade();
	fade.current = 0.0f;
	fade.target = 1.0f;
	fade.rgb = rgb;
	fade.done = 0;
}

/// The fire of the creature's room, the second place movement.l3d marks
constexpr size_t k_CreatureCaveFirePlace = 1;
std::optional<glm::vec3> CreatureCaveFire()
{
	constexpr entt::id_type k_Movement = entt::hashed_string("temple/interior/movement_l3d").value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(k_Movement) || meshes.Handle(k_Movement)->GetExtraMetrics().size() <= k_CreatureCaveFirePlace)
	{
		return std::nullopt;
	}
	return glm::vec3(meshes.Handle(k_Movement)->GetExtraMetrics()[k_CreatureCaveFirePlace][3]);
}

/// The belts' and medals' meshes, data/citadel/icons/<icon>.l3d as "temple/icons/<icon>", and the texture they are all
/// drawn with, read once into the caches; one that can't be read is left out
void LoadCaveIcons()
{
	using filesystem::Path;
	auto& fileSystem = Locator::filesystem::value();
	auto& resources = Locator::resources::value();
	auto& meshes = resources.GetMeshes();
	for (uint32_t i = 0; i < CreatureCaveTrophies::k_IconCount; ++i)
	{
		const auto icon = CreatureCaveTrophies::IconName(i);
		const auto id = entt::hashed_string(fmt::format("temple/icons/{}", icon).c_str()).value();
		try
		{
			if (!meshes.Contains(id))
			{
				meshes.Load(id, resources::L3DLoader::FromDiskTag {},
				            fileSystem.GetPath<Path::Citadel>() / "icons" / fmt::format("{}.l3d", icon));
			}
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The creature room's {} can't be read: {}", icon, e.what());
		}
	}
	auto& textures = resources.GetTextures();
	try
	{
		if (!textures.Contains(TempleCaveTrophy::k_Texture))
		{
			using resources::Texture2DLoader;
			textures.Load(TempleCaveTrophy::k_Texture, Texture2DLoader::FromColourAlphaTag {}, "temple/icons",
			              Texture2DLoader::ColourAlphaDesc {
			                  .colour = fileSystem.GetPath<Path::Textures>() / "icons.raw",
			                  .alpha = fileSystem.GetPath<Path::Textures>() / "iconsa.raw",
			                  .wrap = graphics::Wrapping::Repeat,
			              });
		}
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The creature room's icons can't be read: {}", e.what());
	}
}
} // namespace

TempleInterior::TempleInterior()
    : _doors(PlayTempleSound)
    , _toggles(PlayTempleSound)
{
}

namespace
{
/// What the rooms' scrolls tell of the game: made up, but for what openblack keeps
TempleScrolls::Facts GatherScrollFacts()
{
	auto facts = TempleScrolls::Facts::Mock();
	// The game's count of the people in the world
	facts.population = static_cast<int32_t>(Locator::entitiesRegistry::value().Size<ecs::components::Villager>());
	// The save game room's time played counts from when the game's engine started, on the game's own clock
	facts.timePlayed = std::chrono::seconds(std::max(game_clock::EngineMs(), 0) / 1000);
	// The creature's scrolls tell of the player's own creature, and are blank without one
	if (Locator::creatureCaveSystem::has_value())
	{
		const auto snapshot = Locator::creatureCaveSystem::value().Snapshot();
		facts.creature = snapshot.has_value() ? std::optional(creature_cave::FactsOf(*snapshot)) : std::nullopt;
	}
	return facts;
}
} // namespace

void TempleInterior::SetInterface(gui::GameInterface* interface)
{
	_interface = interface;
	_scrolls.reset();
	_signs.reset();
	_text.clear();
	if (_active)
	{
		CreateScrolls();
	}
}

void TempleInterior::CreateScrolls()
{
	if (_interface == nullptr)
	{
		return;
	}
	// The parchment every scroll is written on, read the first time the rooms are set up and kept in the byte cache
	const auto& parchment =
	    resources::LoadOptionalBlob(Locator::resources::value().GetBlobs(),
	                                Locator::filesystem::value().GetPath<filesystem::Path::Textures>() / "ChallengeScroll.raw",
	                                "temple scrolls' parchment");
	_scrolls = std::make_unique<TempleScrolls>(_interface->GetTexts(), _interface->GetFont(), parchment, PlayTempleSound,
	                                           [] { return audio::TickCount(); });
	_scrolls->Create(GatherScrollFacts());
	_signs = std::make_unique<TempleSigns>(_interface->GetTexts(), _interface->GetFont());

	// The main room finds the buttons of what the map shows as it is set up
	auto& meshes = Locator::resources::value().GetMeshes();
	const entt::id_type mainMeshId = entt::hashed_string("temple/interior/main_l3d").value();
	std::vector<std::string> names;
	if (meshes.Contains(mainMeshId))
	{
		for (const auto& subMesh : meshes.Handle(mainMeshId)->GetSubMeshes())
		{
			names.push_back(subMesh->GetName());
		}
	}
	_toggles.Find(names);
}

bool TempleInterior::HoldControl(bool pressed, float mouseY)
{
	const auto hit = GetCursorHit();
	const auto hoveredMainSubMesh =
	    hit.has_value() && hit->room == TempleRoom::Main ? hit->subMesh : std::optional<uint32_t> {};
	bool held = _toggles.Hold(pressed, hoveredMainSubMesh);
	if (_scrolls != nullptr)
	{
		std::optional<TempleScrolls::Focus> focus;
		TempleScrolls::LazyFacts facts(GatherScrollFacts);
		held = _scrolls->Hold(pressed, mouseY, hit, facts, focus) || held;
		if (focus.has_value() && _cameraModel != nullptr)
		{
			// Looking at a scroll wooshes, unless the camera is looking at one already
			if (!_cameraModel->IsLookingAtSubMesh())
			{
				PlayTempleSound(temple_sounds::Woosh(audio::TickCount()));
			}
			_cameraModel->LookAtSubMesh(focus->position, focus->lookAt);
		}
	}
	return held;
}

std::vector<uint32_t> TempleInterior::GetHiddenSubMeshes(TempleRoom room) const
{
	return room == TempleRoom::Main ? _toggles.GetHidden() : std::vector<uint32_t> {};
}

bool TempleInterior::IsControl(TempleRoom room, uint32_t subMesh) const
{
	return (room == TempleRoom::Main && _toggles.IsControl(subMesh)) ||
	       (_scrolls != nullptr && _scrolls->IsControl(room, subMesh));
}

bool TempleInterior::IsControlHeld() const
{
	return _toggles.IsHeld() || (_scrolls != nullptr && _scrolls->IsHeld());
}

std::vector<TempleSubMeshTexture> TempleInterior::GetScrollTextures(TempleRoom room) const
{
	return _scrolls != nullptr ? _scrolls->GetTextures(room) : std::vector<TempleSubMeshTexture> {};
}

std::vector<TempleSubMeshGlow> TempleInterior::GetControlGlows(TempleRoom room) const
{
	// The control under the cursor brightens by up to 12, 12 and 20 of 255 as the glow
	// runs out
	if (!_hovered.has_value() || _hovered->first != room || !IsControl(room, _hovered->second) || _hoverGlow <= 0.0f)
	{
		return {};
	}
	const auto channel = [this](float strength) {
		return static_cast<float>(static_cast<int32_t>(_hoverGlow * strength * 0.002f)) / 255.0f;
	};
	return {{_hovered->second, glm::vec3(channel(12.0f), channel(12.0f), channel(20.0f))}};
}

const graphics::Texture2D* TempleInterior::GetTextTexture() const
{
	return _interface != nullptr ? &_interface->GetFontTexture() : nullptr;
}

void TempleInterior::UpdateOptionsAndFutureRooms(float seconds)
{
	const bool arrived = _cameraModel != nullptr && _cameraModel->IsInControl() && !_transitionRoom.has_value();
	if (_interface != nullptr)
	{
		auto& menu = _interface->GetMenu();
		menu.SetInsideTemple(_active);
		if (_cameraModel != nullptr)
		{
			_cameraModel->SetDialogOpen(menu.IsOpen());
		}
		// The options room, while no dialog is up: once the camera has come into the room it opens the game's
		// options, and once they are closed it goes back to the main room
		if (_currentRoom == TempleRoom::Options && arrived && !menu.IsOpen())
		{
			if (!_optionsShown)
			{
				menu.Open();
				menu.ShowPage(gui::GameMenu::Page::Options);
				_optionsShown = true;
			}
			else
			{
				_optionsShown = false;
				GoToRoom(TempleRoom::Main);
			}
		}
		else if (_currentRoom != TempleRoom::Options && _optionsShown)
		{
			// The options belong to their room: put elsewhere, as the debug window can, the player leaves them behind
			_optionsShown = false;
			menu.Close();
		}
	}

	// The future room: once the camera has come into the room, "The future is still uncertain..." fades in to half over
	// two seconds, from the start each time the camera comes in again
	std::optional<gui::GameInterface::Message> message;
	if (_currentRoom == TempleRoom::Multi && _cameraModel != nullptr)
	{
		if (!_cameraModel->IsInControl())
		{
			_futureTime = 0.0f;
		}
		else
		{
			_futureTime += seconds;
			if (_interface != nullptr)
			{
				message = gui::GameInterface::Message {
				    .text = std::u16string(_interface->GetTexts().Get("HELP_TEXT_DIALOG_ADDITION_129")),
				    .alpha = std::min(128.0f, _futureTime * 64.0f) / 255.0f,
				};
			}
		}
	}
	if (_interface != nullptr)
	{
		_interface->SetMessage(std::move(message));
	}
}

void TempleInterior::UpdateToolTips(const std::optional<TempleCursorHit>& hit)
{
	if (_interface == nullptr || _cameraModel == nullptr)
	{
		return;
	}
	// The rooms choose the tooltip as they are drawn
	const auto scrolls = _scrolls != nullptr ? _scrolls->GetControls(_currentRoom) : std::vector<TempleScrolls::Control> {};
	const auto toggles = _currentRoom == TempleRoom::Main ? _toggles.GetControls() : std::vector<TempleToggles::Control> {};
	const TempleToolTipInput input {
	    .room = _currentRoom,
	    .inControl = _cameraModel->IsInControl() && !_transitionRoom.has_value(),
	    .zoom = _cameraModel->GetSubMeshZoom(),
	    .lookingAtScroll = _cameraModel->IsLookingAtSubMesh(),
	    .controlHeld = IsControlHeld(),
	    .overPool = _cameraModel->IsOverPool(),
	    .pressingPool = _cameraModel->IsPressingPool(),
	    .hoveredDoor = _cameraModel->GetHoveredDoor(),
	    .overWayBack = _cameraModel->IsOverWayBack(),
	    .caveTarget = _cameraModel->GetCaveTarget(),
	    .zoomingToCaveTarget = _cameraModel->IsZoomingToCaveTarget(),
	    .hoveredSubMesh = hit.has_value() && hit->room == _currentRoom ? hit->subMesh : std::nullopt,
	    .scrolls = scrolls,
	    .toggles = toggles,
	};
	UpdateTempleToolTip(_toolTip, input);

	// The tooltip is drawn by the hand, which is where the cursor meets the room
	std::optional<glm::vec2> onScreen;
	if (hit.has_value() && Locator::windowing::has_value())
	{
		const auto size = glm::vec2(Locator::windowing::value().GetSize());
		glm::vec3 screen;
		if (Locator::camera::value().ProjectWorldToScreen(hit->point, glm::vec4(0.0f, 0.0f, size), screen))
		{
			onScreen = glm::vec2(screen);
		}
	}
	_interface->SetHandOnScreen(onScreen);
}

void TempleInterior::LeaveForMapPoint(glm::vec3 point)
{
	// Only for a point near the middle of the map, or on land above the sea
	constexpr float k_NearMiddle = 10.0f;
	constexpr float k_AboveSea = 1.0f;
	if (!Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto onMap = point - _templePosition;
	const auto world = _map.ToWorld(onMap);
	const float altitude = Locator::terrainSystem::value().GetHeightAt(world);
	if (glm::length(glm::vec2(onMap.x, onMap.z)) >= k_NearMiddle && altitude <= k_AboveSea)
	{
		return;
	}
	// Leaving the temple puts the camera 50 above the place and 70 along z from it, looking at it
	// TODO(raffclar): the hand feels the click
	_leaveTo = std::make_pair(glm::vec3(world.x, altitude + 50.0f, world.y + 70.0f), glm::vec3(world.x, altitude, world.y));
	// The main room's camera fades the temple out to white, and the room leaves once it is
	FadeThrough(k_White);
	_leavingForMapPoint = true;
}

void TempleInterior::FadeToWhite()
{
	FadeThrough(k_White);
}

void TempleInterior::FadeIntoRoom()
{
	// Going to another room covers the cut from 1.2, in whatever colour is still fading
	FadeFrom(1.2f);
}

void TempleInterior::UpdateMapMarkers(float seconds)
{
	_mapMarkers.clear();
	if (_mapTriangles.empty())
	{
		return;
	}
	using namespace ecs::components;
	auto& registry = Locator::entitiesRegistry::value();
	if (_toggles.IsShown(TempleToggles::Display::Temples))
	{
		// Every player's temple
		registry.Each<const Temple, const Transform>([this](const Temple& temple, const Transform& transform) {
			_mapMarkers.push_back({.kind = TempleMapMarkerKind::Temple,
			                       .position = _map.MarkerPosition(glm::vec2(transform.position.x, transform.position.z)),
			                       .colour = TempleMap::MarkerColour(temple.owner)});
		});
	}
	if (_toggles.IsShown(TempleToggles::Display::Creatures))
	{
		// Every creature, in its player's colour
		registry.Each<const Creature, const Transform>([this](const Creature& creature, const Transform& transform) {
			_mapMarkers.push_back({.kind = TempleMapMarkerKind::Creature,
			                       .position = _map.MarkerPosition(glm::vec2(transform.position.x, transform.position.z)),
			                       .colour = TempleMap::MarkerColour(creature.owner)});
		});
	}
	// TODO(raffclar): the map also marks the challenges not yet done, the miracles being cast, and the players'
	// influence, as their buttons show them
	// The markers turn a radian a second while the map is drawn
	_mapMarkerTurn += seconds;
}

void TempleInterior::UpdateCaveTrophies()
{
	using namespace CreatureCaveTrophies;
	_caveTrophies.clear();
	const auto facts = GatherScrollFacts();
	constexpr entt::id_type k_CreatureRoom = entt::hashed_string("temple/interior/creature_l3d").value();
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!facts.creature.has_value() || !meshes.Contains(k_CreatureRoom))
	{
		return;
	}
	std::vector<int32_t> percents;
	percents.reserve(facts.creature->miracles.size());
	for (const auto& miracle : facts.creature->miracles)
	{
		percents.push_back(miracle.percent);
	}
	const auto trophies = Choose(facts.creature->fightBalance, LearningOf(percents));
	if (!trophies.empty() && !_caveIconsLoaded)
	{
		_caveIconsLoaded = true;
		LoadCaveIcons();
	}
	const auto& points = meshes.Handle(k_CreatureRoom)->GetExtraMetrics();
	const auto temple = glm::translate(glm::mat4(1.0f), _templePosition) * glm::eulerAngleY(_templeRotation.y);
	for (const auto& trophy : trophies)
	{
		const entt::id_type mesh = entt::hashed_string(fmt::format("temple/icons/{}", IconName(trophy.icon)).c_str()).value();
		if (trophy.point >= points.size() || !meshes.Contains(mesh))
		{
			continue;
		}
		// The creature's room gives each its point's matrix with its axes made unit long, turned a quarter back about
		// the point's own x axis
		auto place = points[trophy.point];
		const auto across = glm::normalize(glm::vec3(place[0]));
		const auto up = glm::normalize(glm::vec3(place[1]));
		const auto forward = glm::normalize(glm::vec3(place[2]));
		place[0] = glm::vec4(across, 0.0f);
		place[1] = glm::vec4(forward, 0.0f);
		place[2] = glm::vec4(-up, 0.0f);
		_caveTrophies.push_back({
		    .mesh = mesh,
		    .model = temple * place,
		    .colour = trophy.medal ? k_MedalColour : k_BeltColour,
		    .environmentMapped = trophy.environmentMapped,
		});
	}
}

void TempleInterior::StopCreatureCaveSounds()
{
	audio::StopSoundEffect(temple_sounds::k_CreatureCaveFireSample, audio::Owner {}, audio::SfxBank::InGame);
	audio::StopSoundEffect(temple_sounds::k_CreatureCaveWaterSample, audio::Owner {}, audio::SfxBank::InGame);
	_creatureCaveSounds = false;
}

void TempleInterior::UpdateCreatureCave(float milliseconds)
{
	if (!IsRoomDrawn(TempleRoom::CreatureCave))
	{
		if (_creatureCaveSounds)
		{
			StopCreatureCaveSounds();
		}
		return;
	}
	// Every frame the room is drawn it plays the sounds of its water and fire, which carry on as they are while they
	// play, and stop once it is no longer drawn
	const auto fire = CreatureCaveFire();
	PlayTempleSound(temple_sounds::CreatureCaveWater());
	if (fire.has_value())
	{
		PlayTempleSound(temple_sounds::CreatureCaveFire(*fire));
	}
	_creatureCaveSounds = true;
	// Its effects start around its fire the first time it is drawn, and move on each frame it is
	if (_creatureCaveEffects == nullptr && fire.has_value() && CreatureCaveEffects::CanBeMade())
	{
		_creatureCaveEffects = std::make_unique<CreatureCaveEffects>(*fire);
	}
	if (_creatureCaveEffects != nullptr)
	{
		_creatureCaveEffects->Update(static_cast<uint32_t>(milliseconds), game_clock::TickCount());
	}
}

TempleInterior::~TempleInterior() = default;

namespace
{
/// Each room's own mesh, "temple/interior/<room>_l3d", by room
constexpr std::array<entt::id_type, static_cast<size_t>(TempleRoom::Unknown)> k_RoomMeshIds {
    entt::hashed_string("temple/interior/challenge_l3d").value(), //
    entt::hashed_string("temple/interior/creature_l3d").value(),  //
    entt::hashed_string("temple/interior/credits_l3d").value(),   //
    entt::hashed_string("temple/interior/main_l3d").value(),      //
    entt::hashed_string("temple/interior/multi_l3d").value(),     //
    entt::hashed_string("temple/interior/options_l3d").value(),   //
    entt::hashed_string("temple/interior/savegame_l3d").value(),  //
};

/// Each room's path in, data/citadel/engine/<room>.cam, which the game loads as "temple/<room>"
TempleCameraModel::Paths LoadCameraPaths()
{
	TempleCameraModel::Paths paths;
	auto& cameraPaths = Locator::resources::value().GetCameraPaths();
	for (const auto& [room, name] : k_TempleInteriorGlows)
	{
		const auto pathName = fmt::format("temple/{}", name);
		const entt::id_type id = entt::hashed_string(pathName.c_str()).value();
		if (!cameraPaths.Contains(id))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Temple camera path {} isn't loaded", pathName);
			continue;
		}
		paths.at(static_cast<size_t>(room)) = cameraPaths.Handle(id);
	}
	return paths;
}
} // namespace

void TempleInterior::ApplyLens() const
{
	const auto& config = Locator::config::value();
	auto& camera = Locator::camera::value();
	const auto aspect = Locator::windowing::has_value() ? Locator::windowing::value().GetAspectRatio() : 1.0f;
	const auto lens = camera.GetModel().GetLens();
	camera.SetProjectionMatrixPerspective(lens.has_value() ? lens->horizontalFieldOfView : config.cameraXFov, aspect,
	                                      lens.has_value() ? lens->nearClip : config.cameraNearClip, config.cameraFarClip);
}

bool TempleInterior::IsRoomDrawn(TempleRoom room) const
{
	if (room == _currentRoom || room == _transitionRoom)
	{
		return true;
	}
	// From another room, the main room is drawn whole only while its door is open, and otherwise just
	// its doors (the submeshes with joints)
	return room == TempleRoom::Main && !_transitionRoom.has_value() && _doors.GetSwing() > k_MainRoomOpenSwing;
}

void TempleInterior::GoToRoom(TempleRoom room)
{
	if (_active && _cameraModel != nullptr)
	{
		if (room != _currentRoom)
		{
			FadeIntoRoom();
		}
		_cameraModel->GoToRoom(room);
	}
}

void TempleInterior::EnterRoom(TempleRoom room)
{
	if (_active && _cameraModel != nullptr && !_cameraModel->GoThroughDoorTo(room))
	{
		GoToRoom(room);
	}
}

std::optional<TempleCursorHit> TempleInterior::GetCursorHit() const
{
	if (!_active || _cameraModel == nullptr)
	{
		return std::nullopt;
	}
	// The cursor is over the submesh the game picks while drawing the room, and the hand goes where the pick meets
	// it, or onto the floor where the cursor meets that this frame
	const auto& hit = _cameraModel->GetCursorHit();
	const bool onFloor = hit.has_value() && hit->floor;
	const auto& ray = _cameraModel->GetCursorRay();
	if (ray.has_value())
	{
		// The game picks along the ray through every room's mesh it draws, and from another room just the main room's
		// doors
		const auto toRoom =
		    glm::inverse(glm::translate(glm::mat4(1.0f), _templePosition) * glm::eulerAngleY(_templeRotation.y));
		const auto origin = glm::vec3(toRoom * glm::vec4(ray->origin, 1.0f));
		const auto direction = glm::vec3(toRoom * glm::vec4(ray->focus - ray->origin, 0.0f));
		auto& meshes = Locator::resources::value().GetMeshes();
		std::optional<graphics::L3DMesh::PickHit> nearest;
		TempleRoom nearestRoom = TempleRoom::Unknown;
		// The rooms in the same order as ever, which settles a tie between two rooms' picks
		for (const auto& room : k_TempleInteriorGlows | std::views::keys)
		{
			const bool drawn = IsRoomDrawn(room);
			if (!drawn && room != TempleRoom::Main)
			{
				continue;
			}
			const entt::id_type meshId = k_RoomMeshIds.at(static_cast<size_t>(room));
			if (!meshes.Contains(meshId))
			{
				continue;
			}
			// The side rooms' doors aren't drawn shut, so nor are they picked
			const auto hidden = GetHiddenSubMeshes(room);
			if (const auto pick = meshes.Handle(meshId)->Pick(origin, direction, !drawn, room != TempleRoom::Main, hidden);
			    pick.has_value() && (!nearest.has_value() || pick->distance < nearest->distance))
			{
				nearest = pick;
				nearestRoom = room;
			}
		}
		if (nearest.has_value())
		{
			return TempleCursorHit {
			    .point = onFloor ? hit->point : ray->origin + (ray->focus - ray->origin) * nearest->distance,
			    .normal = onFloor ? hit->normal : -glm::normalize(ray->focus - ray->origin),
			    .room = nearestRoom,
			    .subMesh = nearest->subMesh,
			};
		}
	}
	if (!hit.has_value())
	{
		return std::nullopt;
	}
	return TempleCursorHit {.point = hit->point, .normal = hit->normal};
}

void TempleInterior::Escape()
{
	if (_currentRoom != Indoors::Main)
	{
		GoToRoom(Indoors::Main);
	}
	else
	{
		RequestLeave();
	}
}

glm::vec2 TempleInterior::GetWaterfallSlide() const
{
	return {0.0f, _waterfallSlide * k_WaterfallSlideLength};
}

void TempleInterior::ProcessTurn()
{
	// The temple's turn submits the room's tooltip, or none under a dialog, and the help system's turn that follows
	// keeps it or ends it
	if (_active && _interface != nullptr && !_interface->GetMenu().IsOpen() && _toolTip.index.has_value())
	{
		_interface->GetToolTips().Submit(*_toolTip.index, _toolTip.action, _toolTip.arrows);
	}
}

void TempleInterior::Update(std::chrono::microseconds dt)
{
	if (_active)
	{
		const float milliseconds = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(dt).count();
		_waterfallSlide += milliseconds * k_WaterfallSlidePerMillisecond;
		_waterfallSlide -= std::floor(_waterfallSlide);

		UpdateOptionsAndFutureRooms(milliseconds / 1000.0f);

		// The glow of the control under the cursor runs out each frame, and drawing the room sets it off
		// again as the cursor moves onto another submesh, while no control is held
		_hoverGlow = std::max(0.0f, _hoverGlow - std::floor(milliseconds));
		// Picked once a frame: nothing the pick looks at changes before the tooltip is chosen
		const auto hit = GetCursorHit();
		if (!IsControlHeld())
		{
			std::optional<std::pair<TempleRoom, uint32_t>> hovered;
			if (hit.has_value() && hit->subMesh.has_value())
			{
				hovered = std::make_pair(hit->room, *hit->subMesh);
			}
			if (hovered != _hovered)
			{
				_hovered = hovered;
				_hoverGlow = 500.0f;
			}
		}

		// As the rooms are drawn, the scroll the camera is close to has its text drawn in front of it, and the signs
		// their labels
		_text.clear();
		if (_scrolls != nullptr && _cameraModel != nullptr)
		{
			// The facts are gathered only if a scroll is written this frame
			TempleScrolls::LazyFacts facts(GatherScrollFacts);
			_scrolls->SetFocus(_cameraModel->GetSubMeshZoom(), facts);
			_scrolls->AppendFocusedText(_text, facts);
		}
		if (_signs != nullptr)
		{
			// The highlighted sign pulses with the game's clock
			const auto ticks = game_clock::TickCount();
			for (const auto room : {TempleRoom::Main, TempleRoom::CreatureCave, TempleRoom::Credits})
			{
				if (IsRoomDrawn(room))
				{
					// The main room lights the label of the door the cursor is over
					const auto highlighted = room == TempleRoom::Main && _cameraModel != nullptr
					                             ? TempleSigns::MainRoomSignOfDoor(_cameraModel->GetHoveredDoor())
					                             : std::nullopt;
					_signs->Append(_text, room, highlighted, ticks);
				}
			}
		}

		UpdateToolTips(hit);

		// The main room takes the land's heights and brightness for the map every frame the main room is drawn
		_mapTriangles.clear();
		if (IsRoomDrawn(TempleRoom::Main) && Locator::terrainSystem::has_value())
		{
			const auto& island = Locator::terrainSystem::value();
			_map.Build(TempleMap::CellsOf(island), _mapTriangles);
		}
		UpdateMapMarkers(milliseconds / 1000.0f);
		// The temple is lit by the alignment, pulsing with the game's clock
		const float alignment =
		    Locator::alignmentSystem::has_value() ? Locator::alignmentSystem::value().GetCameraAlignment() : 0.0f;
		_light = TempleLight::At(alignment, game_clock::TickCount());
		// The creature's room chooses the belts and medals every frame it is drawn
		if (IsRoomDrawn(TempleRoom::CreatureCave))
		{
			UpdateCaveTrophies();
		}
		else
		{
			_caveTrophies.clear();
		}
		// The main room moves the pool's shimmer on while the main room is drawn
		if (IsRoomDrawn(TempleRoom::Main))
		{
			_poolTime += milliseconds / 1000.0f;
		}
		if (_cameraModel != nullptr)
		{
			if (const auto point = _cameraModel->TakeMapDoubleClick(); point.has_value())
			{
				LeaveForMapPoint(*point);
			}
		}
		// The creature's room's sounds and effects, while it is drawn
		UpdateCreatureCave(milliseconds);
	}

	if (_leavingForMapPoint && Fade().done > 0)
	{
		RequestLeave();
	}
	if (_leaveRequested)
	{
		_leaveRequested = false;
		Deactivate();
	}
}

void TempleInterior::Activate(TempleRoom room)
{
	if (_active)
	{
		return;
	}

	auto& config = Locator::config::value();
	auto& camera = Locator::camera::value();

	_playerPositionOutside = camera.GetOrigin();
	_playerRotationOutside = camera.GetRotation();

	// The main room frames the map on the island as it is each visit, and draws its texture afresh
	++_visits;
	if (Locator::terrainSystem::has_value())
	{
		_map.Frame(TempleMap::CellsOf(Locator::terrainSystem::value()));
	}

	config.drawIsland = false;
	config.drawWater = false;

	// Create temple entities
	// (inferred) no source; _templeRotation is never written (0), so the sign does not show
	auto rotation = glm::eulerAngleY(_templeRotation.y);
	auto scale = glm::vec3(1.0f);

	for (const auto& [roomType, assetName] : k_TempleInteriorParts)
	{
		addRoomToRegistry(assetName, roomType, _templePosition, rotation, scale);
	}
	for (const auto& [roomType, assetName] : k_TempleInteriorGlows)
	{
		addGlowsToRegistry(roomType);
	}

	Locator::rendereringSystem::emplace<ecs::systems::RenderingSystemTemple>();

	// The temple's camera takes over from the island's, coming into the room along its path
	_active = true;
	_leaveRequested = false;
	_leavingForMapPoint = false;
	// Entering the temple covers the way in from 1.2
	FadeIntoRoom();
	_transitionRoom.reset();
	_doors = TempleDoors(PlayTempleSound);
	// The creature's room's effects start around its fire once the room is first drawn (UpdateCreatureCave)
	_currentRoom = room;
	auto model = std::make_unique<TempleCameraModel>(LoadCameraPaths(), _currentRoom);
	_cameraModel = model.get();
	_outsideCameraModel = camera.SetModel(std::move(model));
	_cameraModel->StartIntro(_currentRoom, false);
	camera.SetOrigin(_cameraModel->GetTargetOrigin());
	camera.SetFocus(_cameraModel->GetTargetFocus());
	ApplyLens();
	CreateScrolls();
	// going inside the citadel pauses the world, whose pause until then is kept to give back on leaving
	_pausedOutside = game_clock::IsPaused();
	game_clock::Pause(true);
	// and sets the citadel sequence mode
	game_clock::SetSequenceMode(game_clock::k_SequenceModeCitadel);
}

void TempleInterior::Deactivate()
{
	if (!_active)
	{
		return;
	}
	_scrolls.reset();
	_signs.reset();
	_text.clear();
	if (_interface != nullptr)
	{
		// Nor do the options room's options go out of the temple with the player
		if (_optionsShown)
		{
			_interface->GetMenu().Close();
		}
		_interface->SetMessage(std::nullopt);
		_interface->GetMenu().SetInsideTemple(false);
		// The temple's tooltip does not follow the player out
		_interface->SetHandOnScreen(std::nullopt);
		_interface->GetToolTips().Clear();
	}
	_optionsShown = false;
	// The creature's room's fire and water stop, and its effects go before the rest of the rooms
	if (_creatureCaveSounds)
	{
		StopCreatureCaveSounds();
	}
	_creatureCaveEffects.reset();

	auto& registry = Locator::entitiesRegistry::value();
	auto& config = Locator::config::value();
	config.drawIsland = true;
	config.drawWater = true;
	registry.Each<const ecs::components::TempleInteriorPart>(
	    [&registry](const entt::entity entity, auto&&...) { registry.Destroy(entity); });

	// leaving the citadel: the temple's fire and water samples stop
	audio::LeaveCitadel();

	auto& camera = Locator::camera::value();
	Locator::rendereringSystem::emplace<ecs::systems::RenderingSystem>();
	if (_outsideCameraModel != nullptr)
	{
		camera.SetModel(std::move(_outsideCameraModel));
	}
	_cameraModel = nullptr;
	_transitionRoom.reset();
	_leavingForMapPoint = false;
	_caveTrophies.clear();
	// Leaving the temple goes out to the island all white, which fades over a second
	FadeFrom(1.0f, k_White);
	ApplyLens();
	camera.SetOrigin(_playerPositionOutside);
	camera.SetFocus(_playerPositionOutside + glm::quat(_playerRotationOutside) * glm::vec3(0.0f, 0.0f, 1.0f));
	if (_leaveTo.has_value())
	{
		camera.SetOrigin(_leaveTo->first);
		camera.SetFocus(_leaveTo->second);
		_leaveTo.reset();
	}
	if (Locator::cameraPathSystem::has_value() && Locator::cameraPathSystem::value().IsPathing())
	{
		Locator::cameraPathSystem::value().Stop();
	}
	_active = false;
	// leaving the citadel: no sequence mode, and the world paused as it was before the player went in. The paused time
	// does not count, so the world goes on from the turn it stopped at
	game_clock::SetSequenceMode(game_clock::k_SequenceModeNone);
	game_clock::Pause(_pausedOutside);
}