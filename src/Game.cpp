/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Game.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <array>
#include <sstream>
#include <string>
#include <string_view>

#include <LHVM.h>
#include <SDL.h>
#include <bgfx/bgfx.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/transform.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/DayNightClock.h"
#include "3D/L3DMesh.h"
#include "3D/LandAvoid.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/NightLights.h"
#include "3D/OceanInterface.h"
#include "3D/ScreenFade.h"
#include "3D/SkyInterface.h"
#include "3D/SkyType.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/Audio.h"
#include "Audio/Engine/MusicStream.h"
#include "Audio/Engine/SamplePlay.h"
#include "Audio/Services/AtmosBanks.h"
#include "Audio/Services/Confirmation.h"
#include "Audio/Services/GameMusic.h"
#include "Audio/Services/Guidance.h"
#include "Audio/Services/LanternSounds.h"
#include "Audio/Services/ScriptAudioState.h"
#include "Audio/Services/SoundMap.h"
#include "Audio/Services/SoundTags.h"
#include "Audio/Services/SpookyVoices.h"
#include "Audio/Services/Voices.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/CameraHelp.h"
#include "Camera/ScriptCamera.h"
#include "Common/EventManager.h"
#include "Common/GameRandom.h"
#include "Common/HelpText.h"
#include "Common/StringUtils.h"
#include "Debug/DebugGuiInterface.h"
#include "Debug/FixedClock.h"
#include "Debug/StateHash.h"
#include "ECS/AnimalAI.h"
#include "ECS/AnimalAnimations.h"
#include "ECS/Animations.h"
#include "ECS/Archetypes/OneOffSpellSeedArchetype.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/AudioQueries.h"
#include "ECS/CarriedProps.h"
#include "ECS/Components/CameraBookmark.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureLoop.h"
#include "ECS/DesignedScenery.h"
#include "ECS/DisappearSmoke.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fields.h"
#include "ECS/FireFlies.h"
#include "ECS/FishShoals.h"
#include "ECS/Footpaths.h"
#include "ECS/GroundMarks.h"
#include "ECS/Influence/Influence.h"
#include "ECS/IntroSpecial.h"
#include "ECS/LivingTurn.h"
#include "ECS/Map.h"
#include "ECS/MissionaryBoat.h"
#include "ECS/MobileDrawing.h"
#include "ECS/MobileWalkPaths.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectGhosts.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/Buildings.h"
#include "ECS/Physics/Dust.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/PlayerCreature.h"
#include "ECS/PuzzleGames.h"
#include "ECS/Registry.h"
#include "ECS/Rivers.h"
#include "ECS/RoutePlanWorld.h"
#include "ECS/ScriptHeld.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/ScriptTimer.h"
#include "ECS/Sharks.h"
#include "ECS/ShowNeeds.h"
#include "ECS/SuperVillager.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/DrawUpdate.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/PathfindingSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/ScreenFadeSystemInterface.h"
#include "ECS/Systems/ScreenshotRequestSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownFeatures.h"
#include "ECS/Town/TownProcess.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/Wonders.h"
#include "ECS/Trees.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/WaterRings.h"
#include "ECS/Weather/WeatherLoop.h"
#include "Engine/FrameSnapshot.h"
#include "Engine/GpuCommands.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/OverlayFrame.h"
#include "Graphics/RendererInterface.h"
#include "Graphics/SuperVillagerFrame.h"
#include "Gui/GameInterface.h"
#include "Gui/GameMenu.h"
#include "Gui/TextDatabase.h"
#include "Help/HelpProfile.h"
#include "Help/HelpSystem.h"
#include "Help/InputPromptIcon.h"
#include "Help/InterfaceInteraction.h"
#include "Help/ScriptControl.h"
#include "Help/SpiritsRuntime.h"
#include "Help/ToolTips.h"
#include "Input/FixedMouse.h"
#include "Input/GameActionMapInterface.h"
#include "Input/GameCursor.h"
#include "Input/GamePackets.h"
#include "Input/HandDemo.h"
#include "Input/InterfaceActive.h"
#include "Input/RealInput.h"
#include "LHScriptX/Script.h"
#include "LandBalance.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/MagicLoop.h"
#include "ModLoader/ModLoader.h"
#include "ModLoaderStatus.h"
#include "Parsers/InfoFile.h"
#include "Particles/PSysManager.h"
#include "Particles/TownBelief.h"
#include "Profiler.h"
#include "Resources/BlobStream.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "Serializer/FotFile.h"
#include "Video/FallingSpellVideo.h"
#include "Video/VideoPlayer.h"
#include "Worship/Citadel.h"

#ifdef __ANDROID__
#include <spdlog/sinks/android_sink.h>
#endif

using namespace openblack;
using namespace openblack::lhscriptx;
using namespace std::chrono_literals;

constexpr std::string_view k_WindowTitle = "openblack";

namespace
{
/// The day / night clock (Locator::dayNightClock)
DayNightClock& TheDayNightClock()
{
	return Locator::dayNightClock::value().Clock();
}

/// The script fade and the cinema bars (Locator::screenFade)
ScreenFade& TheScreenFade()
{
	return Locator::screenFade::value().Fade();
}

/// What the audio's music reads from the game (Audio/GameQueries.h); the queries left unset are the systems openblack
/// does not have yet (videos, the wide screen bars moving, the towns' desires, creature, worship)
audio::GameQueries MakeMusicQueries(Game& game)
{
	audio::GameQueries queries;
	queries.landNumber = []() { return Locator::mapScriptSystem::value().Globals().landNumber; };
	// (inferred) openblack's turn counter, back to 0 at each LoadMap, stands for the game's turn
	queries.turn = [&game]() { return game.GetTurn(); };
	queries.camera = []() -> std::optional<audio::CameraState> {
		if (!Locator::camera::has_value())
		{
			return std::nullopt;
		}
		audio::CameraState camera;
		camera.position = Locator::camera::value().GetOrigin();
		// the camera's map position is updated with the camera: x, z in map units (x 6553.6, truncated); the height
		// above ground = y - the altitude byte of the cell (x >> 16, z >> 16) x 0.67, not interpolated; y alone off the
		// map (a cell > 0x1FF) or where no block is
		float ground = 0.0f;
		if (Locator::terrainSystem::has_value())
		{
			const auto& island = Locator::terrainSystem::value();
			const glm::u16vec2 cell {map_coords::CellOf(map_coords::ToFixed(camera.position.x)),
			                         map_coords::CellOf(map_coords::ToFixed(camera.position.z))};
			if (cell.x <= 0x1FF && cell.y <= 0x1FF && island.HasBlockAt(cell))
			{
				ground = static_cast<float>(island.GetCellAltitude(island.GetCell(cell))) * LandIslandInterface::k_HeightUnit;
			}
		}
		camera.heightAboveGround = camera.position.y - ground;
		return camera;
	};
	// an available thing (ecs::IsAvailable) with a Transform; (approximate) its float position for the thing's
	// MapCoords (without their 16.16 rounding)
	queries.thingPosition = [](audio::ThingId thing) -> std::optional<glm::vec3> {
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = static_cast<entt::entity>(thing);
		if (!ecs::IsAvailable(entity))
		{
			return std::nullopt;
		}
		const auto* transform = registry.TryGet<ecs::components::Transform>(entity);
		if (transform == nullptr)
		{
			return std::nullopt;
		}
		return transform->position;
	};
	// the land's altitude, as a sound tag made at a map position takes it
	queries.landAltitude = [](float x, float z) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
	};
	// a script holds the wide screen (read by the alignment music)
	queries.scriptWideScreen = []() {
		const auto* helpSystem = help::Get();
		return helpSystem != nullptr && helpSystem->IsScriptWideScreen();
	};
	// the guidance level of the help system (0 when it is off); 3 without a help system (its defaults)
	queries.helpLevel = []() {
		const auto* helpSystem = help::Get();
		return helpSystem != nullptr ? helpSystem->GetGuidanceLevel() : 3;
	};
	// game_clock::IsInsideCitadel
	queries.insideCitadel = []() { return game_clock::IsInsideCitadel(); };
	// the local interface's player number: openblack's local player is PLAYER_ONE
	queries.localPlayerNumber = []() { return static_cast<uint32_t>(PlayerNames::PLAYER_ONE); };
	// whether it is night to the eye (read by the moon phase check of the help sprites)
	queries.visualNight = []() { return TheDayNightClock().IsVisualNight(); };
	// the interface's hand position (inferred: the hand's MapCoords, as the sound map takes it): the player's first
	// hand
	queries.handPosition = []() -> std::optional<glm::vec3> {
		if (!Locator::handSystem::has_value() || !Locator::entitiesRegistry::has_value())
		{
			return std::nullopt;
		}
		const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
		auto& registry = Locator::entitiesRegistry::value();
		if (!registry.Valid(hand))
		{
			return std::nullopt;
		}
		const auto* transform = registry.TryGet<const ecs::components::Transform>(hand);
		return transform != nullptr ? std::optional<glm::vec3>(transform->position) : std::nullopt;
	};
	// a help spirit's message: the help system's RunMessage and TriggerCategory
	queries.helpRunMessage = [&game](uint32_t first, uint32_t last, std::string_view script) {
		auto* helpSystem = help::Get();
		return helpSystem != nullptr && Locator::vm::has_value() &&
		       help::script_control::RunMessage(*helpSystem, first, last, script, chlapi::ScriptVm(), game.GetTurn());
	};
	queries.helpTriggerCategory = [](int category) {
		if (auto* helpSystem = help::Get(); helpSystem != nullptr)
		{
			helpSystem->TriggerCategory(category);
		}
	};
	// the spooky voices' name: the profile's name. (inferred) openblack has no profiles: OPENBLACK_PLAYER_NAME
	// (ASCII / UTF-8 letters as they are), else none
	queries.profileName = []() {
		std::u16string name;
		if (const char* env = std::getenv("OPENBLACK_PLAYER_NAME"); env != nullptr)
		{
			for (const char* c = env; *c != 0; ++c)
			{
				name.push_back(static_cast<char16_t>(static_cast<unsigned char>(*c)));
			}
		}
		return name;
	};
	// What the audio reads of the ECS things (the clips' villagers and animals, the lanterns, the surface, the weather)
	ecs::audio_queries::Fill(queries);
	return queries;
}

/// The PreDraw for the overlays (Graphics/OverlayFrame.h): what Renderer::DrawFinishFrameOverlays and
/// DrawHelpText draw, read from the game and laid out once a frame before DrawScene, at the Main view's resolution
void FillOverlayFrame(const ScreenFade& fade, graphics::OverlayFrame& out)
{
	const auto& renderer = Locator::rendererInterface::value();
	const auto resolution = renderer.GetResolution();
	out.width = resolution.x;
	out.height = resolution.y;
	// drawn at the end of the frame: the script fade's colour and the wide screen bars
	out.fade.colour = fade.GetColour();
	out.fade.barPixels = ScreenFade::LetterboxHeight(out.width, out.height, fade.GetWideScreenFraction());

	// the help text's frame (pending: its gate, a front-end state, is not ported)
	auto& helpText = out.helpText;
	const auto* helpSystem = help::Get();
	helpText.active = helpSystem != nullptr && out.width != 0 && out.height != 0;
	helpText.text = {};
	if (helpText.active)
	{
		const help::WidthFn widthOf = [&renderer](help::TextFont font, std::u16string_view text, float size) {
			return renderer.MeasureText(font, text, size);
		};
		// laid out again only when the texts, their animation, the flags or the screen changed
		helpText.text = helpSystem->GetDisplay().CachedLayout(
		    out.width, out.height, out.fade.barPixels, helpSystem->GetTextDraw(), helpSystem->GetTextTopToBottom(), widthOf);
	}
	// the key / mouse icons' draw half (Renderer::DrawInputPrompts): the input prompt icons as the two updates left them,
	// newest first; the hand's point on the screen for DrawKeyOrMouse (the hand's position projected; (inferred) its ints are
	// truncated); the tick count of the icons' update
	help::input_prompt::Snapshot(out.inputPrompts.icons);
	out.inputPrompts.tickCount = game_clock::TickCount();
	out.inputPrompts.hand.reset();
	if (Locator::handSystem::has_value() && Locator::camera::has_value() && out.width != 0 && out.height != 0)
	{
		const auto hands = Locator::handSystem::value().GetPlayerHands();
		const auto& registry = Locator::entitiesRegistry::value();
		glm::vec3 screen;
		if (!hands.empty() && registry.Valid(hands[0]) &&
		    Locator::camera::value().ProjectWorldToScreen(registry.Get<ecs::components::Transform>(hands[0]).position,
		                                                  glm::vec4(0.0f, 0.0f, out.width, out.height), screen))
		{
			out.inputPrompts.hand = glm::vec2(std::trunc(screen.x), std::trunc(screen.y)); // y from the top
		}
	}

	// the did-you-know bubble, drawn by the help system's 3D draw
	// (note) the bubble's life (Age) and scroll (ClampScroll) change here, in the fill, as the original changes them in
	// the draw; FillOverlayFrame runs once a frame on the logic side, so the Renderer still only reads the copy
	out.didYouKnow = {};
	if (auto* helpSystem = help::Get(); helpSystem != nullptr && helpSystem->GetBubbleThing().has_value() &&
	                                    !helpSystem->IsScriptWideScreen() && Locator::camera::has_value() && out.width != 0 &&
	                                    out.height != 0)
	{
		auto& bubble = helpSystem->GetBubble();
		const auto thing = static_cast<entt::entity>(*helpSystem->GetBubbleThing());
		const auto& registry = Locator::entitiesRegistry::value();
		const auto& camera = Locator::camera::value();
		if (registry.Valid(thing) && registry.AllOf<ecs::components::Transform>(thing) && bubble.Life() > 0.0f)
		{
			// (approximate) the Transform's point stands for the MapCoords' x, z and altitude + the thing's height
			const glm::vec3 point = registry.Get<ecs::components::Transform>(thing).position +
			                        glm::vec3(0.0f, ecs::object::GetHeight(thing) * 0.75f, 0.0f);
			const glm::vec3 origin = camera.GetOrigin();
			const glm::vec3 forward = glm::normalize(camera.GetFocus() - origin);
			const float depth = glm::dot(point - origin, forward);
			glm::vec3 screen;
			if (camera.ProjectWorldToScreen(point, glm::vec4(0.0f, 0.0f, out.width, out.height), screen))
			{
				const auto tail = help::TailFor(screen.x, out.width);
				if (const auto layout =
				        help::LayoutBubble(bubble.Sizes(), depth, {screen.x, screen.y}, tail.offset, tail.place))
				{
					// the dark colour at 255 x alpha
					constexpr uint32_t k_Colour = 0xFF3BBDED;
					const uint32_t dark = ((k_Colour >> 1) & 0x7F7F7Fu) | 0xFF000000u;
					const float alpha = std::min(bubble.Life(), 1.0f) * layout->fade;
					const auto a = static_cast<uint32_t>(std::clamp(static_cast<int>(alpha * 255.0f), 0, 255));
					// the scroll clamped with the last draw's text height, before it is taken in pixels for this
					// frame's draw (scroll x text size); the drag changes the scroll only after that
					const float size = layout->textSize;
					const auto marks = bubble.ClampScroll(layout->height, layout->margin, size);
					const float scrollPixels = bubble.Scroll() * size;
					// the text area: m = margin x 0.333333; t = (bottom - top) x 0.333333 +
					// top; the bottom b = bottom - m, or t when that is less; the clip b + m / 4; the start y = b + the
					// scroll in pixels
					const float m = layout->margin * 0.333333f;
					const float left = layout->topLeft.x + m;
					const float right = layout->topLeft.x + layout->width - m;
					const float top = layout->topLeft.y;
					const float boxBottom = layout->topLeft.y + layout->height;
					const float third = (boxBottom - top) * 0.333333f + top;
					const float bottom = boxBottom - m < third ? third : boxBottom - m;
					const float clip = bottom + m * 0.25f;
					float y = bottom + scrollPixels;
					float textHeight = 0.0f;
					const std::array<std::u16string, help::Bubble::k_Parts> parts = {helptext::Get(helpSystem->GetBubbleText()),
					                                                                 u"", helptext::Get(0x1016)};
					for (size_t partIndex = 0; partIndex < parts.size(); ++partIndex)
					{
						const auto& whole = parts.at(partIndex);
						// a part that starts with $g / $m keeps only that token (4 characters); else the text up to the
						// first $g (or else $m), then that token. As the original, what follows it is not drawn
						std::u16string part = whole;
						std::u16string token;
						auto at = whole.find(u"$g");
						if (at == std::u16string::npos)
						{
							at = whole.find(u"$m");
						}
						if (at != std::u16string::npos)
						{
							token = whole.substr(at, 4);
							part = whole.substr(0, at);
						}
						// the gathering text's line breaking (help::WrapGatheringText), each line `size` high; broken
						// again only when the part, the width or the size changed
						const auto& wrapped =
						    bubble.PartWrap(partIndex).Wrap(part, right - left, size, [&renderer, size](std::u16string_view t) {
							    return renderer.MeasureText(help::TextFont::J0, t, size);
						    });
						const float height = size * static_cast<float>(wrapped.size());
						y -= height; // bottom up
						textHeight += height;
						// a line whose bottom (y + size) is at or above the area's top is not drawn; else the alpha of
						// its top edge (at y) and of its bottom edge (at y + size) is faded in over [top, top + h / 3]
						// (GetFrac) and out over [b, b + m / 4] (1 - GetFrac), the colour's alpha byte times that,
						// truncated
						const float fadeInEnd = third;
						const auto edgeAlpha = [top, fadeInEnd, bottom, clip](float edge, uint8_t full) {
							float f = 1.0f;
							if (edge < fadeInEnd)
							{
								f = help::GetFrac(edge, top, fadeInEnd);
							}
							if (edge > bottom)
							{
								f = 1.0f - help::GetFrac(edge, bottom, clip);
							}
							return static_cast<uint8_t>(static_cast<int>(f * static_cast<float>(full)));
						};
						for (size_t i = 0; i < wrapped.size(); ++i)
						{
							const float ly = y + size * static_cast<float>(i);
							if (ly + size <= top)
							{
								continue;
							}
							const auto shadowA = static_cast<uint8_t>(std::clamp(static_cast<int>(alpha * 128.0f), 0, 255));
							const auto textA = static_cast<uint8_t>(a);
							out.didYouKnow.lines.push_back({{help::TextFont::J0, wrapped[i], left + 2.0f, ly + 2.0f, size, 0, 0,
							                                 0, edgeAlpha(ly + 2.0f, shadowA), top, clip},
							                                edgeAlpha(ly + 2.0f + size, shadowA)});
							out.didYouKnow.lines.push_back({{help::TextFont::J0, wrapped[i], left, ly, size, 255, 255, 255,
							                                 edgeAlpha(ly, textA), top, clip},
							                                edgeAlpha(ly + size, textA)});
						}
						// $g<n> (0 < n < 24) a square of 2 x size on its own step, its cell n % 16 of S_Gesture<n /
						// 16>, a shadow at +2 in the dark colour then the glyph. (approximate) the square's place: the
						// line's left, one step of 2 x size above; the original's step after it is a product x 0.7
						// (taken off y and added to the text height), its factors not traced; (pending) $m<n> (drawn
						// with DrawKeyOrMouse)
						if (token.size() >= 3 && token[1] == u'm')
						{
							// the action's binding, then DrawKeyOrMouse
							const int action = std::atoi(std::string(token.begin() + 2, token.end()).c_str());
							int32_t animType = 0;
							int32_t clickType = 0;
							int32_t row = 0;
							std::u16string keyName;
							if (help::input_prompt::ResolveAction(action, animType, clickType, row, keyName))
							{
								const float side = 2.0f * size;
								y -= side;
								textHeight += side;
								out.didYouKnow.keyIcons.push_back({animType, clickType, row, keyName,
								                                   static_cast<int32_t>((left + right) * 0.5f),
								                                   static_cast<int32_t>(y + side * 0.5f),
								                                   static_cast<int32_t>(side), static_cast<uint8_t>(a)});
							}
						}
						if (token.size() >= 3 && token[1] == u'g')
						{
							const int n = std::atoi(std::string(token.begin() + 2, token.end()).c_str());
							if (n > 0 && n < 24)
							{
								const float side = 2.0f * size;
								y -= side;
								textHeight += side;
								const int cell = n % 16;
								const float u0 = static_cast<float>(cell % 4) * 0.25f;
								const float v0 = static_cast<float>(cell / 4) * 0.25f;
								auto& quads = out.didYouKnow.gestures.at(static_cast<size_t>(n / 16));
								const auto quad = [&quads](float x, float qy, float w, float h, float uu, float vTop,
								                           float vBottom, uint32_t abgr) {
									const graphics::SpiritQuadVertex p0 {x, qy, 0.5f, uu, vTop, abgr};
									const graphics::SpiritQuadVertex p1 {x + w, qy, 0.5f, uu + 0.25f, vTop, abgr};
									const graphics::SpiritQuadVertex p2 {x + w, qy + h, 0.5f, uu + 0.25f, vBottom, abgr};
									const graphics::SpiritQuadVertex p3 {x, qy + h, 0.5f, uu, vBottom, abgr};
									quads.insert(quads.end(), {p0, p1, p2, p0, p2, p3});
								};
								const uint32_t darkAbgr =
								    (a << 24) | ((dark & 0xFFu) << 16) | (dark & 0xFF00u) | ((dark >> 16) & 0xFFu);
								// the quad cut to [top, clip], its v moved by the part cut
								// (a quarter of the cell per side length)
								const auto clipped = [&quad, top, clip, side](float x, float qy, float uu, float vv,
								                                              uint32_t abgr) {
									float y0 = qy;
									float y1 = qy + side;
									float vTop = vv;
									float vBottom = vv + 0.25f;
									if (y1 <= top || y0 >= clip)
									{
										return;
									}
									if (y0 < top)
									{
										vTop += (top - y0) / side * 0.25f;
										y0 = top;
									}
									if (y1 > clip)
									{
										vBottom -= (y1 - clip) / side * 0.25f;
										y1 = clip;
									}
									quad(x, y0, side, y1 - y0, uu, vTop, vBottom, abgr);
								};
								clipped(left + 2.0f, y + 2.0f, u0, v0, darkAbgr);
								clipped(left, y, u0, v0, (a << 24) | 0x00FFFFFFu);
							}
						}
					}
					// the text height (0, then each part's and icon's height added) is written by the z object's draw
					// only: none is queued while alpha x the fade is 0 or less, and the height keeps the last value
					if (alpha > 0.0f)
					{
						bubble.SetTextHeight(textHeight);
					}
					// the scroll arrows, while the tick count % 750 < 400, with h the box's height and s = 0.2 h, k = s
					// / 9: the below mark's arrow at the top right, the above mark's one at the bottom right, squares
					// of 0.5 s + 2 k in atmos.raw; colour {0, 0xFF, 0xFF, 0xFF}, alpha = the dark colour's alpha / 2
					if (game_clock::TickCount() % 750u < 400u)
					{
						const float boxTop = layout->topLeft.y;
						const float boxRight = layout->topLeft.x + layout->width;
						const float s5 = 0.2f * layout->height;
						const float k = s5 / 9.0f;
						const float xr = boxRight - 0.8f * s5;
						// the alpha of the z object's dark colour, stored at 0xFF and never faded: 0xFF >> 1
						out.didYouKnow.arrowAlpha = 0x7F;
						const auto ftolf = [](float v) { return static_cast<float>(static_cast<int>(v)); };
						if (marks.below)
						{
							out.didYouKnow.arrows.push_back({ftolf(xr + 0.25f * s5 - k), ftolf(boxTop + 0.5f * s5 - k),
							                                 ftolf(xr + k + 0.75f * s5), ftolf(boxTop + s5 + k), 0.87890625f,
							                                 0.12890625f, 0.99609375f, 0.24609375f});
						}
						if (marks.above)
						{
							out.didYouKnow.arrows.push_back({ftolf(xr + 0.25f * s5 - k), ftolf(boxBottom - s5 - k),
							                                 ftolf(xr + k + 0.75f * s5), ftolf(boxBottom - 0.5f * s5 + k),
							                                 0.75390625f, 0.00390625f, 0.87109375f, 0.12109375f});
						}
					}
					const auto shape = help::BubbleShape(layout->topLeft.x, layout->topLeft.y,
					                                     layout->topLeft.x + layout->width, layout->topLeft.y + layout->height,
					                                     layout->margin, layout->topLeft.x + layout->width * layout->tailPlace,
					                                     layout->tailRight, (dark & 0x00FFFFFFu) | (a << 24), true, 0, 0xFF);
					out.didYouKnow.active = !shape.empty();
					for (const auto& v : shape)
					{
						const uint32_t abgr = (v.argb & 0xFF00FF00u) | ((v.argb >> 16) & 0xFFu) | ((v.argb & 0xFFu) << 16);
						out.didYouKnow.shape.push_back({v.x, v.y, 0.5f, v.u, v.v, abgr});
					}
					// the hand's drag of the bubble and its hover
					{
						const auto mouse = input::GameCursor();
						const auto id = *helpSystem->GetBubbleThing();
						auto* hand = Locator::handSystem::has_value() ? &Locator::handSystem::value() : nullptr;
						bubble.Drag(hand != nullptr && hand->DraggedScreenObject() == id, mouse.y, size);
						const float x0 = layout->topLeft.x;
						const float y0 = layout->topLeft.y;
						const auto mx = static_cast<float>(mouse.x);
						const auto my = static_cast<float>(mouse.y);
						if (mx > x0 && mx < x0 + layout->width && my > y0 && my < y0 + layout->height)
						{
							if (hand != nullptr) // (openblack guard) the original always has its interface
							{
								static_cast<void>(hand->OfferScreenObject(id, layout->depth));
							}
							bubble.KeepAlive(); // life back to 3.0
						}
					}
				}
			}
		}
		bubble.Age(static_cast<float>(game_clock::FrameGameMs())); // the frame's game ms x 0.0002
	}

	// the advisor spirits (Help/SpiritsRuntime.h): the spirits and their trails. Not while the film covers the screen
	// or the falling spell hides the world: DrawScene draws no spirit then, and the halo clock stays (as when the
	// Renderer asked the runtime itself)
	out.spirits.clear();
	out.spiritTrails.clear();
	if (auto* spirits = help::spirits::Get();
	    spirits != nullptr && !video::Get().CoversScreen() && !video::GetFallingSpell().HidesWorld())
	{
		spirits->FillOverlay(out);
	}
}

/// The mod loader library next to the executable, when it is there: checked by version, then asked to read the mods
/// folder, with one log line either way. The game is not linked against it and starts the same without it
void LoadModLoader()
{
	std::filesystem::path baseDirectory;
	if (char* base = SDL_GetBasePath(); base != nullptr)
	{
		baseDirectory = base;
		SDL_free(base);
	}
#if defined(_WIN32)
	constexpr const char* k_LibraryName = "ModLoader.dll";
#elif defined(__APPLE__)
	constexpr const char* k_LibraryName = "ModLoader.dylib";
#else
	constexpr const char* k_LibraryName = "ModLoader.so";
#endif
	auto logger = spdlog::get("game");
	// a missing library leaves SDL's error message as it was
	const std::string sdlError = SDL_GetError();
	void* library = SDL_LoadObject((baseDirectory / k_LibraryName).string().c_str());
	if (library == nullptr)
	{
		if (sdlError.empty())
		{
			SDL_ClearError();
		}
		else
		{
			SDL_SetError("%s", sdlError.c_str());
		}
		SPDLOG_LOGGER_INFO(logger, "ModLoader: no library, no mods");
		return;
	}
	const auto version = reinterpret_cast<ModLoaderVersionFn>(SDL_LoadFunction(library, k_ModLoaderVersionSymbol));
	const auto loadMods = reinterpret_cast<ModLoaderLoadModsFn>(SDL_LoadFunction(library, k_ModLoaderLoadModsSymbol));
	if (version == nullptr || loadMods == nullptr || version() != k_ModLoaderVersion)
	{
		SPDLOG_LOGGER_INFO(logger, "ModLoader: the library is not version {}, no mods", k_ModLoaderVersion);
		SDL_UnloadObject(library);
		return;
	}
	auto& status = Locator::modLoader::emplace();
	status.version = version();
	const auto keepLine = [](const char* line, void* context) { static_cast<ModLoaderStatus*>(context)->result = line; };
	loadMods((baseDirectory / "Mods").string().c_str(), keepLine, &status);
	SPDLOG_LOGGER_INFO(logger, "ModLoader {}: {}", status.version, status.result);
	// nothing of the library is used after this yet
	SDL_UnloadObject(library);
}
} // namespace

Game* Game::sInstance = nullptr;

Game::Game(Arguments&& args) noexcept
    : _gamePath(args.gamePath)
    , _startMap(args.startLevel)
{
	// with the members, before anything else: the script fade, the day / night clock, the map script's globals and
	// the frame count with its screenshot request (--screenshot-frame's, if given)
	InitializeGameState();
	if (args.requestScreenshot.has_value())
	{
		Locator::screenshotRequest::value().RequestAt(args.requestScreenshot->first, args.requestScreenshot->second);
	}
	Locator::camera::emplace(glm::zero<glm::vec3>());
	// what the draw reads instead: a copy of it made every frame (Run)
	_drawCamera = std::make_unique<Camera>();
	std::function<std::shared_ptr<spdlog::logger>(const std::string&)> createLogger;
#ifdef __ANDROID__
	if (!args.logFile.empty() && args.logFile == "logcat")
	{
		createLogger = [](const std::string& name) { return spdlog::android_logger_mt(name, "spdlog-android"); };
	}
	else
#endif // __ANDROID__
	{
		if (!args.logFile.empty() && args.logFile != "stdout")
		{
			// One file sink shared by every subsystem's logger: with a basic_logger_mt each, every logger opened the same
			// file on its own and their writes overwrote each other (lines went missing from openblack.log)
			auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(args.logFile);
			createLogger = [fileSink](const std::string& name) {
				auto logger = std::make_shared<spdlog::logger>(name, fileSink);
				spdlog::register_logger(logger);
				return logger;
			};
		}
		else
		{
			createLogger = [](const std::string& name) { return spdlog::stdout_color_mt(name); };
		}
	}
	// TODO (#749) use std::views::enumerate
	for (size_t i = 0; const auto& subsystem : k_LoggingSubsystemStrs)
	{
		auto logger = createLogger(subsystem.data());
		logger->set_level(args.logLevels.at(i));
		// test hook: OPENBLACK_FLUSH_LOG=1 writes every line at once (the last lines before a crash are kept)
		if (std::getenv("OPENBLACK_FLUSH_LOG") != nullptr)
		{
			logger->flush_on(spdlog::level::trace);
		}
		++i;
	}
	sInstance = this;
	// the game timer at speed 1, as the game starts; paused until a map is loaded (openblack)
	// OPENBLACK_FIXED_FRAME_MS: the same ms every frame, for runs that must repeat (Debug/FixedClock.h)
	InitializeClock();
	fixed_clock::InstallFromEnvironment();
	game_clock::Reset();
	game_clock::Start(true);

	auto& config = Locator::config::emplace();
	config.numFramesToSimulate = args.numFramesToSimulate;
	config.numFramesToSimulate = args.numFramesToSimulate;
	config.numFramesToSimulate = args.numFramesToSimulate;
	config.resolution = {args.windowWidth, args.windowHeight};
	config.displayMode = args.displayMode;
	config.graphicsBackend = args.graphicsBackend;
	config.vsync = args.vsync;
	config.detailLevel = args.detailLevel;
	config.profileCreatureFile = args.creatureFile;
	// Mods: the mod loader library checks the mods folder's version and loads no mod yet. A mod SDK is planned on it
	// once the whole game is rebuilt: it should give the mods only the Locator's interfaces, send them the game's
	// events instead of calls from the game loop, and let them replace resources only through the resource loaders
	LoadModLoader();
	config.guiScale = args.guiScale;
}

Game::~Game() noexcept
{
	// the audio's shutdown order: the sample channels, then the game music before the music player, then the music
	// thread and its OpenAL sources before the audio context
	// the guidance closes with the interface status; then the spooky voices shut down
	audio::guidance::Close();
	audio::spooky::Shutdown();
	audio::Shutdown();
	audio::game_music::Shutdown();
	audio::music::Shutdown();
	// the temple's scrolls are written with the interface's text, and their textures go before the renderer too
	if (Locator::temple::has_value())
	{
		Locator::temple::value().SetInterface(nullptr);
	}
	// and the Creature Cave's screen is written with it
	if (Locator::creatureCaveSystem::has_value())
	{
		Locator::creatureCaveSystem::value().SetInterface(nullptr);
	}
	_interface.reset();        // its textures before the renderer
	help::spirits::Shutdown(); // its meshes before the renderer goes
	help::Shutdown();
	ShutDownServices();
	SDL_Quit(); // todo: move to GameWindow
	spdlog::shutdown();
}

bool Game::ProcessEvents(const SDL_Event& event) noexcept
{
	if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP)
	{
		const input::MouseButtonEvent button {
		    .button = event.button.button == SDL_BUTTON_LEFT     ? input::MouseButton::Left
		              : event.button.button == SDL_BUTTON_MIDDLE ? input::MouseButton::Middle
		              : event.button.button == SDL_BUTTON_RIGHT  ? input::MouseButton::Right
		                                                         : input::MouseButton::Other,
		    .down = event.type == SDL_MOUSEBUTTONDOWN,
		    .position = {event.button.x, event.button.y},
		};
		input::ApplyMouseButton(_mouseButtons, button);
		// the interface passes the click to the help system (inferred: the click is the left button going down)
		if (auto* helpSystem = help::Get(); helpSystem != nullptr && button.button == input::MouseButton::Left && button.down)
		{
			helpSystem->ProcessInterface(true);
		}
	}

	// A hand demo plays the recorded buttons (the mouse buttons are not read while it plays)
	if (!hand_demo::IsPlaying())
	{
		auto& hand = input::HandButtonsRef();
		hand.gripping = input::HandGripping(_mouseButtons);
		hand.action = input::HandAction(_mouseButtons);
	}

	auto& window = Locator::windowing::value();
	auto& camera = Locator::camera::value();

	switch (event.type)
	{
	case SDL_QUIT:
		return false;
	case SDL_WINDOWEVENT:
		if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == window.GetID())
		{
			return false;
		}
		else if (event.window.event == SDL_WINDOWEVENT_MINIMIZED || event.window.event == SDL_WINDOWEVENT_RESTORED)
		{
			// the screen's activation switches the audio off / on. Not on a focus change: the original deactivates on
			// minimise (inferred WM_SIZE SIZE_MINIMIZED) and reactivates on restore (inferred WM_SYSCOMMAND SC_RESTORE)
			audio::OnFocus(event.window.event == SDL_WINDOWEVENT_RESTORED);
		}
		else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
		{
			const auto resolution = glm::u16vec2(event.window.data1, event.window.data2);
			Locator::rendererInterface::value().Reset(resolution);
			Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Main, resolution, 0x274659ff);

			auto aspect = window.GetAspectRatio();
			const auto& config = Locator::config::value();
			camera.SetProjectionMatrixPerspective(config.cameraXFov, aspect, config.cameraNearClip, config.cameraFarClip);
		}
		break;
	case SDL_KEYDOWN:
		switch (event.key.keysym.sym)
		{
		case SDLK_ESCAPE:
			// with a full screen film ESC skips it (Video/VideoPlayer.h); without one openblack quits. (inferred) one
			// skip a press: SDL's key repeats are not keys of the original
			if (video::IsPlaying())
			{
				if (event.key.repeat == 0)
				{
					video::Get().EscapeKey((event.key.keysym.mod & KMOD_SHIFT) != 0, (event.key.keysym.mod & KMOD_CTRL) != 0);
				}
				break;
			}
			// nothing while a script holds the wide screen (a tutorial cinema or
			// hand demo; a demo whose bars come only from StartPlayBack, owner 0, is not protected: inferred)
			if (const auto* helpSystem = help::Get(); helpSystem != nullptr && helpSystem->IsScriptWideScreen())
			{
				break;
			}
			// with the in-game menus, an Escape they did not take (Shift or Ctrl held) does nothing, as in the
			// original; openblack quits only without them
			if (_interface)
			{
				break;
			}
			return false;
		case SDLK_f:
			window.SetDisplayMode(windowing::DisplayMode::Fullscreen);
			break;
		case SDLK_p:
			game_clock::Pause(!game_clock::IsPaused());
			break;
		case SDLK_F1:
			Locator::rendererInterface::value().SetDebug(!Locator::rendererInterface::value().GetDebug());
			break;
		case SDLK_1:
		case SDLK_2:
		case SDLK_3:
		case SDLK_4:
		case SDLK_5:
		case SDLK_6:
		case SDLK_7:
		case SDLK_8:
			// the keys 1..Tab are skipped while a script holds the wide screen or a SET_INTERFACE_INTERACTION level has
			// a ControlMap switch off
			if ((help::Get() != nullptr && help::Get()->IsScriptWideScreen()) ||
			    !help::interface_interaction::KeyShortcutsEnabled())
			{
				break;
			}
			// the bookmark keys only while they are enabled (PLAY_JC_SPECIAL 14 / 15)
			if (!Locator::cameraBookmarkSystem::value().IsEnabled())
			{
				break;
			}
			if ((event.key.keysym.mod & KMOD_CTRL) != 0)
			{
				const auto index = static_cast<uint8_t>(event.key.keysym.sym - SDLK_1);
				const auto positions = Locator::handSystem::value().GetPlayerHandPositions();
				if (positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)] ||
				    positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Right)])
				{
					const auto handPosition =
					    positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)].value_or(
					        positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Right)].value_or(
					            glm::zero<glm::vec3>()));
					Locator::cameraBookmarkSystem::value().SetBookmark(index, handPosition, camera.GetOrigin());
				}
			}
			else
			{
				const auto& entitiesRegistry = Locator::entitiesRegistry::value();
				const size_t index = event.key.keysym.sym - SDLK_1;
				const auto& bookmarkEntities = Locator::cameraBookmarkSystem::value().GetBookmarks();
				const auto entity = bookmarkEntities.at(index);
				const auto [transform, bookmark] =
				    entitiesRegistry.TryGet<ecs::components::Transform, ecs::components::CameraBookmark>(entity);
				if (transform != nullptr && bookmark != nullptr)
				{
					// The flight is the player's camera's: Creature Mode hands it back first
					if (Locator::creatureModeSystem::has_value())
					{
						Locator::creatureModeSystem::value().Leave();
					}
					camera.GetModel().SetFlight(bookmark->savedOrigin, transform->position);
				}
			}
			break;
		}
		break;
	case SDL_MOUSEMOTION:
	{
		auto& cursor = input::GameCursorRef();
		SDL_GetMouseState(&cursor.x, &cursor.y);
		break;
	}
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		switch (event.button.button)
		{
		case SDL_BUTTON_MIDDLE:
		{
			// Relative mode while held: the cursor stays put and only the motion drives the camera.
			// (the press position is kept by ApplyMouseButton above)
			const bool pressed = event.type == SDL_MOUSEBUTTONDOWN;
			SDL_SetRelativeMouseMode(pressed ? SDL_TRUE : SDL_FALSE);
			if (!pressed)
			{
				const auto& pressPosition = _mouseButtons.middlePressPosition;
				SDL_WarpMouseInWindow(static_cast<SDL_Window*>(window.GetHandle()), pressPosition.x, pressPosition.y);
			}
		}
		break;
		}
		break;
	}

	return true;
}

bool Game::GameLogicLoop() noexcept
{
	using namespace ecs::components;
	using namespace ecs::systems;

	// one game turn: the game inputs before the game code, so before the turn number goes up: the packets the last
	// flush passed on (Input/GamePackets), the hand's part of the interface's process, then the interface's pump with
	// the power-up system
	game_packets::DispatchQueuedPackets();
	if (Locator::handSystem::has_value())
	{
		Locator::handSystem::value().ProcessTurn();
	}
	magic::ProcessGameInputs();
	// the turn number goes up at the start of the turn
	game_clock::StartTurn();
	const auto currentTime = std::chrono::steady_clock::now();
	_turnDeltaTime = currentTime - _lastGameLoopTime;
	_lastGameLoopTime = currentTime;
	const uint32_t turn = game_clock::Turn();

	auto& profiler = Locator::profiler::value();
	// Build Map Grid Acceleration Structure
	{
		auto mapRebuild = profiler.BeginScoped(Profiler::Stage::TurnMapRebuild);
		Locator::entitiesMap::value().Rebuild();
	}
	// the reactions' clock (the game turn) for the whole turn, and the ones whose initiator went
	// (ECS/Effects/Reactions)
	ecs::effects::reactions::BeginTurn();

	// the game's turn, call by call in the original's order (docs/bw1-notes/engine-loop.md §2)
	// the sharks' turn
	ecs::ProcessSharksTurn();
	// the atmosphere, the influence rings, the players (their towns' turn, ECS/Town) and the dances
	// (Magic/MagicLoop.cpp)
	magic::ProcessTurnStart(turn);
	// the global game lists: the fields, the fish farms, the walk along the path of the WALK_PATH list, (not ported)
	// two more lists, the puzzle games
	ecs::ProcessFieldsTurn(turn);
	ecs::ProcessFishFarmsTurn(turn);
	ecs::ProcessMobileWalkPaths();
	ecs::ProcessPuzzleGamesTurn();
	// the forests, before the living
	magic::ProcessForests(turn);

	// the living: the villagers and the animals in the one living list, each one's turn start (its position kept as the
	// last one), reaction and state, the walk inside the states that walk (ECS/LivingTurn.h)
	{
		auto actions = profiler.BeginScoped(Profiler::Stage::LivingActionUpdate);
		ecs::living_turn::ProcessLiving(TheDayNightClock().GetVisualTime());
		// (approximate) the creatures' turn, after the living list rather than inside it (ECS/CreatureLoop.h)
		ecs::creature_loop::ProcessTurn(profiler);
		// what follows the list on openblack's side: the souls of the dead (LivingActionSystem::Update)
		Locator::livingActionSystem::value().Update();
	}
	// the fire, the balls, the reactions, the spells (Magic/MagicLoop.cpp)
	{
		auto magicTurn = profiler.BeginScoped(Profiler::Stage::TurnMagic);
		magic::ProcessTurn(turn);
	}
	// the particle effects, one step per turn of the turn's length (the test hooks first)
	psys::manager::RunDebugHooks();
	magic::RunDebugHooks();
	{
		auto particlesTurn = profiler.BeginScoped(Profiler::Stage::TurnParticles);
		psys::manager::ProcessTurn(game_clock::k_TurnSeconds);
	}
	// the fireflies
	ecs::ProcessFireFliesTurn(TheDayNightClock());
	// the physics objects
	{
		auto physicsTurn = profiler.BeginScoped(Profiler::Stage::TurnPhysicsObjects);
		ecs::physics::PhysicsObjects::GameTurnUpdate();
	}
	// the particle system's end of loop: the exploded meshes' queue and the PSys sounds (Magic/MagicLoop.cpp)
	magic::ProcessSpellParticlesEndOfLoop();

	{
		auto scripts = profiler.BeginScoped(Profiler::Stage::ScriptsUpdate);
		// the scripts
		auto& lhvm = Locator::vm::value();
		// the countdown timer's turn, before the scripts
		ecs::script_countdown::ProcessTurn();
		lhvm.LookIn(lhvm::ScriptType::All);
		// the things no script variable holds any more are released
		ecs::script_held::Process();
		// the script fade, once per turn
		TheScreenFade().ProcessTurn();
		// the help system: the hand state, the input prompt icons, the spirits' turn and the tooltips
		help::Process(chlapi::ScriptVm(), GetTurn());
		// the help profile, after the scripts and the help system: the events of this turn are counted
		// (GET_TOTAL_EVENTS), the next turn may count them again
		help_profile::Process();
		// the day / night clock
		TheDayNightClock().ProcessTurn();
		// OPENBLACK_TIME_OF_DAY=<script hour> pins the clock there every turn (screenshots), over the scripts' times
		if (const char* hour = std::getenv("OPENBLACK_TIME_OF_DAY"); hour != nullptr)
		{
			TheDayNightClock().SetScriptTime(std::clamp(static_cast<float>(std::atof(hour)), 0.0f, 24.0f));
		}
		Locator::skySystem::value().SetTime(TheDayNightClock().GetScriptTime());
		if (turn % 50 == 0 && std::getenv("OPENBLACK_CLOCK_TRACE") != nullptr)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Clock: turn {} visual {:.4f} script {:.4f} sky type {:.3f} frame {:.3f} dome {:.3f}", turn,
			                   TheDayNightClock().GetVisualTime(), TheDayNightClock().GetScriptTime(),
			                   TheDayNightClock().SkyType(), sky_type::Frame(), sky_type::Dome().Built());
		}
		// OPENBLACK_TEST_TEXT_CLICK=1 (openblack only): the player's click on a text that waits for one (RUN_TEXT with
		// interaction 1), every turn while it waits, as the left button going down does (ProcessEvents);
		// ProcessInterface itself ignores the click until the text has been shown long enough
		if (static const bool textClick = std::getenv("OPENBLACK_TEST_TEXT_CLICK") != nullptr; textClick)
		{
			if (auto* helpSystem = help::Get(); helpSystem != nullptr && helpSystem->IsWaitingForClick())
			{
				helpSystem->ProcessInterface(true);
				if (!helpSystem->IsWaitingForClick())
				{
					SPDLOG_LOGGER_INFO(spdlog::get("game"), "OPENBLACK_TEST_TEXT_CLICK: click taken at turn {}", turn);
				}
			}
		}
	}
	// the weather things
	weather::ProcessWeatherThings();
	// the bookmarks: (not ported)
	// the script highlights
	ecs::script_highlight::ProcessHighlights();
	// the climate (+ the weather's test hooks)
	weather::ProcessClimate();
	ecs::town_belief::ProcessOncePerTurn(); // the belief
	// the hand's turn (Magic/MagicLoop.cpp)
	magic::ProcessHandTurn();
	// the players' sparkles, the mobile objects' checksum, the dead list and the rewards: (not ported)
	// the spooky voices, the help sprites' moon phase check, the town desires' sounds (and the interface's heart beat),
	// the confirmations (Audio/Services/Guidance.h)
	audio::guidance::ProcessGameTurn();
	// the influence circles rebuilt every 10 turns when a radius moved
	// (ECS/Influence/InfluenceCircles.cpp)
	influence::Update3DInfluence();
	// the camera's stacked modes are checked: the script camera's things that have gone are dropped
	script_camera::Validate();
	// each fragment's timer, once a turn
	ecs::physics::Buildings::ProcessTurn();
	// the music mood: (not verified) in audio::ProcessTurn
	// the end of the turn (unpaused: this loop does not run in pause): the sound map, the sound tags (the street
	// lanterns' too), then the audio's turn after turn 5 (its music, atmos, channels and listener), the atmos before
	audio::ProcessTurn();
	ecs::audio_queries::RunTestHooks(turn); // OPENBLACK_AUDIO_TEST_VIEW / _ANIM / _LANTERN / _CITADEL
	ecs::effects::reactions::EndTurn();
	// after the game code the original also runs the wall huggers' lookahead and the repair of missing mothers

	// OPENBLACK_STATE_HASH: the state of this turn, for the replay test (Debug/StateHash.h)
	state_hash::OnTurnEnd(turn);

	return false;
}

bool Game::Update() noexcept
{
	auto& profiler = Locator::profiler::value();

	profiler.Frame();

	auto& camera = Locator::camera::value();
	auto& config = Locator::config::value();

	auto previous = profiler.GetEntries().at(profiler.GetEntryIndex(-1)).frameStart;
	auto current = profiler.GetEntries().at(profiler.GetEntryIndex(0)).frameStart;
	// Prevent spike at first frame
	if (previous.time_since_epoch().count() == 0)
	{
		current = previous;
	}
	auto deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(current - previous);
	if (fixed_clock::Enabled())
	{
		deltaTime = fixed_clock::AdvanceFrame();
	}

	Locator::debugGui::value().SetScale(config.guiScale);

	// (openblack) Bullet is not stepped: the original has no rigid-body world (its physics is the physics objects'
	// update, once a turn); the dynamics system only answers ray casts (the hand, the
	// camera, the console). Stepping it with the wall clock moved the Features and dropped their yaw every frame.

	// Input events
	{
		auto sdlInput = profiler.BeginScoped(Profiler::Stage::SdlInput);
		if (!Locator::debugGui::value().StealsFocus())
		{
			Locator::gameActionSystem::value().Frame();
		}
		SDL_Event e;
		while (SDL_PollEvent(&e) != 0)
		{
			// OPENBLACK_FIXED_MOUSE and OPENBLACK_IGNORE_REAL_INPUT (test runs): the real mouse is not dispatched
			if (input::DropRealEvent(e.type, input::FixedMouse().has_value(), input::IgnoreRealInput()))
			{
				continue;
			}
			Locator::events::value().Create<SDL_Event>(e);
		}
		// the script camera mode has no keys; nor does a hand demo, which sets the camera from its records
		if (!script_camera::Drives() && !hand_demo::IsPlaying())
		{
			camera.HandleActions(deltaTime);
		}
		// The room keys are read as raffclar's temple port reads them, during script cameras and hand demos too
		ProcessTempleRoomKeys();
		// the leash keys, not while the debug windows have the keyboard (the actions are not framed then, so a key would
		// read as pressed again every frame), nor in the citadel, where the world stands still (ECS/CreatureLoop.h)
		if (!Locator::debugGui::value().StealsFocus() &&
		    !ecs::creature_loop::FrozenInCitadel(game_clock::IsPaused(), game_clock::IsInsideCitadel()))
		{
			ecs::creature_loop::ProcessLeashKeys(Locator::gameActionSystem::value());
		}
	}

	if (!config.running)
	{
		return false;
	}

	// ImGui events + prepare
	{
		auto guiLoop = profiler.BeginScoped(Profiler::Stage::GuiLoop);
		if (Locator::debugGui::value().Loop())
		{
			return false; // Quit event
		}
	}
	// The in-game editor keeps its camera on what it has picked
	if (Locator::editorSystem::has_value())
	{
		auto editor = profiler.BeginScoped(Profiler::Stage::EditorUpdate);
		Locator::editorSystem::value().Update(deltaTime);
	}

	// Update Game Logic in Registry: the game loop's turns before the frame clock and the draw
	{
		auto gameLogic = profiler.BeginScoped(Profiler::Stage::GameLogic);
		// Inside the temple the world is paused (TempleInterior::Activate) and its turns wait, while the temple has a
		// turn of its own every 100 ms of the clock's ticks
		if (game_clock::IsPaused() && game_clock::IsInsideCitadel() && _pausedTurn.Due(game_clock::TickCount()))
		{
			ProcessTempleTurn();
		}
		// while the local timer says a turn is due and fewer than 1 turn this frame (game_clock::TurnDue)
		while (game_clock::TurnDue())
		{
			if (GameLogicLoop())
			{
				return false; // Quit event
			}
		}
		// the end of the turn while paused: the audio's atmos only. Not inside the citadel, which ends no turn there: its
		// own turn's audio lets the ambience fade out instead of cutting it
		if (game_clock::IsPaused() && !game_clock::IsInsideCitadel())
		{
			audio::Paused();
		}
	}
	// the packets sent so far go to the session; the next turn applies them
	game_packets::Flush();
	// the remainder, the visual clock, the frame's game ms and the fraction of the turn; then the frame's real delta
	// time, as the render's frame start takes it
	game_clock::UpdateFrameClock();
	game_clock::UpdateRealClock();
	{
		auto cameraSection = profiler.BeginScoped(Profiler::Stage::CameraUpdate);
		// the camera's update (with the graphics, after the turns and the frame clock): the script camera mode moves it
		// while it lives, else the player's model, with this frame's game ms
		const auto lastDrawn = camera.GetOrigin(); // the camera drawn the frame before, shake included
		if (!script_camera::UpdateCamera(camera, static_cast<float>(game_clock::CameraFrameMs()) * 0.001f,
		                                 game_clock::FrameGameMs(), game_clock::FrameGameSeconds()))
		{
			// Creature Mode keeps the camera on its creature, and takes it or gives it back before it moves
			if (Locator::creatureModeSystem::has_value())
			{
				Locator::creatureModeSystem::value().Update(
				    deltaTime, {.handGripping = input::HandGripping(_mouseButtons),
				                // the hand's pick of its last update: the camera moves before the hand
				                .creatureUnderHand = Locator::creatureHandSystem::has_value()
				                                         ? Locator::creatureHandSystem::value().CreatureUnderHand()
				                                         : std::nullopt,
				                .doubleClickFlies = camera_help::IsFeatureEnabled(camera_help::Feature::DoubleClickFly)});
			}
			camera.Update(deltaTime);
		}
		else if (Locator::creatureModeSystem::has_value() && Locator::creatureModeSystem::value().IsActive())
		{
			// A script's camera ends Creature Mode
			Locator::creatureModeSystem::value().Leave();
		}
		script_camera::ApplyShake(camera, lastDrawn); // SHAKE_CAMERA on whichever camera is drawn
		// The original's near plane follows the camera height above the ground: 0.3 + 0.16 h, clamped to 0.3..3.5. A
		// camera model with a lens of its own, as the temple's, keeps its own
		if (!camera.GetModel().GetLens().has_value() && Locator::terrainSystem::has_value() && Locator::windowing::has_value())
		{
			const auto origin = camera.GetOrigin();
			const float height = origin.y - Locator::terrainSystem::value().GetHeightAt(glm::vec2(origin.x, origin.z));
			const float nearClip = std::clamp(0.3f + 0.16f * height, 0.3f, 3.5f);
			auto& config = Locator::config::value();
			if (std::abs(nearClip - config.cameraNearClip) > 0.01f)
			{
				config.cameraNearClip = nearClip;
				camera.SetProjectionMatrixPerspective(config.cameraXFov, Locator::windowing::value().GetAspectRatio(),
				                                      config.cameraNearClip, config.cameraFarClip);
			}
		}
		// The temple's frame: what its rooms do by themselves, and leaving it when its camera or a key asked to
		if (Locator::temple::has_value())
		{
			Locator::temple::value().Update(deltaTime);
		}
		// The Creature Cave follows the temple into and out of its creature room
		if (Locator::creatureCaveSystem::has_value())
		{
			Locator::creatureCaveSystem::value().Update();
		}
		Locator::cameraBookmarkSystem::value().Update(deltaTime);
	}
	if (_interface) // the in-game menus
	{
		// openblack's test hooks (not the original's; off unless the variable is set): OPENBLACK_TEST_MENU_AT=<frame>
		// opens the escape menu, OPENBLACK_TEST_SKIP_ANSWER=<answer>,<frame> answers the SkipBox (screenshots without a
		// keyboard or mouse)
		const uint32_t frame = Locator::screenshotRequest::value().Frame();
		if (const char* at = std::getenv("OPENBLACK_TEST_MENU_AT");
		    at != nullptr && frame == static_cast<uint32_t>(std::strtoul(at, nullptr, 10)))
		{
			_interface->GetMenu().Open();
		}
		if (const char* skip = std::getenv("OPENBLACK_TEST_SKIP_ANSWER"); skip != nullptr)
		{
			const std::string spec(skip);
			const auto comma = spec.find(',');
			if (comma != std::string::npos &&
			    frame == static_cast<uint32_t>(std::strtoul(spec.substr(comma + 1).c_str(), nullptr, 10)))
			{
				_interface->AnswerSkipBox(std::atoi(spec.substr(0, comma).c_str()));
			}
		}
		_interface->Update(std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());
		HandleInterfaceAction();
	}
	// the full screen film's frame (Video/VideoPlayer.h), paced by the wall
	// clock (the game is paused while it plays)
	video::Get().Process(game_clock::FrameRealMs());
	// the falling spell's film (its update, its end at state 4 or without a film) and the temple's fade, with the real
	// delta time (Video/FallingSpellVideo.h)
	video::GetFallingSpell().ProcessFrame(game_clock::FrameRealMs());
	// with the landscape's draw: the physics objects drawn between their last two turn poses (before the
	// trees' bending, which takes them as sources, and before the drawing), and the dust
	// the profile's "Frame Updaters" (no return before its End below: Profiler::End checks the level)
	profiler.Begin(Profiler::Stage::FrameUpdaters);
	ecs::physics::PhysicsObjects::UpdateFrame(GetTurnFraction(), game_clock::FrameGameSeconds());

	// Fields: visibility and sinking with their food (as their draw does)
	ecs::UpdateFields(std::chrono::duration<float>(deltaTime).count());
	// the trees' draw: their brightness this frame and the rustle of the tall ones by the camera
	ecs::UpdateTrees(std::chrono::duration<float>(deltaTime).count());

	// Fireflies (as their draw does): orbit and fade, in game time
	ecs::UpdateFireFlies(game_clock::FrameGameSeconds(), camera.GetOrigin());
	// the script highlights: the scrolls turn (the frame's game ms x pi / 1000) and grow with the camera's distance
	ecs::script_highlight::UpdateFrame(game_clock::FrameGameMs(), camera.GetOrigin());
	// the workshop's draw: the needs signs (the frame's game ms x 0.001)
	ecs::show_needs::UpdateFrame(static_cast<float>(game_clock::FrameGameMs()) * 0.001f, camera.GetOrigin());

	// Water rings: the frame's game time, in milliseconds
	ecs::UpdateWaterRings(static_cast<float>(game_clock::FrameGameMs()));
	// the designed scenery of Land 3 (waterfall) and Land 4 (ark, dinosaur), by land number
	ecs::designed_scenery::Update(static_cast<float>(game_clock::FrameGameMs()));
	// The marks on the ground (just before the smoky stuff): fade and go
	ecs::ground_marks::Update(static_cast<float>(game_clock::FrameGameMs()));
	// the missionaries' boat before its draw, the smoky stuff, after its draw (ecs/MissionaryBoat.h). The smoky stuff also
	// moves the smoke an object leaves when it goes (ecs/DisappearSmoke.h), in game time, boat or not.
	ecs::missionary_boat::Update(static_cast<float>(game_clock::FrameGameMs()));
	// in the landscape's draw, before the SuperVillager loop: the intro hand's grip, from the hand as the last frame
	// left it (ecs/IntroSpecial.h)
	ecs::super_villager::SetIntroHandGrip(ecs::intro_special::Grip());
	// just before the boat's after-draw, and the light's Z-object callback of this frame: the intro light, the debug
	// camera's points and the intro hand. (approximate) after the boat's update here. The light's CRT Random draws:
	// (approximate, pending): the original draws in the Z drain, after the clouds, night lights and smoke; to move to
	// the end of Renderer::PreDraw
	ecs::intro_special::Update(game_clock::FrameGameMs());

	// in the landscape's draw: the SuperVillagers go without a script wide screen; the others take the on-screen
	// test and set this frame's fade and yaw parameters of the smooth drawing (ecs/SuperVillager.h)
	ecs::super_villager::Update();
	// Villagers and animals drawn between turns, turning smoothly, on the slope (ecs/MobileDrawing.h)
	ecs::UpdateMobileDrawing(GetTurnFraction(), static_cast<float>(game_clock::FrameGameMs()));
	// after the villagers' draw: a SuperVillager with feature 7 at the intro hand's grip
	ecs::super_villager::FollowHand();
	// Skeletal animation of villagers and animals (ecs/Animations.h), in milliseconds of game time
	ecs::UpdateVillagerAnimations();
	ecs::UpdateAnimalAnimations();
	ecs::UpdateAnimations(static_cast<float>(game_clock::FrameGameMs()));
	ecs::UpdateCarriedProps();
	// the creatures between turns, which stand still with the world while it is paused in the citadel
	// (ECS/CreatureLoop.h)
	if (!ecs::creature_loop::FrozenInCitadel(game_clock::IsPaused(), game_clock::IsInsideCitadel()))
	{
		ecs::creature_loop::UpdateFrame(GetTurnFraction(), game_clock::FrameGameMs(), profiler);
	}
	// the sharks drawn between turns, heading, wake rings (ecs/Sharks.h)
	ecs::UpdateSharks(GetTurnFraction(), static_cast<float>(game_clock::FrameGameMs()));

	// FishFarm shoals, moved with the frame's game time
	ecs::UpdateFishShoals(game_clock::FrameGameSeconds(), camera.GetOrigin());
	// after the fish and the boat's after-draw: each SuperVillager's eyes after its body, on screen only
	// (ecs/SuperVillagerEyes.h); the swim rings' Random(0, 2 pi) of the driver loop came before them, in Update
	ecs::super_villager::Draw(static_cast<int32_t>(game_clock::FrameGameMs()));
	// the town centres' draw: the town belief symbols' PSys step, once a rendered frame
	psys::town_belief::Step();

	// from the help system's Draw3D: the cinema bars slide with the game time of this frame
	TheScreenFade().UpdateWideScreen(static_cast<float>(game_clock::FrameGameMs()));
	profiler.End(Profiler::Stage::FrameUpdaters);
	// OPENBLACK_TEST_TEXT_SHOT (openblack only): the screenshot after the text
	if (_textShotAtMs.has_value() && static_cast<uint32_t>(SDL_GetTicks()) >= *_textShotAtMs)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "OPENBLACK_TEST_TEXT_SHOT: {}", _textShotPath);
		Locator::screenshotRequest::value().Request(_textShotPath);
		_textShotAtMs = UINT32_MAX; // once
	}
	// A hand demo (Input/HandDemo, played by the interface): the recorded mouse (rounded), buttons and camera.
	// SetPositionAndFocus sets the camera's zoomers (here the script camera's) and the drawn camera; the camera's own
	// update is skipped while it plays, so the record wins. The original runs it in the turn (with the game inputs),
	// before the frame's graphics (and the help system's Draw3D): its camera is set before the spirits are laid out and
	// drawn with it. Both passes; (approximate) here once a frame before Draw3D, where the original's frame pass (the
	// frame updates) reaches the screen a frame later
	{
		const auto screenSize =
		    Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
		if (screenSize.x > 0 && screenSize.y > 0)
		{
			if (const auto demo = hand_demo::Update(game_clock::VisualMs()); demo)
			{
				input::GameCursorRef() = glm::ivec2(glm::vec2(screenSize) * demo->mouse + 0.5f);
				input::HandButtonsRef() = {.gripping = demo->grip, .action = demo->action};
				if (demo->eye && demo->focus)
				{
					script_camera::SetPositionAndFocus(*demo->eye, *demo->focus);
					camera.SetOrigin(*demo->eye);
					camera.SetFocus(*demo->focus);
				}
			}
		}
	}
	// the help system's Draw3D: the texts' slide-in and the click cue's fade, with the frame's game ms, or the real
	// delta time in the citadel
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		const bool citadel = game_clock::IsInsideCitadel();
		const auto resolution = Locator::rendererInterface::value().GetResolution();
		const int bars = ScreenFade::LetterboxHeight(resolution.x, resolution.y, TheScreenFade().GetWideScreenFraction());
		helpSystem->Draw3D(static_cast<float>(citadel ? game_clock::FrameRealMs() : game_clock::FrameGameMs()),
		                   help::ComputeTextRegion(resolution.x, resolution.y, bars), resolution.x);
	}
	// in Draw3D: the advisors' control (the real delta time x 0.001, 0.4), their flight, anims and pose; drawn by
	// Renderer::DrawSpirits. (approximate) audio::advisor::Update (the sentence update, which the original runs first,
	// and the lip sync) comes later in this frame, so the mouth and the tags read the previous frame's LipSyncThisFrame
	if (auto* spirits = help::spirits::Get(); spirits != nullptr)
	{
		spirits->Update();
	}
	// the key / mouse icons' update twice a frame with the same real delta time x 0.001:
	// #1 at the end of Draw3D (after the advisors' control and their draw),
	// #2 at the end of the help text's end-of-frame draw. Each icon keeps its alpha per pass (a1, a2) for
	// Renderer::DrawInputPrompts
	{
		const float inputPromptSeconds = static_cast<float>(game_clock::FrameRealMs()) * 0.001f;
		help::input_prompt::Frame(inputPromptSeconds, 0);
		help::input_prompt::Frame(inputPromptSeconds, 1);
	}

	// Update Uniforms
	{
		auto profilerScopedUpdateUniforms = profiler.BeginScoped(Profiler::Stage::UpdateUniforms);

		// Update Hand and intersection point
		ecs::components::Transform intersectionTransform {};
		{
			const auto screenSize =
			    Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
			const auto scale = glm::vec3(50.0f, 50.0f, 50.0f);
			if (screenSize.x > 0 && screenSize.y > 0)
			{
				// (the hand demo's mouse, buttons and camera are set above, before the help system's Draw3D)
				// OPENBLACK_FIXED_MOUSE (test runs): the hand's ray from the fixed position (no SDL_MOUSEMOTION reaches
				// ProcessEvents to set it)
				auto& cursor = input::GameCursorRef();
				if (input::FixedMouse().has_value())
				{
					cursor = *input::FixedMouse();
				}
				// Test hook: fixed cursor at a fraction of the window ("0.5,0.6"), for screenshots without the real mouse
				if (const char* at = input::MouseAt(); at != nullptr)
				{
					glm::vec2 fraction(0.5f);
					if (std::sscanf(at, "%f,%f", &fraction.x, &fraction.y) == 2)
					{
						cursor = glm::ivec2(glm::vec2(screenSize) * fraction);
					}
				}
				auto rayCast = profiler.BeginScoped(Profiler::Stage::HandRayCast);
				glm::vec3 rayOrigin;
				glm::vec3 rayDirection;
				camera.DeprojectScreenToWorld(static_cast<glm::vec2>(cursor) / static_cast<glm::vec2>(screenSize), rayOrigin,
				                              rayDirection);
				auto& dynamicsSystem = Locator::dynamicsSystem::value();

				if (!glm::any(glm::isnan(rayOrigin) || glm::isnan(rayDirection)))
				{
					if (auto hit = dynamicsSystem.RayCastClosestHit(rayOrigin, rayDirection, 1e10f))
					{
						intersectionTransform = hit->first;
					}
					else // For the water
					{
						float intersectDistance = 0.0f;
						const auto planeOrigin = glm::vec3(0.0f, 0.0f, 0.0f);
						const auto planeNormal = glm::vec3(0.0f, 1.0f, 0.0f);
						if (glm::intersectRayPlane(rayOrigin, rayDirection, planeOrigin, planeNormal, intersectDistance))
						{
							intersectionTransform.position = rayOrigin + rayDirection * intersectDistance;
							intersectionTransform.rotation = glm::mat3(1.0f);
						}
					}
					// ObtainRequiredHandPosition: the hand goes along the mouse ray to the surface under the cursor
					// (an object's mesh or the land), smoothed by the hand distance zoomer.
					{
						const bool land = intersectionTransform.position != glm::zero<glm::vec3>();
						const auto point = Locator::handSystem::value().ResolveCursorPoint(
						    rayOrigin, rayDirection, land ? std::optional(intersectionTransform.position) : std::nullopt,
						    input::HandButtonsRef().gripping, deltaTime);
						intersectionTransform.position = point.value_or(glm::zero<glm::vec3>());
					}
				}
				intersectionTransform.scale = scale;
			}

			// Hand animation (hh.HBN): Cwiggle / Cgrip + L*_lr / L*_fb layers driven by the cursor motion.
			{
				auto handUpdate = profiler.BeginScoped(Profiler::Stage::HandUpdate);
				const auto mouseDelta = glm::vec2(input::MouseMotion(_mouseMotion, input::GameCursorRef()));
				Locator::handSystem::value().Update(deltaTime, mouseDelta, input::HandButtonsRef().gripping,
				                                    input::HandButtonsRef().action);
			}
			// The miracles' per-frame part (the one-shot orbs' texture), in game time
			{
				auto magicFrame = profiler.BeginScoped(Profiler::Stage::MagicFrame);
				magic::Update(game_clock::FrameGameSeconds());
			}

			// Palm towards the ground, index fingertip on the point under the cursor, fingertips dug in while gripping.
			const bool overLand = intersectionTransform.position != glm::zero<glm::vec3>();
			auto handPlace = profiler.BeginScoped(Profiler::Stage::HandPlace);
			Locator::handSystem::value().Place(overLand ? std::optional(intersectionTransform.position) : std::nullopt,
			                                   camera.GetForward(), input::HandButtonsRef().gripping, deltaTime);

			// after the landscape and the hand are placed, the hand's point (written by the landscape's draw) is tested
			// against the influence circles unless the game is paused: crossing a player's influence circle rings
			// G_HandThroughInfluence_01.
			if (!game_clock::IsPaused())
			{
				if (const auto& hands = Locator::handSystem::value().GetPlayerHandPositions(); hands[0].has_value())
				{
					influence::ProcessHandCrossing(*hands[0]);
				}
			}
			// every ripple the crossings made is updated (life -= the frame's game ms, which is 0 while paused).
			// (inferred) outside the pause test, traced only up to the crossing call
			influence::UpdateRipples(game_clock::FrameGameMs());
		}
		// the leashes' ropes swing from where the hand now is, in the citadel too (only their drawing stops there)
		ecs::creature_loop::UpdateLeash(game_clock::FrameGameSeconds(), profiler);

		// Update Entities
		{
			auto updateEntities = profiler.BeginScoped(Profiler::Stage::UpdateEntities);
			// Not while a full screen film hides the world: the scene is not drawn (the same test as Renderer::PreDraw
			// and DrawScene, and the film only starts or ends before this point of the frame), and only the draw reads
			// what PrepareDraw writes. Its dirty flags stay set, so the first frame after the film writes the instances
			// from the world as it is then
			const bool worldHidden = video::Get().CoversScreen() || video::GetFallingSpell().HidesWorld();
			if (ecs::systems::ShouldPrepareDraw(config.drawEntities, worldHidden))
			{
				Locator::rendereringSystem::value().PrepareDraw(config.drawBoundingBoxes, config.drawFootpaths,
				                                                config.drawStreams);
			}
		}
	} // Update Uniforms

	// Update Audio
	{
		auto updateAudio = profiler.BeginScoped(Profiler::Stage::UpdateAudio);
		// the sample main volume of the configuration, live (the options dialog's slider)
		audio::UpdateFrame();
		audio::music::Update();
		// the advisors' control loop, once a frame: their delayed sentences (by the tick count) and their lip-sync
		// (with the frame's seconds)
		audio::advisor::Update(std::chrono::duration<float>(deltaTime).count());
	} // Update Audio

	return config.numFramesToSimulate == 0 || Locator::screenshotRequest::value().Frame() < config.numFramesToSimulate;
}

bool Game::Initialize() noexcept
{
	auto& config = Locator::config::value();

	if (config.graphicsBackend != GraphicsBackend::Noop)
	{
		uint32_t extraFlags = 0;
		if (config.graphicsBackend == GraphicsBackend::Metal)
		{
			extraFlags |= SDL_WINDOW_METAL;
		}
		// Dev tooling, not original: OPENBLACK_WINDOW_TAG names the window so several running copies can be told apart
		// ("openblack [tag]").
		std::string windowTitle {k_WindowTitle};
		if (const char* tag = std::getenv("OPENBLACK_WINDOW_TAG"); tag != nullptr && *tag != '\0')
		{
			windowTitle += std::string(" [") + tag + "]";
		}
		openblack::InitializeWindow(windowTitle, config.resolution.x, config.resolution.y, config.displayMode, extraFlags);
	}

	using filesystem::Path;
	if (!InitializeEngine(config.graphicsBackend, config.vsync))
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Failed to initialize engine services.");
		return false;
	}
	auto& fileSystem = Locator::filesystem::value();
	auto& events = Locator::events::value();

	events.AddHandler(std::function([this, &config](const SDL_Event& event) {
		// If gui captures this input, do not propagate
		if (!Locator::debugGui::value().ProcessEvents(event))
		{
			// Inside the temple, Escape goes back to its main room and out, as the temple's keys do
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && event.key.repeat == 0 &&
			    Locator::temple::has_value() && Locator::temple::value().Active())
			{
				Locator::temple::value().Escape();
				return;
			}
			// The Creature Cave shown on its own, without a temple, closes on Escape
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && event.key.repeat == 0 &&
			    Locator::creatureCaveSystem::has_value() && Locator::creatureCaveSystem::value().Escape())
			{
				return;
			}
			// the escape menu takes Escape, and the keyboard and mouse while a box is open. Escape opens no menu with
			// a modifier held (Shift or Ctrl), over a full screen film (it skips the film instead) or while a script
			// holds the wide screen. (pending) a help task's dialogue stopped first, and the 300 ms debounce.
			// Alt is not a modifier here: Alt + Escape opens the menu
			// gui::EscapeBlocked: the original's Escape gate (Gui/GameMenu.h)
			const bool escapeBlocked =
			    event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE &&
			    gui::EscapeBlocked({.shiftOrCtrl = (event.key.keysym.mod & (KMOD_SHIFT | KMOD_CTRL)) != 0,
			                        .filmPlaying = video::IsPlaying(),
			                        .scriptWideScreen = help::Get() != nullptr && help::Get()->IsScriptWideScreen(),
			                        .menuOpen = _interface && _interface->GetMenu().IsOpen(),
			                        .msSinceMenuClosed = static_cast<int32_t>(SDL_GetTicks() - _menuClosedTicks)});
			if (_interface && !escapeBlocked && Locator::windowing::has_value() &&
			    _interface->ProcessEvent(event, static_cast<glm::u16vec2>(Locator::windowing::value().GetSize())))
			{
				HandleInterfaceAction();
				return;
			}
			config.running = this->ProcessEvents(event);
			Locator::gameActionSystem::value().ProcessEvent(event);
		}
	}));

	if (!fileSystem.IsPathValid(_gamePath))
	{
		// no key, don't guess, let the user know to set the command param
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game Path missing",
		                         "Game path was not supplied, use the -g "
		                         "command parameter to set it.",
		                         nullptr);
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to find the GameDir.");
		return false;
	}

	fileSystem.SetGamePath(_gamePath);

	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "The GamePath is \"{}\".", fileSystem.GetGamePath().generic_string());

	if (std::filesystem::path(_startMap).is_absolute())
	{
		if (std::find(_startMap.begin(), _startMap.end(), "Scripts") != _startMap.end())
		{
			auto p = _startMap;
			while (p.filename() != "Scripts" && p != p.parent_path())
			{
				p = p.parent_path();
			}
			fileSystem.AddAdditionalPath(p.parent_path());
		}
		else
		{
			fileSystem.AddAdditionalPath(_startMap.parent_path());
		}
	}
	else
	{
		_startMap = fileSystem.GetPath<Path::Scripts>() / _startMap;
	}

	if (!InitializeGame())
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Failed to initialize game services.");
		return false;
	}

	auto& resources = Locator::resources::value();
	auto& meshManager = resources.GetMeshes();
	auto& textureManager = resources.GetTextures();
	auto& animationManager = resources.GetAnimations();
	auto& levelManager = resources.GetLevels();
	auto& glowManager = resources.GetGlows();
	auto& cameraPathManager = resources.GetCameraPaths();

	fileSystem.Iterate(
	    fileSystem.GetPath<Path::Citadel>() / "OutsideMeshes", false, [&meshManager](const std::filesystem::path& f) {
		    if (f.extension() == ".zzz")
		    {
			    SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading temple mesh: {}", f.stem().string());
			    try
			    {
				    meshManager.Load(fmt::format("temple/{}", f.stem().string()), resources::L3DLoader::FromDiskTag {}, f);
			    }
			    catch (std::runtime_error& err)
			    {
				    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			    }
		    }
	    });

	fileSystem.Iterate( //
	    fileSystem.GetPath<filesystem::Path::Citadel>() / "engine", false,
	    [&meshManager, &glowManager, &cameraPathManager](const std::filesystem::path& f) {
		    if (f.extension() == ".zzz")
		    {
			    if (f.stem().string().ends_with("lo_l3d"))
			    {
				    SPDLOG_LOGGER_WARN(
				        spdlog::get("game"),
				        "Skipping lo duplicate lo meshes. See https://github.com/openblack/openblack/issues/727");
				    return;
			    }
			    SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading interior temple mesh: {}", f.stem().string());
			    try
			    {
				    meshManager.Load(fmt::format("temple/interior/{}", f.stem().string()), resources::L3DLoader::FromDiskTag {},
				                     f);
			    }
			    catch (std::runtime_error& err)
			    {
				    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			    }
		    }
		    else if (f.extension() == ".glw")
		    {
			    SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading interior temple glows: {}", f.stem().string());
			    try
			    {
				    glowManager.Load(fmt::format("temple/interior/glow/{}", f.stem().string()),
				                     resources::LightLoader::FromDiskTag {}, f);
			    }
			    catch (std::runtime_error& err)
			    {
				    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			    }
		    }
		    else if (f.extension() == ".cam")
		    {
			    SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading interior temple cam: {}", f.stem().string());
			    try
			    {
				    cameraPathManager.Load(fmt::format("temple/{}", f.stem().string()),
				                           resources::CameraPathLoader::FromDiskTag {}, f);
			    }
			    catch (std::runtime_error& err)
			    {
				    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			    }
		    }
	    });
	// The main room's markers of the temples, creatures and challenges on its map
	for (const auto* icon : {"I_citadel_on_map", "I_creature_on_map", "I_challenge_on_map"})
	{
		try
		{
			meshManager.Load(fmt::format("temple/icons/{}", icon), resources::L3DLoader::FromDiskTag {},
			                 fileSystem.GetPath<Path::Citadel>() / "icons" / fmt::format("{}.l3d", icon));
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	}

	pack::PackFile pack;

	// read once into the byte cache: the exploding objects parse the same copy later
	auto packResult = pack::PackResult::ErrCantOpen;
	try
	{
		const auto& packBytes = resources::LoadBlob(Locator::resources::value().GetBlobs(),
		                                            fileSystem.FindPath(fileSystem.GetPath<Path::Data>() / "AllMeshes.g3d"));
		packResult = pack.ReadFile(*resources::BlobStream(packBytes));
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "AllMeshes.g3d: {}", e.what());
	}
	if (packResult != pack::PackResult::Success)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Unable to load AllMeshes.g3d: {}", pack::ResultToStr(packResult));
		return false;
	}

	const auto& meshes = pack.GetMeshes();
	// TODO (#749) use std::views::enumerate
	for (size_t i = 0; const auto& mesh : meshes)
	{
		const auto meshId = static_cast<MeshId>(i);
		// a pack may have more meshes than openblack has names for
		const auto name = i < k_MeshNames.size() ? k_MeshNames[i] : fmt::format("Mesh{}", i);
		meshManager.Load(meshId, resources::L3DLoader::FromBufferTag {}, name, mesh);
		++i;
	}

	const auto& textures = pack.GetTextures();
	for (auto const& [name, g3dTexture] : textures)
	{
		textureManager.Load(g3dTexture.header.id, resources::Texture2DLoader::FromPackTag {}, name, g3dTexture);
	}

	pack::PackFile animationPack;
	packResult = animationPack.ReadFile(*fileSystem.GetData(fileSystem.GetPath<Path::Data>() / "AllAnims.anm"));
	if (packResult != pack::PackResult::Success)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Unable to load AllAnims.anm: {}", pack::ResultToStr(packResult));
		return false;
	}

	const auto& animations = animationPack.GetAnimations();
	// TODO (#749) use std::views::enumerate
	for (size_t i = 0; i < animations.size(); i++)
	{
		animationManager.Load(i, resources::L3DAnimLoader::FromBufferTag {}, animations[i]);
	}

	fileSystem.Iterate(fileSystem.GetPath<Path::CreatureMesh>(), false, [&meshManager](const std::filesystem::path& f) {
		const auto& fileName = f.stem().string();
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading creature mesh: {}", fileName);
		try
		{
			if (string_utils::BeginsWith(fileName, "Hand"))
			{
				return;
			}

			const auto meshId = creature::GetIdFromMeshName(fileName);
			if (!meshId.has_value())
			{
				// A file whose name names no appearance is not a species' body; loading it would take the id of the
				// real mesh of that species
				return;
			}
			meshManager.Load(*meshId, resources::L3DLoader::FromDiskTag {}, f);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});

	// Load loose one-off assets
	{
		// the ids of the loose assets (the coffre's animation and mesh share one id, in different caches)
		constexpr auto k_CoffreId = entt::hashed_string("coffre");
		constexpr auto k_ConeMesh = entt::hashed_string("cone");
		constexpr auto k_MarkerMesh = entt::hashed_string("marker");
		constexpr auto k_RiverMesh = entt::hashed_string("river");
		constexpr auto k_River2Mesh = entt::hashed_string("river2");
		constexpr auto k_MetreSphereMesh = entt::hashed_string("metre_sphere");

		using AFromDiskTag = resources::L3DAnimLoader::FromDiskTag;
		animationManager.Load(k_CoffreId.value(), AFromDiskTag {}, fileSystem.GetPath<Path::Misc>() / "coffre.anm");

		using LFromDiskTag = resources::L3DLoader::FromDiskTag;
		meshManager.Load(ecs::components::Hand::k_MeshId, LFromDiskTag {},
		                 fileSystem.GetPath<Path::CreatureMesh>() / "Hand_Boned_Base2.l3d");
		// What moves each species' body, and the meshes whose skins are painted
		resources::LoadCreatureRigs(Locator::resources::value());
		// The eyes every creature is drawn with
		for (const auto& [id, file] : {std::pair {ecs::components::CreatureEyes::k_EyeballMeshId, "Eyeball.l3d"},
		                               std::pair {ecs::components::CreatureEyes::k_EyelidMeshId, "Eyelid.l3d"}})
		{
			if (const auto path = fileSystem.GetPath<Path::Data>() / file; fileSystem.Exists(path))
			{
				meshManager.Load(id, LFromDiskTag {}, path);
			}
		}
		meshManager.Load(k_CoffreId.value(), LFromDiskTag {}, fileSystem.GetPath<Path::Misc>() / "coffre.l3d");
		meshManager.Load(k_ConeMesh.value(), LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "cone.l3d");
		meshManager.Load(k_MarkerMesh.value(), LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "marker.l3d");
		meshManager.Load(k_RiverMesh.value(), LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "river.l3d");
		meshManager.Load(k_River2Mesh.value(), LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "river2.l3d");
		meshManager.Load(k_MetreSphereMesh.value(), LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "metre_sphere.l3d");
		// the in-game menus, greeting the player by OPENBLACK_PLAYER_NAME or the login name (openblack has no profiles)
		{
			const char* user = std::getenv("OPENBLACK_PLAYER_NAME");
			user = user != nullptr ? user : std::getenv("USERNAME");
			// the options box's controls: the sliders at the main volumes / 127; the detail at the running level
			const auto& config = Locator::config::value();
			gui::MenuSettings settings {
			    .sfxVolume = static_cast<float>(config.audioSampleMainVolume) / 127.0f,
			    .musicVolume = static_cast<float>(config.audioMusicMainVolume) / 127.0f,
			    .detail = config.detailLevel,
			};
			// without a profile value, the help system's read speed
			if (const auto* helpSystem = help::Get(); helpSystem != nullptr)
			{
				settings.helpTextSpeed = helpSystem->GetReadSpeed();
			}
			_interface = gui::GameInterface::Create(gui::ToUtf16(user != nullptr ? user : "Player"), settings);
			if (!_interface)
			{
				SPDLOG_LOGGER_WARN(spdlog::get("game"), "The in-game menus are not available, Escape quits");
			}
			// the temple writes its scrolls and signs with the interface's text and font, or none without it
			if (Locator::temple::has_value())
			{
				Locator::temple::value().SetInterface(_interface.get());
			}
			// and the Creature Cave's screen with its text
			if (Locator::creatureCaveSystem::has_value())
			{
				Locator::creatureCaveSystem::value().SetInterface(_interface.get());
			}
		}
		// the dispensers' bubble, made with the one-shot orb: .\data\spells\meshes\O_Bibble_up.l3d (not in the
		// test data)
		try
		{
			meshManager.Load(ecs::archetypes::OneOffSpellSeedArchetype::k_MeshName, LFromDiskTag {},
			                 fileSystem.GetPath<Path::Data>() / "Spells" / "Meshes" / "O_bibble_up.l3d");
			// the shared mesh with MaterialProperties {1, 1, 0, 1, 1}: the change flag rewrites every primitive:
			// the cap's AlphaTextured (4) -> 6 -> additive 13 -> with Z write 12 (SRCALPHA / ONE, alpha = texture x
			// diffuse), and the double-sided bit cleared. The object alpha's mode table keeps mode 12, so the bubble
			// ADDS its texture x 0x95 to what is behind it: the bright, pearly bubble of the original
			meshManager.Handle(resources::HashIdentifier(ecs::archetypes::OneOffSpellSeedArchetype::k_MeshName))
			    ->SetMaterialProperties(
			        {.additive = true, .zWrite = true, .doubleSided = false, .change = true, .alpha = true});
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	}

	// TODO(raffclar): #400: Parse level files within the resource loader
	// TODO(raffclar): #405: Determine campaign levels from the challenge script file
	// Load the campaign levels
	fileSystem.Iterate(fileSystem.GetPath<Path::Scripts>(), false, [&levelManager](const std::filesystem::path& f) {
		const auto& name = f.stem().string();
		if (f.extension() != ".txt" || name.rfind("InfoScript", 0) != std::string::npos)
		{
			return;
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading campaign level: {}", f.stem().string());
		try
		{
			if (Level::IsLevelFile(f))
			{
				levelManager.Load(fmt::format("campaign/{}", name), resources::LevelLoader::FromDiskTag {}, f,
				                  Level::LandType::Campaign);
			}
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});
	// Load Playgrounds
	// Attempt to load additional levels as playgrounds
	fileSystem.Iterate(fileSystem.GetPath<Path::Playgrounds>(), false, [&levelManager](const std::filesystem::path& f) {
		if (f.extension() != ".txt")
		{
			return;
		}
		const auto& name = f.stem().string();
		if (levelManager.Contains(fmt::format("playgrounds/{}", name)))
		{
			// Already added
			return;
		}

		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading custom level: {}", f.stem().string());
		try
		{
			if (Level::IsLevelFile(f))
			{
				levelManager.Load(fmt::format("playgrounds/{}", name), resources::LevelLoader::FromDiskTag {}, f,
				                  Level::LandType::Skirmish);
			}
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});

	// the audio's start: the sample main volume and the channels' queries, then its banks (every .sad of the Audio
	// directory, audio::banks)
	audio::Init(MakeMusicQueries(*this));

	// The voices of the help texts, rebuilt from the wave names of villagers, HelpSprites and Guidance
	audio::voices::BuildTable();

	// the music player on the audio device (started with the audio)
	audio::music::Start();

	{
		InfoFile infoFile;
		auto result = infoFile.LoadFromFile(Locator::filesystem::value().GetPath<filesystem::Path::Scripts>() / "info.dat");
		if (!result)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to load game info data.");
			return false;
		}
		Locator::infoConstants::reset(result.release());
	}

	// the game music (its banks), with the town distances of info.dat
	{
		const auto& sound = Locator::infoConstants::value().sound;
		audio::game_music::Start(MakeMusicQueries(*this), {sound.townTriggerDistance, sound.townTriggerOffDistance});
	}

	// the guidance's HELP_SPRITES_GUIDANCE lists (info.dat GHelpSpritesGuidance)
	{
		std::array<audio::guidance::SpriteList, audio::guidance::k_SpriteLists> lists {};
		const auto& guidance = Locator::infoConstants::value().helpSpritesGuidance;
		static_assert(sizeof(guidance[0]) == sizeof(lists[0]), "GHelpSpritesGuidance is 34 HELP_TEXT ids");
		for (size_t k = 0; k < lists.size() && k < guidance.size(); ++k)
		{
			std::memcpy(lists.at(k).data(), &guidance.at(k), sizeof(lists[0]));
		}
		audio::guidance::SetSpriteLists(lists);
	}
	// the spooky voices, at the game's one-time start (the help texts and the voice table are ready)
	audio::spooky::Init();

	// when the help system is made: the advisors' control with the HelpSprites bank for both advisors; the models
	// MarkGood.Hd / MarkEvil.Hd are help::spirits::Start below
	audio::advisor::Init(audio::Bank(audio::SfxBank::HelpSprites));

	// the help system with HelpSystemInfo of info.dat
	{
		const auto& helpInfo = Locator::infoConstants::value().helpSystem;
		help::HelpSystem::Queries queries;
		// the game turn
		queries.turn = []() { return game_clock::Turn(); };
		// the engine timer's ms
		queries.nowMs = []() { return game_clock::EngineMs(); };
		// inside the citadel and paused
		queries.citadelClock = []() { return game_clock::IsInsideCitadel() && game_clock::IsPaused(); };
		// the task's script type
		queries.taskScriptType = [](uint32_t task) -> uint32_t {
			return Locator::vm::has_value() ? static_cast<uint32_t>(Locator::vm::value().GetTaskScriptType(task)) : 1;
		};
		// the voice bank is loaded
		queries.voiceBankLoaded = [](audio::SfxBank bank) { return audio::voices::BankRegistered(bank); };
		// an advisor is talking
		queries.advisorsTalking = []() { return audio::advisor::AnyTalking(); };
		// the owner's sample of the bank is playing
		queries.isPlaying = [](audio::SfxBank bank, audio::VoiceOwner owner, uint32_t sample) {
			return audio::IsPlaying(audio::Owner::Key(static_cast<uint32_t>(owner)), static_cast<int>(sample), bank);
		};
		// the screen height the help text is made with (its line height is H / 30)
		queries.screenHeight = []() { return Locator::windowing::has_value() ? Locator::windowing::value().GetSize().y : 480; };
		// the interface plays a hand demo (no click on a text during a hand demo)
		queries.playBack = []() { return hand_demo::IsPlaying(0); };
		help::HelpSystem::Hooks hooks;
		// the text's voice
		hooks.sayVoice = [](uint32_t textId, help::VoiceRoute /*route*/, audio::TextVoice voice) {
			audio::voices::RunTextVoice(helptext::GetEntry(textId).narrator, voice);
		};
		// OPENBLACK_TEST_TEXT_SHOT="textId,path[,ms]" (openblack only): a screenshot `ms` ms (1000 by default, wall clock)
		// after that text is shown (Game::Loop takes it, see _textShotAtMs)
		if (const char* shot = std::getenv("OPENBLACK_TEST_TEXT_SHOT"); shot != nullptr)
		{
			const std::string spec(shot);
			const auto comma = spec.find(',');
			const auto comma2 = comma == std::string::npos ? std::string::npos : spec.find(',', comma + 1);
			if (comma != std::string::npos)
			{
				const auto id = static_cast<uint32_t>(std::strtoul(spec.substr(0, comma).c_str(), nullptr, 10));
				_textShotPath = spec.substr(comma + 1, comma2 == std::string::npos ? std::string::npos : comma2 - comma - 1);
				const uint32_t delay = comma2 == std::string::npos
				                           ? 1000u
				                           : static_cast<uint32_t>(std::strtoul(spec.c_str() + comma2 + 1, nullptr, 10));
				hooks.textStarted = [this, id, delay](uint32_t textId) {
					if (textId == id && !_textShotAtMs.has_value())
					{
						_textShotAtMs = static_cast<uint32_t>(SDL_GetTicks()) + delay;
					}
				};
			}
		}
		// a click cuts the villagers' narration with the 20 ms ramp
		hooks.stopVoicesOnClick = []() { audio::voices::CutByClick(); };
		// a spirit's stop goes to the advisors' control with dude = (the spirit's type != 1) (inferred: the good spirit
		// has type 1 and dude 0)
		// the spirit's way home (Help/Spirits.h SpiritHome)
		hooks.spiritHome = [](int32_t spirit, int32_t arg) {
			if (auto* spirits = help::spirits::Get(); spirits != nullptr)
			{
				spirits->Control().SpiritHome(spirit, arg != 0);
			}
		};
		hooks.spiritStop = [](int32_t spirit, int32_t arg) {
			audio::advisor::Interrupt(spirit == 1 ? audio::advisor::k_GoodSpirit : audio::advisor::k_EvilSpirit, arg);
		};
		// the help system's wide screen: the bars slide in HelpSystemInfo.wideScreenTime seconds from where they are;
		// the interface is active unless on with an owner (the owner kept is on ? owner : 0); hiding the dialogue
		// boxes is not ported, and the owning task while on is what the sound effects read to skip the user-param-1
		// samples
		hooks.wideScreen = [time = helpInfo.wideScreenTime](bool on) {
			TheScreenFade().SetWideScreen(on, time);
			const auto* helpSystem = help::Get();
			interface_active::SetActive(!(on && helpSystem != nullptr && helpSystem->GetWideScreenOwner() != 0));
			audio::SetScriptWideScreen(helpSystem != nullptr && helpSystem->IsScriptWideScreen());
		};
		help::Start({helpInfo.readDefaultAdjustGTTime, helpInfo.readDefaultWordGTTime}, std::move(queries), std::move(hooks));
	}
	// the advisors' control and its two .hd models (Help/SpiritsRuntime.h)
	help::spirits::Start();

	fileSystem.Iterate(fileSystem.GetPath<Path::Textures>(), false, [&textureManager](const std::filesystem::path& f) {
		if (string_utils::LowerCase(f.extension().string()) == ".raw")
		{
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading raw texture: {}", f.stem().string());
			try
			{
				textureManager.Load(fmt::format("raw/{}", f.stem().string()), resources::Texture2DLoader::FromDiskTag {}, f);
			}
			catch (std::runtime_error& err)
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			}
		}
	});

	// The texture every creature's hair is drawn with, and its alpha beside it
	for (const auto& [id, name] : {std::pair {ecs::components::CreatureHair::k_TextureId, "C_Ape_Hair.raw"},
	                               std::pair {ecs::components::CreatureHair::k_AlphaTextureId, "C_Ape_Haira.raw"}})
	{
		const auto path = fileSystem.GetPath<Path::Data>() / name;
		if (!fileSystem.Exists(path))
		{
			continue;
		}
		try
		{
			textureManager.Load(id, resources::Texture2DLoader::FromDiskTag {}, path);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	}

	// What creatures' tattoos and marks are painted with. The symbols the game ships with fill the cells no player's
	// symbol has been written into.
	try
	{
		const auto dataDirectory = fileSystem.GetPath<Path::Data>();
		const auto texturesDirectory = fileSystem.GetPath<Path::Textures>();
		Locator::resources::value().GetCreatureSkinArt().Load(
		    creature_skin::k_ArtId, resources::CreatureSkinArtLoader::FromDiskTag {},
		    resources::CreatureSkinArtLoader::Paths {
		        .symbols = texturesDirectory / "PlayersSymbols.raw",
		        .defaultSymbols = texturesDirectory / "OriginalChooseSymbol.raw",
		        .freshDamage = dataDirectory / "damage_new256.raw",
		        .freshDamageAlpha = dataDirectory / "damage_new256A.raw",
		        .oldDamage = dataDirectory / "damage_old256.raw",
		        .oldDamageAlpha = dataDirectory / "damage_old256A.raw",
		        .palette = dataDirectory / "tattoocols.raw",
		    });
	}
	catch (const std::exception& err)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Creature skin art: {}", err.what());
	}

	return true;
}

bool Game::Run() noexcept
{
	auto& config = Locator::config::value();

	if (!LoadMap(_startMap))
	{
		return false;
	}

	Locator::dynamicsSystem::value().RegisterRigidBodies();

	auto& fileSystem = Locator::filesystem::value();

	auto challengePath = fileSystem.GetPath<filesystem::Path::Quests>() / "challenge.chl";
	if (fileSystem.Exists(challengePath))
	{
		auto& chlapi = Locator::chlapi::value();
		auto& lhvm = Locator::vm::value();
		// the VM's object references: the original's script library calls the ADD_REFERENCE / REMOVE_REFERENCE
		// natives (which raise and lower the object's script reference count) for a popped object and the variable's
		// old one (POP) and for a stopped task's object locals; object 0 is the scripts' null
		lhvm.Initialise(
		    &chlapi.GetFunctionsTable(), nullptr, nullptr,
		    // the task-stop callback: the dialogue, the wide screen and the camera of the task go back
		    // (Help/ScriptControl.cpp)
		    [](uint32_t taskNumber) {
			    auto& cameraControl = help::script_control::GetCameraControl();
			    const auto cameraOwner = cameraControl.owner;
			    help::script_control::OnTaskStopped(taskNumber, help::Get(), cameraControl, audio::GetScriptAudioState());
			    if (cameraOwner != 0 && cameraControl.owner == 0)
			    {
				    script_camera::End(); // the camera part (Camera/ScriptCamera.h)
			    }
		    },
		    [](lhvm::ErrorCode code, const std::string v0, uint32_t v1) {
			    SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "VM error {} ({}, {})", static_cast<int>(code), v0, v1);
		    },
		    [](uint32_t objId) {
			    if (objId != 0)
			    {
				    ecs::script_held::IncrementReference(static_cast<entt::entity>(objId));
			    }
		    },
		    [](uint32_t objId) {
			    if (objId != 0)
			    {
				    ecs::script_held::DecrementReference(static_cast<entt::entity>(objId));
			    }
		    });
		try
		{
			lhvm.LoadBinary(resources::LoadBlob(Locator::resources::value().GetBlobs(), challengePath));
			lhvm.StartScript("LandControlAll", lhvm::ScriptType::All);
			// a new game, right after starting LandControlAll: the skip-tutorial question clears bits 23, 24 and 25 of
			// the game's flags, pauses the game and shows the SkipBox; its answer sets them (0 none, 1 bit 23, 2 bits
			// 23+24, 3 bits 23+24+25) and unpauses (Game::HandleInterfaceAction). SetupLand1 reads them later through
			// CAN_SKIP_TUTORIAL and the others (openblack guard: without a window nobody answers the box, and the pause
			// would stay)
			if (_interface && Locator::windowing::has_value())
			{
				_interface->ShowSkipBox();
				game_clock::Pause(true);
			}
			auto& skipFlags = Locator::mapScriptSystem::value().Globals().tutorialSkipFlags;
			skipFlags.canSkipTutorial = false;
			skipFlags.canSkipCreatureTraining = false;
			skipFlags.isKeepingOldCreature = false;
		}
		catch (const std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to read challenge file at {}: {}",
			                    (fileSystem.GetGamePath() / challengePath).generic_string(), err.what());
		}
	}
	else
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Challenge file not found at {}",
		                    (fileSystem.GetGamePath() / challengePath).generic_string());
		return false;
	}

	// Test hooks: OPENBLACK_TEST_FADE="r,g,b,seconds" runs SET_FADE, OPENBLACK_TEST_WIDESCREEN=1 SET_WIDESCREEN(1)
	if (const char* fade = std::getenv("OPENBLACK_TEST_FADE"); fade != nullptr)
	{
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
		float seconds = 0.0f;
		if (std::sscanf(fade, "%f,%f,%f,%f", &r, &g, &b, &seconds) == 4)
		{
			TheScreenFade().FadeTo(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), seconds);
		}
	}
	if (std::getenv("OPENBLACK_TEST_WIDESCREEN") != nullptr)
	{
		TheScreenFade().SetWideScreen(true, Locator::infoConstants::value().helpSystem.wideScreenTime);
		// as SET_WIDESCREEN: the help system's owning task is set too (the user-param-1 samples are skipped)
		audio::SetScriptWideScreen(true);
	}
	// OPENBLACK_TEST_VIDEO=<intro|fall|path> plays a full screen film (docs/bw1-notes/video.md): intro as the intro
	// sequence (data\intro.bik, 60 s), fall as the falling spell's kick-off without its creature test (mode 2,
	// data\Spells\fall\fall.bik with alpha 0x50 and no world drawn, Video/FallingSpellVideo.h), else the given .bik
	if (const char* film = std::getenv("OPENBLACK_TEST_VIDEO"); film != nullptr)
	{
		const std::string name = film;
		const auto& data = fileSystem.GetPath<filesystem::Path::Data>();
		const std::filesystem::path path = name == "intro"  ? data / "intro.bik"
		                                   : name == "fall" ? data / "Spells" / "fall" / "fall.bik"
		                                                    : std::filesystem::path(name);
		std::filesystem::path found = path;
		try
		{
			found = fileSystem.FindPath(path);
		}
		catch (const std::exception&)
		{
		}
		if (name == "fall")
		{
			video::GetFallingSpell().Start();
		}
		else
		{
			video::Get().Play(found);
			if (name == "intro")
			{
				video::Get().ScheduleIntro();
			}
		}
	}
	// OPENBLACK_TEST_MOVE_TIME="hour,seconds" runs MOVE_GAME_TIME; OPENBLACK_CLOCK_TRACE=1 logs the clock every 50 turns
	if (const char* move = std::getenv("OPENBLACK_TEST_MOVE_TIME"); move != nullptr)
	{
		float hour = 0.0f;
		float seconds = 0.0f;
		if (std::sscanf(move, "%f,%f", &hour, &seconds) == 2)
		{
			TheDayNightClock().MoveScriptTime(hour, seconds);
		}
	}

	// Initialize the Acceleration Structure
	Locator::entitiesMap::value().Rebuild();

	if (Locator::windowing::has_value())
	{
		const auto size = static_cast<glm::u16vec2>(Locator::windowing::value().GetSize());
		Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Main, size, 0x274659ff);
	}

	{
		uint16_t width;
		uint16_t height;
		Locator::oceanSystem::value().GetReflectionFramebuffer().GetSize(width, height);
		Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Reflection, {width, height}, 0x274659ff);
	}

	if (config.drawIsland)
	{
		uint16_t width;
		uint16_t height;
		Locator::terrainSystem::value().GetFootprintFramebuffer().GetSize(width, height);
		Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Footprint, {width, height}, 0x00000000);
	}

	Game::SetTime(config.timeOfDay);

	Locator::screenshotRequest::value().ResetFrame();
	// the ms since Run started come from the game's clock, so that a run with a fixed frame time draws the same
	const uint32_t runStartTicks = game_clock::TickCount();
	// one frame's logic -> draw hand-over (Engine/FrameSnapshot.h): one, refilled every frame (its vectors
	// keep their capacity, as the two locals it replaces did)
	engine::FrameSnapshot snapshot;
	while (Update())
	{
		LogicFrame(snapshot, game_clock::TickCount() - runStartTicks);
		DrawFrame(snapshot);
		LateLogic();
		FinishFrame();
	}

	return true;
}

void Game::LogicFrame(engine::FrameSnapshot& out, uint32_t timeMs)
{
	auto& profiler = Locator::profiler::value();
	const auto& config = Locator::config::value();
	// the PreDraw (the end of the frame's logic, nothing between it and the draw): the overlays read here, the
	// draw reads only the copy
	FillOverlayFrame(TheScreenFade(), out.overlay);
	// Inside the temple the tooltip's icon is drawn by where the cursor meets the room, not by the hand on the land
	if (_interface && Locator::temple::has_value() && Locator::temple::value().Active())
	{
		const auto& onScreen = _interface->GetHandOnScreen();
		out.overlay.inputPrompts.hand.reset();
		if (onScreen.has_value())
		{
			out.overlay.inputPrompts.hand = glm::vec2(std::trunc(onScreen->x), std::trunc(onScreen->y)); // y from the top
		}
	}
	ecs::super_villager::FillFrame(out.superVillagers);
	ecs::intro_special::FillFrame(out.overlay.introLight);

	profiler.Begin(Profiler::Stage::SceneDraw); // ends in DrawFrame, after DrawScene (as the scope did)
	// the draw reads a copy of the camera and the frame's clocks, taken here: nothing between this and the
	// draw moves them (PreDraw, which may read the live ones, gets the same values)
	Locator::camera::value().CopyViewTo(*_drawCamera);
	out.camera = _drawCamera.get();
	out.hand.position.reset();
	if (Locator::handSystem::has_value())
	{
		out.hand.position = glm::vec3(Locator::handSystem::value().GetHandMatrix()[3]);
	}
	out.time = timeMs; // TODO(#481): get actual time
	out.timeOfDay = Locator::skySystem::value().GetTime();
	out.clock = {.frameGameMs = game_clock::FrameGameMs(),
	             .turn = game_clock::Turn(),
	             .turnFraction = game_clock::TurnFraction(),
	             .paused = game_clock::IsPaused()};
	out.bumpMapStrength = config.bumpMapStrength;
	out.smallBumpMapStrength = config.smallBumpMapStrength;
	out.drawSky = config.drawSky;
	out.drawWater = config.drawWater;
	out.drawIsland = config.drawIsland;
	out.drawEntities = config.drawEntities;
	out.drawSprites = config.drawSprites;
	out.drawBoundingBoxes = config.drawBoundingBoxes;
	out.wireframe = config.wireframe;
	const auto& screenshots = Locator::screenshotRequest::value();
	out.frameCount = screenshots.Frame();
	out.screenshot = screenshots.Due();
	{
		auto preDraw = profiler.BeginScoped(Profiler::Stage::PreDraw);
		Locator::rendererInterface::value().PreDraw(out.Desc(Locator::entitiesRegistry::value()));
	}
}

void Game::DrawFrame(const engine::FrameSnapshot& in)
{
	auto& profiler = Locator::profiler::value();
	{
		const engine::gpu::ScopedPhase drawPhase(engine::gpu::Phase::Draw); // what the draw thread will run
		Locator::rendererInterface::value().DrawScene(in.Desc(Locator::entitiesRegistry::value()));
	}
	profiler.End(Profiler::Stage::SceneDraw); // begun in LogicFrame

	// the in-game menus over the scene. (pending) the real mouse and SDL_GetTicks read here: into the snapshot
	if (_interface && Locator::windowing::has_value())
	{
		glm::ivec2 mouse;
		SDL_GetMouseState(&mouse.x, &mouse.y);
		if (input::FixedMouse().has_value())
		{
			mouse = *input::FixedMouse();
		}
		else if (input::IgnoreRealInput())
		{
			// the game's cursor (OPENBLACK_MOUSE_AT or a hand demo), not the real one
			mouse = input::GameCursorRef();
		}
		const engine::gpu::ScopedPhase drawPhase(engine::gpu::Phase::Draw);
		_interface->Draw(static_cast<glm::u16vec2>(Locator::windowing::value().GetSize()), mouse, SDL_GetTicks());
	}

	{
		auto section = profiler.BeginScoped(Profiler::Stage::GuiDraw);
		const engine::gpu::ScopedPhase drawPhase(engine::gpu::Phase::Draw);
		// Skip drawing Debug UI for screenshots
		if (in.screenshot.has_value())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Requesting a screenshot at frame {}...", in.frameCount);
			Locator::rendererInterface::value().RequestScreenshot(*in.screenshot);
			// test hook OPENBLACK_SCREENSHOT_GUI=1: the debug UI (menus, windows) in the screenshot too
			if (std::getenv("OPENBLACK_SCREENSHOT_GUI") != nullptr)
			{
				Locator::debugGui::value().Draw();
			}
		}
		else
		{
			// (pending) ImGui is still drawn from the logic's state: the single-thread mode only, until its draw data
			// is copied
			Locator::debugGui::value().Draw();
		}
	}

	if (std::getenv("OPENBLACK_DRAW_STATS") != nullptr && (in.frameCount % 30 == 0 || in.frameCount < 8))
	{
		const auto* stats = bgfx::getStats();
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "DRAWSTATS frame {} draws {}", in.frameCount, stats->numDraw);
	}
}

void Game::LateLogic()
{
	// Test hook: "<frames>:<script>,<script>..." loads the next script every <frames> frames, at the point where the debug
	// menu's "Load Island" does (a check that changing maps doesn't crash). LoadMap is a sync point: the draw
	// thread parked
	if (static const char* cycle = std::getenv("OPENBLACK_TEST_MAP_CYCLE"); cycle != nullptr)
	{
		static const auto parsed = [](const std::string& text) {
			std::vector<std::string> scripts;
			const auto colon = text.find(':');
			const int frames = colon == std::string::npos ? 300 : std::max(1, std::atoi(text.substr(0, colon).c_str()));
			std::stringstream list(colon == std::string::npos ? text : text.substr(colon + 1));
			for (std::string script; std::getline(list, script, ',');)
			{
				scripts.push_back(script);
			}
			return std::make_pair(static_cast<uint32_t>(frames), scripts);
		}(cycle);
		const auto& [frames, scripts] = parsed;
		const uint32_t frame = Locator::screenshotRequest::value().Frame();
		if (frame > 0 && frame % frames == 0 && frame / frames <= scripts.size())
		{
			const auto& script = scripts[frame / frames - 1];
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Map cycle: loading {}", script);
			LoadMap(Locator::filesystem::value().GetPath<filesystem::Path::Scripts>() / script);
		}
	}
}

void Game::FinishFrame()
{
	auto& profiler = Locator::profiler::value();
	{
		auto section = profiler.BeginScoped(Profiler::Stage::RendererFrame);
		const engine::gpu::ScopedPhase drawPhase(engine::gpu::Phase::Draw);
		Locator::rendererInterface::value().Frame();
	}

	// Clear the stale screenshot request, then count the frame
	Locator::screenshotRequest::value().FinishFrame();
}

bool Game::LoadMap(const std::filesystem::path& path) noexcept
{
	const engine::gpu::ScopedPhase loadPhase(engine::gpu::Phase::Load); // a sync point with the draw thread
	auto& fileSystem = Locator::filesystem::value();

	if (!fileSystem.Exists(path))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Could not find script {}", path.generic_string());
		return false;
	}

	psys::manager::Clear();
	magic::OnLoadMap();
	// (inferred) a new land: no packet of the last one waits (their objects are gone), and the interface status's
	// synced hand and turn motion start again
	game_packets::Reset();
	if (Locator::handSystem::has_value())
	{
		Locator::handSystem::value().ResetTurnState();
	}
	// every land balance value back to 1 before the script
	land_balance::Reset();
	// the map's clear: the object creation counter back to 0 (2 on the first land: two HelpSpirits)
	ecs::object_index::OnLoadMap();
	// the map's clear: the script highlights (after the bookmarks)
	ecs::script_highlight::OnClearMap();
	// the game's start puts both random seeds at their start value once, before the first land of a new campaign (start
	// mode 1: the map script loaded, no map clear); the map's clear puts them at 0 for every later LOAD_MAP (a
	// playground game start) and for the skirmish playground (start mode 4).
	// (inferred) openblack has no start mode: "a script in Playgrounds" stands for mode 4
	// a playground game start (every land but the campaign's first): its second town feature assignment below
	const bool playgroundStart = _firstMapLoaded || path.parent_path().filename() == "Playgrounds";
	if (!playgroundStart)
	{
		game_random::Init();
	}
	else
	{
		game_random::Reset();
	}
	_firstMapLoaded = true;
	// no sequence mode
	game_clock::SetSequenceMode(game_clock::k_SequenceModeNone);
	// both influence multipliers back to 1 before the map script
	auto& mapGlobals = Locator::mapScriptSystem::value().Globals();
	mapGlobals.townInfluenceMultiplier = 1.0f;
	mapGlobals.playerInfluenceMultiplier = 1.0f;
	// the day / night clock's default cycle at noon; the Land script may change it (SET_NIGHTTIME)
	TheDayNightClock().Reset();
	Locator::skySystem::value().SetTime(TheDayNightClock().GetScriptTime());
	ecs::ClearFireFlies();
	ecs::ClearForests();
	ecs::animal_ai::ClearReactions();
	ecs::DisappearSmoke::Clear();
	ecs::object_ghosts::Clear(); // the objects' ghosts go with the map
	ecs::ground_marks::Clear();  // with the map's clear
	// the creatures' footprints go with the map
	ecs::creature_loop::OnLoadMap();
	night_lights::Clear();
	// the audio's reset with the map's SoundTags and street lanterns, before the registry reset (it destroys the
	// lanterns' emitters without freeing their sources); then the scripts' reset (its audio switches)
	audio::ClearMap();
	audio::GetScriptAudioState().Reset();
	// the scripts' reset: the camera switches
	help::script_control::GetCameraControl().Reset();
	// the scripts' reset: SET_INTERFACE_CITADEL's value back to 1 (the citadel entrance may be tapped)
	worship::citadel::ResetInterfaceCitadel();
	ecs::wonders::RegisterStateHash();
	magic::players::RegisterStateHash();
	ecs::town_queries::RegisterStateHash();
	ecs::footpaths::RegisterStateHash();
	ecs::player_creature::RegisterStateHash();
	script_camera::Reset(); // no script camera mode, the FOV at 70 degrees (the camera's start value)
	// the game cleaned for a script reboot: a hand demo still playing ends
	hand_demo::End();
	// the map's clear and the script reboot: every SuperVillager released
	ecs::super_villager::ReleaseAll();
	// the script reboot: the intro's things released (the intro light and hand; the debug camera off)
	ecs::intro_special::ReleaseAll();
	// the script reboot: the interface's state cleared, the hand reach 1800, the camera features 0x1BF, the ControlMap
	// switches 1 / 1, then SetInterfaceInteraction(0), which writes all of them again. (pending) one more interface
	// reset is not ported
	help::interface_interaction::Set(0);
	// the script reboot: the bookmarks on
	if (Locator::cameraBookmarkSystem::has_value())
	{
		Locator::cameraBookmarkSystem::value().SetEnabled(true);
	}
	// the script reboot: the confirmation stops, a START_ANGLE_SOUND 285 / 348 still on goes off
	audio::confirmation::Stop(audio::confirmation::Get());
	// the game's variables cleared: no film goes on into the new map
	if (video::Get().IsPlaying())
	{
		video::Get().Stop();
	}
	// the scripts' reset also resets the help system: the text part
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->Reset();
	}
	ecs::designed_scenery::OnLoadMap();

	// the map script from the byte cache, read afresh on every map load so that a script edited while the game runs is
	// the one loaded (the debug menu's map list)
	auto& blobs = Locator::resources::value().GetBlobs();
	blobs.Erase(resources::BlobId(path));
	const auto& data = resources::LoadBlob(blobs, path);
	const auto source = std::string(reinterpret_cast<const char*>(data.data()), data.size());

	// Reset everything. Deletes all entities and their components. The field / fish farm deletion listeners first:
	// clearing the registry must not send their workers to 163 (fields::DisconnectDeletionListeners)
	ecs::fields::DisconnectDeletionListeners();
	Locator::entitiesRegistry::value().Reset();
	// the dust puffs are plain data now (not entities): they go with the map (ECS/Physics/Dust)
	ecs::physics::Dust::Clear();
	// TODO(#661): split entities that are permanent from map entities and move hand and camera to init
	// We need a hand for the player
	Locator::handSystem::value().Initialize();

	// create our camera
	auto& config = Locator::config::value();
	const auto aspect = Locator::windowing::has_value() ? Locator::windowing::value().GetAspectRatio() : 1.0f;
	Locator::camera::value().SetProjectionMatrixPerspective(config.cameraXFov, aspect, config.cameraNearClip,
	                                                        config.cameraFarClip);

	// the script's map state (the offset, the overrides, the last created), before the map features are loaded
	Script::BeginMapFeatures();
	Script script;
	try
	{
		script.Load(source);
	}
	catch (const std::exception& e)
	{
		// LoadMap is noexcept: a script it cannot read must not end the program
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Error in the map script {}: {}", path.generic_string(), e.what());
	}

	// AssignTownFeatures once the whole script has made the towns, trees and stores: every town's scenic forest, then
	// every town's forest list
	try
	{
		ecs::town_features::AssignTownFeatures();
		// a playground game start: AssignTownFeatures again, after the map features
		if (playgroundStart)
		{
			ecs::town_features::AssignTownFeatures();
		}
	}
	catch (const std::exception& e)
	{
		// LoadMap is noexcept (as the script above): the stores it can make must not end the program
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Error assigning the town features of {}: {}", path.generic_string(),
		                    e.what());
	}

	// the rivers' landscape footprints, once the script has placed their points
	ecs::CreateRiverFootprints();

	// Each released map comes with an optional .fot file which contains the footpath information for the map
	const auto stem = string_utils::LowerCase(path.stem().generic_string());
	const auto fotPath = fileSystem.GetPath<filesystem::Path::Landscape>() / fmt::format("{}.fot", stem);

	if (fileSystem.Exists(fotPath))
	{
		FotFile fotFile(*this);
		fotFile.Load(fotPath);
	}
	else
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The map at {} does not come with a footpath file. Expected {}",
		                   path.generic_string(), fotPath.generic_string());
	}

	_lastGameLoopTime = std::chrono::steady_clock::now();
	_turnDeltaTime = 0ns;
	SetGameSpeed(Game::k_TurnDurationMultiplierNormal);
	// a new game from turn 0 (inferred: openblack has no saved games, so this stands for a loaded game too), then the
	// start of the game loop: the timer from 0 and the local game timer reset
	game_clock::SetTurn(0);
	game_clock::OnLoad();
	// the guidance starts with the interface status (its turn is the new land's), and the spooky voices take the
	// player's name
	audio::guidance::Init();
	audio::spooky::UpdatePlayerName();
	// The original runs from the first frame; OPENBLACK_START_PAUSED=1 keeps openblack's old paused start (test hook)
	game_clock::Start(std::getenv("OPENBLACK_START_PAUSED") != nullptr);

	// (openblack engine) the draw's lazy loads now, not in the middle of a frame, and the upload of what the land made
	if (Locator::rendererInterface::has_value())
	{
		Locator::rendererInterface::value().PreloadForLand();
	}

	return true;
}

void Game::ProcessTempleTurn()
{
	// The temple's turn while the world is paused inside it. (not ported) the debug messages cleared first
	// the temple's help scripts alone
	auto& lhvm = Locator::vm::value();
	lhvm.LookIn(lhvm::ScriptType::TempleHelp | lhvm::ScriptType::TempleSpecial);
	// the audio game turn: the citadel's music
	audio::ProcessCitadelTurn();
	// the room's tooltip, then the help system's turn: the hand state, the input prompt icons, the spirits' turn and the
	// tooltips, which age once a temple turn
	if (Locator::temple::has_value())
	{
		Locator::temple::value().ProcessTurn();
	}
	help::Process(chlapi::ScriptVm(), GetTurn());
	ecs::audio_queries::RunCitadelTestHook(); // OPENBLACK_AUDIO_TEST_CITADEL counts the temple's turns too
}

void Game::ProcessTempleRoomKeys()
{
	if (!Locator::temple::has_value())
	{
		return;
	}
	// The keys for the temple's rooms take the player into the temple at that room's path in, and inside the temple
	// they cut to the room
	constexpr std::array<std::pair<input::BindableActionMap, TempleRoom>, 6> k_RoomKeys {{
	    {input::BindableActionMap::ZOOM_TO_INSIDE_TEMPLE, TempleRoom::Main},
	    {input::BindableActionMap::ZOOM_TO_CREATURE_ROOM, TempleRoom::CreatureCave},
	    {input::BindableActionMap::ZOOM_TO_CHALLENGE_ROOM, TempleRoom::Challenge},
	    {input::BindableActionMap::ZOOM_TO_SAVE_GAME_ROOM, TempleRoom::SaveGame},
	    {input::BindableActionMap::ZOOM_TO_OPTIONS_ROOM, TempleRoom::Options},
	    {input::BindableActionMap::ZOOM_TO_LIBRARY, TempleRoom::Credits},
	}};
	const auto& actions = Locator::gameActionSystem::value();
	auto& temple = Locator::temple::value();
	for (const auto& [action, room] : k_RoomKeys)
	{
		if (!actions.Get(action) || !actions.GetChanged(action))
		{
			continue;
		}
		if (temple.Active())
		{
			temple.GoToRoom(room);
		}
		else
		{
			temple.Activate(room);
		}
		return;
	}
}

void Game::HandleInterfaceAction()
{
	using Action = gui::GameMenu::Action;
	// the SkipBox's answer: 0 none, 1 bit 23, 2 bits 23 and 24, 3 the three; then the game unpaused
	if (const auto answer = _interface->TakeSkipAnswer(); answer)
	{
		auto& skipFlags = Locator::mapScriptSystem::value().Globals().tutorialSkipFlags;
		skipFlags.canSkipTutorial = *answer >= 1;
		skipFlags.canSkipCreatureTraining = *answer >= 2;
		skipFlags.isKeepingOldCreature = *answer >= 3;
		game_clock::Pause(false);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "SkipBox: answer {}", *answer);
	}
	const auto action = _interface->TakeAction();
	if (_interface->TakeSettingsChanged())
	{
		// the options box: a slider sets its main volume at once, value x 127 truncated toward zero; Audio applies the
		// configuration's every frame
		const auto& settings = _interface->GetMenu().GetSettings();
		auto& config = Locator::config::value();
		config.audioSampleMainVolume = static_cast<uint32_t>(settings.sfxVolume * 127.0f);
		config.audioMusicMainVolume = static_cast<uint32_t>(settings.musicVolume * 127.0f);
		// The detail only takes effect at the next start ("detailidx", saved and read at start-up): (pending)
		// openblack has no settings file to keep it in. (pending) Auto Save (a bit of the game's flags) and Push
		// Scroll (a profile setting): openblack has neither yet
	}
	// the menu pauses the game while it is open (Continue: hide and unpause). (inferred) Opened inside the temple, by
	// its options room, the menu leaves the pause alone: the temple holds it and gives it back as the player leaves
	const bool open = _interface->GetMenu().IsOpen();
	if (open && !_menuWasOpen)
	{
		_menuPaused = !game_clock::IsInsideCitadel();
		if (_menuPaused)
		{
			game_clock::Pause(true);
		}
	}
	else if (!open && _menuWasOpen)
	{
		// closing the options: the help's READ_SPEED from the slider
		if (auto* helpSystem = help::Get(); helpSystem != nullptr)
		{
			helpSystem->SetReadSpeed(_interface->GetMenu().GetSettings().helpTextSpeed);
		}
		if (_menuPaused)
		{
			game_clock::Pause(false); // Continue: unpause, a plain flag
		}
		_menuClosedTicks = SDL_GetTicks(); // the menu's close event: when, for Escape's debounce
	}
	_menuWasOpen = open;
	if (action == Action::Quit)
	{
		Locator::config::value().running = false;
	}
	else if (action != Action::None && action != Action::Continue)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "This part of the menu is not available yet");
	}
}

void Game::LoadLandscape(const std::filesystem::path& path)
{
	auto& fileSystem = Locator::filesystem::value();

	auto fixedName = fileSystem.FindPath(filesystem::FileSystemInterface::FixPath(path));

	if (!fileSystem.Exists(fixedName))
	{
		throw std::runtime_error("Could not find landscape " + path.generic_string());
	}
	InitializeLevel(fixedName);
	// the landscape's open: the creature's walkable mask of the new landscape
	land_avoid::Validate(Locator::terrainSystem::value());
	ecs::route_plan_world::OnLandLoaded();
	land_avoid::DumpIfRequested();

	// There is always a player active
	Locator::playerSystem::value().AddPlayer(ecs::archetypes::PlayerArchetype::Create(PlayerNames::PLAYER_ONE));

	Locator::cameraBookmarkSystem::value().Initialize();
	Locator::dynamicsSystem::value().RegisterIslandRigidBodies(Locator::terrainSystem::value());
	Locator::playerSystem::value().RegisterPlayers();
}

void Game::SetTime(float time) noexcept
{
	// SET_GAME_TIME: the clock keeps running from this script time
	TheDayNightClock().SetScriptTime(time);
	Locator::skySystem::value().SetTime(TheDayNightClock().GetScriptTime());
}
