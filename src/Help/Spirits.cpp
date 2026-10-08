/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Spirits.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <string>
#include <utility>

#include "3D/ObjectMatrix.h"
#include "Camera/ScriptCamera.h"
#include "Common/GameRandom.h"
#include "ECS/FastExp.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "Help/HelpDudeFile.h"
#include "Locator.h"

namespace openblack::help::spirits
{

namespace
{
// pi, 2 pi and pi / 2: the shared constants of Camera/ScriptCamera.h
using script_camera::k_HalfPi;
using script_camera::k_Pi;
using script_camera::k_TwoPi;
constexpr float k_HoverClampX = 0.75f;
constexpr float k_HoverClampY = 0.6f;
constexpr float k_Third = 0.333333343f;

/// The anim names (0..9 fixed, 10..17 copied from the emotion names, 18.. written when the table is built)
constexpr std::array<const char*, k_AnimSlots> k_AnimNames = {
    "Stand",         "Hover",       "Hover left",  "Hover right", "Wingflap",        "L Eye Shut",
    "R Eye Shut",    "Vowel E",     "Vowel O",     "Vowel S",     "Normal",          "Pleased",
    "Displeased",    "Sad",         "Sarcastic",   "Afraid",      "Confused",        "Furious",
    "Look L/R",      "Look Up/Dn",  "NodHead",     "ShakeHead",   "CockHead",        "HeadInHands",
    "Laugh",         "CrossArms",   "Spare",       "PointL In",   "Point L",         "PointR In",
    "Point R",       "HoverStable", "Avoid L",     "Avoid R",     "Avoid U",         "Avoid D",
    "HandsOnHips",   "Spare!",      "ShakeFinger", "ScratchHead", "ScratchChin",     "PickNose",
    "PointAtCamera", "HangHead",    "Sulk",        "Cry",         "GoInvisible",     "Dance",
    "PressFace",     "StrokeBeard", "CoverEyes",   "Pray",        "PunchAir",        "WaveNo",
    "Dodgy",         "Dismiss",     "Rude",        "HeadSlap",    "KnockScreen",     "Wanker",
    "HandGun",       "Burp",        "Fart",        "Triumph",     "Shrug",           "ClingL",
    "ClingR",        "ClingU",      "ClingD",      "GimmeFive",   "Look L/R Stable", "Look U/D Stable",
    "Chuckle",       "Spare 4",     "Spare 5",     "Spare 6",     "Spare 7",         "Spare 8",
    "Spare 9",       "Spare 10",
};
/// The emotion names
constexpr std::array<const char*, 8> k_EmotionNames = {"Normal",    "Pleased", "Displeased", "Sad",
                                                       "Sarcastic", "Afraid",  "Confused",   "Furious"};

/// Row-vector v x rows (the original's 3x3 matrix products)
glm::vec3 ByRows(const glm::vec3& v, const glm::mat3& rows)
{
	return v.x * rows[0] + v.y * rows[1] + v.z * rows[2];
}

bool IsSpace(char c)
{
	return std::isspace(static_cast<unsigned char>(c)) != 0;
}

int CompareIgnoringCase(const char* a, const char* b, size_t n)
{
	for (size_t i = 0; i < n; ++i)
	{
		const int ca = std::toupper(static_cast<unsigned char>(a[i]));
		const int cb = std::toupper(static_cast<unsigned char>(b[i]));
		if (ca != cb || ca == 0)
		{
			return ca - cb;
		}
	}
	return 0;
}

/// The tag parser's word buffer: it keeps its word from one call to the next (Locator::scriptState)
struct TagWordState
{
	std::string word;
};

std::string& TagWordBuffer()
{
	return openblack::Locator::scriptState::value().Get<TagWordState>().word;
}

/// One tag of `s` from `pos`: the position after it (and the spaces after it)
size_t ParseOneTag(const std::string& s, size_t pos, AudioTag& tag, int& errors)
{
	auto skipSpaces = [&s](size_t p) {
		while (p < s.size() && IsSpace(s[p]))
		{
			++p;
		}
		return p;
	};
	auto at = [&s](size_t p) { return p < s.size() ? s[p] : '\0'; };

	// the talker
	tag.who = 0;
	switch (std::toupper(static_cast<unsigned char>(at(pos))))
	{
	case 'T':
		tag.who = 0;
		break;
	case '!':
	case 'O':
		tag.who = 1;
		break;
	case 'G':
		tag.who = 2;
		break;
	case 'E':
		tag.who = 3;
		break;
	case '*':
	case 'B':
		tag.who = 4;
		break;
	default:
		++errors; // "unrecognised talker in audio tag [%s]"
		break;
	}
	pos = skipSpaces(pos + 1);

	// the type
	tag.action = 0;
	const int next = std::toupper(static_cast<unsigned char>(at(pos + 1)));
	switch (std::toupper(static_cast<unsigned char>(at(pos))))
	{
	case 'A':
		if (next == 'S')
		{
			++pos;
			tag.action = 2;
		}
		else if (next == 'R')
		{
			++pos;
			tag.action = 3;
		}
		else
		{
			tag.action = 1;
		}
		break;
	case 'E':
		tag.action = 4;
		break;
	case 'L':
		if (next == 'S')
		{
			++pos;
			tag.action = 6;
		}
		else if (next == 'R')
		{
			++pos;
			tag.action = 7;
		}
		else
		{
			tag.action = 5;
		}
		break;
	case 'R':
		tag.action = 3;
		break;
	default:
		++errors; // "unrecognised tag type in audio tag [%s]"
		break;
	}
	pos = skipSpaces(pos + 1);

	// the word: after the space skip the rest is either empty, which leaves the buffer's previous word (sscanf gives
	// EOF), or starts with a word; the error test counts only a result of 0, which cannot happen here. Then the position
	// moves on by the buffer's length, the previous word's length too
	std::string& buffer = TagWordBuffer();
	if (pos < s.size())
	{
		size_t end = pos;
		while (end < s.size() && !IsSpace(s[end]))
		{
			++end;
		}
		buffer = s.substr(pos, end - pos);
	}
	const std::string word = buffer;
	// (openblack) guard: the original's pointer can pass the label's NUL when the previous word is kept
	pos = std::min(pos + word.size(), s.size());

	// the trailing digits: from the last character back over digits, never testing the first
	tag.value = 100;
	size_t digits = word.size();
	if (digits > 0)
	{
		--digits;
		while (digits > 0 && std::isdigit(static_cast<unsigned char>(word[digits])) != 0)
		{
			--digits;
		}
	}
	if (digits + 1 < word.size() && std::isdigit(static_cast<unsigned char>(word[digits + 1])) != 0)
	{
		tag.value = std::atoi(word.c_str() + digits + 1);
	}
	// the letters at the start: counted up to the first non-letter after the first character, at most up to the first
	// trailing digit; the count goes up before each isalpha
	const size_t bound = word.empty() ? 1 : digits + 1;
	size_t letters = 0;
	if (!word.empty() && std::isalpha(static_cast<unsigned char>(word[0])) != 0)
	{
		do
		{
			if (letters >= bound)
			{
				break;
			}
			++letters;
		} while (std::isalpha(static_cast<unsigned char>(word[letters])) != 0);
	}
	if (tag.value <= 0 || tag.value >= 100)
	{
		tag.value = 100;
	}

	tag.index = 0;
	bool found = true;
	switch (tag.action)
	{
	case 1:
	case 2:
	case 3: // strnicmp over min(strlen(name), letters) against the 80 anim names
		found = false;
		for (size_t i = 0; i < k_AnimNames.size(); ++i)
		{
			const size_t n = std::min(std::strlen(k_AnimNames[i]), letters);
			if (CompareIgnoringCase(word.c_str(), k_AnimNames[i], n) == 0)
			{
				tag.index = static_cast<int32_t>(i);
				found = true;
				break;
			}
		}
		break;
	case 4: // the same against the 8 emotions
		found = false;
		for (size_t i = 0; i < k_EmotionNames.size(); ++i)
		{
			const size_t n = std::min(std::strlen(k_EmotionNames[i]), letters);
			if (CompareIgnoringCase(word.c_str(), k_EmotionNames[i], n) == 0)
			{
				tag.index = static_cast<int32_t>(i);
				found = true;
				break;
			}
		}
		break;
	case 5:
	case 6:
	case 7: // the first letter
		switch (std::toupper(static_cast<unsigned char>(word.empty() ? '\0' : word[0])))
		{
		case 'O':
			tag.index = 1;
			break;
		case 'C':
			tag.index = 2;
			break;
		case 'P':
			tag.index = 3;
			break;
		case 'H':
		case 'M':
			tag.index = 4;
			break;
		case 'D':
		case 'N':
			tag.index = 0;
			break;
		default:
			found = false;
			break;
		}
		break;
	default:
		found = false;
		break;
	}
	if (!found)
	{
		++errors; // "unrecognised name in audio tag [%s]"
	}
	return skipSpaces(pos);
}

} // namespace

float Smooth(float t)
{
	if (t < 0.0f)
	{
		return 0.0f;
	}
	if (t > 1.0f)
	{
		return 1.0f;
	}
	return (1.0f - std::cos(t * k_Pi)) * 0.5f;
}

std::vector<AudioTag> ParseAudioTags(std::string_view label, float time, int* errors)
{
	int count = 0;
	std::string s(label);
	// ';' and '/' end the label
	if (const auto cut = s.find_first_of(";/"); cut != std::string::npos)
	{
		s.resize(cut);
	}
	auto trim = [](std::string& t) {
		size_t b = 0;
		while (b < t.size() && IsSpace(t[b]))
		{
			++b;
		}
		t.erase(0, b);
		while (!t.empty() && IsSpace(t.back()))
		{
			t.pop_back();
		}
	};
	trim(s);
	if (!s.empty() && s[0] == '[')
	{
		s.erase(0, 1);
		if (const auto close = s.find(']'); close != std::string::npos)
		{
			s.resize(close);
		}
		else
		{
			++count; // an unclosed '[' is an error
		}
		trim(s);
	}
	std::vector<AudioTag> tags;
	size_t pos = 0;
	while (pos < s.size())
	{
		AudioTag tag;
		tag.time = time;
		const size_t next = ParseOneTag(s, pos, tag, count);
		tags.push_back(tag);
		if (next <= pos)
		{
			break;
		}
		pos = next;
	}
	if (errors != nullptr)
	{
		*errors += count;
	}
	return tags;
}

DudeData DudeData::FromFile(const HelpDudeFile& file)
{
	DudeData data;
	for (size_t i = 0; i < k_AnimSlots; ++i)
	{
		data.clips[i] = file.Clip(i);
		if (i < file.animEvents.size())
		{
			data.loopStart[i] = file.animEvents[i].loopStart;
			data.loopEnd[i] = file.animEvents[i].loopEnd;
		}
	}
	data.flags = file.animFlags;
	// 8 face records of 0x40 bytes
	const size_t records = std::min<size_t>(file.emotionFaces.size() / 0x40, data.faces.size());
	for (size_t r = 0; r < records; ++r)
	{
		std::memcpy(data.faces[r].data(), file.emotionFaces.data() + r * 0x40, 0x40);
	}
	data.startEmotion = file.startEmotion;
	data.modelSize = file.restHeight;
	data.nearDepth = file.nearDepth;
	data.farDepth = file.farDepth * 1.2f;
	data.scale = file.modelScale;
	data.pitchOffset = file.value35C0;
	data.fingertipOffsetRow0 = file.value35C4;
	data.fingertipOffsetRow2 = file.value35C8;
	return data;
}

// ---------------------------------------------------------------------------------------------------------------------
// AdvisorSpirit

float AdvisorSpirit::Zone::Feel(float px, float py) const
{
	if (strength == 0.0f)
	{
		return 0.0f;
	}
	const float dx = px - x;
	if (outer < std::abs(dx))
	{
		return 0.0f;
	}
	const float dy = py - y;
	if (outer < std::abs(dy))
	{
		return 0.0f;
	}
	const float d2 = dy * dy + dx * dx;
	if (d2 > outer * outer)
	{
		return 0.0f;
	}
	if (d2 < inner * inner)
	{
		return strength;
	}
	return (1.0f - (std::sqrt(d2) - inner) / (outer - inner)) * strength;
}

AdvisorSpirit::AdvisorSpirit(int index, const DudeData& data, AdvisorSpiritController& control)
    : _index(index)
    , _data(data)
    , _control(control)
{
	// everything 0 but the values below; the emotion starts at the .hd's
	_slotLast.fill(-1.0f);
	_lastMouse = control.Frame().mouse;
	_emotion = data.startEmotion;
	_face = data.faces[0]; // (pending) the record the original builds before the first UpdateFace
	_hoverX.SetPosition(0.0f);
	_hoverY.SetPosition(0.0f);
	_depth.SetPosition(0.0f);
	_worldTarget.SetPosition(glm::vec3(0.0f));
	SetState(dude_state::k_Hover, false);
}

void AdvisorSpirit::SetPosition(glm::ivec2 pixel)
{
	const Screen& screen = _control.GetScreen();
	const auto halfW = static_cast<float>(screen.HalfWidth());
	_hoverX.SetPosition(static_cast<float>(pixel.x - screen.HalfWidth()) / halfW);
	_hoverY.SetPosition(static_cast<float>(pixel.y - screen.HalfHeight()) / halfW);
	ResetTrail();
	SetState(dude_state::k_Hover, false);
}

void AdvisorSpirit::FlyTo(glm::ivec2 pixel, float seconds, bool clamp)
{
	const Screen& screen = _control.GetScreen();
	const auto halfW = static_cast<float>(screen.HalfWidth());
	const float hx = static_cast<float>(pixel.x - screen.HalfWidth()) / halfW;
	const float hy = static_cast<float>(pixel.y - screen.HalfHeight()) / halfW;
	SetHoverX(hx, seconds, clamp);
	SetHoverY(hy, seconds, clamp);
	_clingX = hx; // the unclamped target
	_clingY = hy;
	SnapClingEdge();
	SetState(dude_state::k_Hover, false);
}

void AdvisorSpirit::SetHoverX(float target, float seconds, bool clamp)
{
	if (_puffRunning)
	{
		return;
	}
	if (clamp)
	{
		if (target <= -k_HoverClampX)
		{
			target = -k_HoverClampX;
		}
		else if (!(target < k_HoverClampX))
		{
			target = k_HoverClampX;
		}
	}
	_hoverX.SetDestinationWithSpeedAndTime(target, 0.0f, seconds);
}

void AdvisorSpirit::SetHoverY(float target, float seconds, bool clamp)
{
	if (_puffRunning)
	{
		return;
	}
	if (clamp)
	{
		if (target <= -k_HoverClampY)
		{
			target = -k_HoverClampY;
		}
		else if (!(target < k_HoverClampY))
		{
			target = k_HoverClampY;
		}
	}
	_hoverY.SetDestinationWithSpeedAndTime(target, 0.0f, seconds);
}

void AdvisorSpirit::SetEmotion(uint32_t emotion, float peak)
{
	_emotionTime = 0.0f;
	_emotionPeak = peak > 0.0f ? peak : 0.0f;
	_emotionTarget = emotion;
}

void AdvisorSpirit::ClearAnims()
{
	_slotMode.fill(0);
}

void AdvisorSpirit::SnapClingEdge()
{
	if (_state == dude_state::k_ClingLeave)
	{
		return;
	}
	if (std::abs(_clingY * 1.28205f) < std::abs(_clingX))
	{
		if (_clingX > 0.0f)
		{
			_edge = Edge::Right;
			_clingX = 1.04f;
		}
		else
		{
			_edge = Edge::Left;
			_clingX = -1.04f;
		}
	}
	else if (_clingY > 0.0f)
	{
		_edge = Edge::Bottom;
		_clingY = 0.78f;
	}
	else
	{
		_edge = Edge::Top;
		_clingY = -0.78f;
	}
}

void AdvisorSpirit::Cling(float hx, float hy, bool fromHome)
{
	_clingX = hx;
	_targetX = hx;
	_clingY = hy;
	_targetY = hy;
	SnapClingEdge();
	if (fromHome)
	{
		SetPosition(_control.Anchor(static_cast<int>(_edge)));
		ResetTrail();
	}
	SetState(dude_state::k_Cling, false);
}

bool AdvisorSpirit::IsPlayingAnim() const
{
	if ((_state == dude_state::k_FlyToAnim || _queuedState == dude_state::k_FlyToAnim) &&
	    _afterFly == dude_state::k_ScriptedAnim)
	{
		return true;
	}
	return _state == dude_state::k_ScriptedAnim || _queuedState == dude_state::k_ScriptedAnim;
}

void AdvisorSpirit::PlayAnim(float hx, float hy, uint32_t anim, float speed)
{
	if (anim == 0 && IsPlayingAnim())
	{
		SetState(dude_state::k_Hover, false);
		return;
	}
	_targetY = hy;
	_targetX = hx;
	_afterFly = dude_state::k_ScriptedAnim;
	_animSpeed = speed;
	_scriptAnim = anim;
	SetState(dude_state::k_FlyToAnim, false);
}

void AdvisorSpirit::ScreenPoint(glm::ivec2 pixel)
{
	_pointOffScreen = false;
	const Screen& screen = _control.GetScreen();
	const auto halfW = static_cast<float>(screen.HalfWidth());
	_pointX = static_cast<float>(pixel.x - screen.HalfWidth()) / halfW;
	_pointY = static_cast<float>(pixel.y - screen.HalfHeight()) / halfW;
	SetState(dude_state::k_Point, false);
}

void AdvisorSpirit::PointAt(const glm::vec3& position, bool inWorld, float side, float height)
{
	if (_inWorld == 0.0f)
	{
		_worldTarget.SetPosition(position);
	}
	else
	{
		_worldTarget.SetDestinationWithTime(position, 0.5f);
	}
	_worldSide = side;
	_inWorldTarget = inWorld ? 1.0f : 0.0f;
	_worldHeight = height;
	_pointOffScreen = false;
	const Queries& q = _control.GetQueries();
	std::optional<ProjectedPoint> projected;
	if (q.projectPoint)
	{
		projected = q.projectPoint(position);
	}
	else
	{
		projected = ProjectedPoint {static_cast<int32_t>(position.x), static_cast<int32_t>(position.y), position.z};
	}
	const float nearClip = q.nearClip ? q.nearClip() : 0.0f;
	// off screen: the projection fails, depth < 1.1 near or y > H
	if (!projected || nearClip * 1.1f > projected->depth || projected->y > _control.GetScreen().height)
	{
		_pointOffScreen = true;
	}
	const auto hover = WorldToHover(position, true);
	if (hover && !_pointOffScreen)
	{
		_pointY = hover->y;
		_pointX = hover->x;
		SetState(dude_state::k_Point, false);
		_lookTarget = position;
		return;
	}
	_pointX = 0.0f;
	_pointY = 1.0f;
	SetState(dude_state::k_Point, false);
	_lookTarget.reset();
	_pointOffScreen = true;
}

void AdvisorSpirit::SetState(uint32_t next, bool force)
{
	if (next == 0)
	{
		next = _queuedState != 0 ? _queuedState : dude_state::k_Hover;
	}
	if (_state == next)
	{
		return;
	}
	if (!force && (_state & 0x30) != 0)
	{
		_queuedState = next;
		return;
	}
	_queuedState = 0;
	bool enter = true;
	if (_state > dude_state::k_Cling)
	{
		if (_state == dude_state::k_ClingLeave)
		{
			_state = dude_state::k_Hover;
		}
	}
	else if (_state == dude_state::k_Cling)
	{
		if ((next & 0x100) == 0)
		{
			next = dude_state::k_ClingLeave;
			enter = false;
		}
	}
	else if (_state == dude_state::k_PointHoldL || _state == dude_state::k_PointHoldR)
	{
		if ((next & 8) == 0 && (next & 0x100) == 0)
		{
			next = _state | 0x20; // the outro
			enter = false;
		}
	}
	else if (_state == dude_state::k_PointOutroL || _state == dude_state::k_PointOutroR)
	{
		_state = dude_state::k_Hover;
	}

	if (enter)
	{
		switch (next)
		{
		case dude_state::k_Hover: // re-sync the hover from the model's 3D point
		{
			const auto hover = WorldToHover(_position, true);
			const glm::vec2 h = hover ? *hover : glm::vec2(0.0f);
			_hoverX.SetPosition(h.x);
			_hoverY.SetPosition(h.y);
			break;
		}
		case dude_state::k_Point: // the arm
		{
			uint32_t side = _state & 3;
			if (side == 0)
			{
				side = _control.LocalRand(2) != 0 ? 1 : 2;
			}
			const float hx = _hoverX.value;
			if (hx < _pointX || _pointX > 0.25f)
			{
				side = 1;
			}
			if (hx > _pointX || _pointX < -0.25f)
			{
				side = 2;
			}
			if (_inWorldTarget != 0.0f)
			{
				side = 1;
			}
			float x;
			if (side == 1)
			{
				next = dude_state::k_PointIntroL;
				x = _pointX - 0.2f;
			}
			else
			{
				side = 2;
				next = dude_state::k_PointIntroR;
				x = _pointX + 0.2f;
			}
			if ((_state & 8) == 0)
			{
				SetHoverY(_pointY, 1.0f, true);
				SetHoverX(x, 1.0f, true);
			}
			else if ((_state & 3) == side)
			{
				next = _state; // the same arm: no change
			}
			else
			{
				SetHoverY(_pointY, 1.0f, true);
				SetHoverX(x, 1.0f, true);
				next = _state | 0x20; // the other arm: outro, then point again
				_queuedState = dude_state::k_Point;
			}
			break;
		}
		case dude_state::k_Avoid:
			_avoidDir &= 3;
			if (_state != dude_state::k_Hover)
			{
				next = _state;
				break;
			}
			_inWorldTarget = 0.0f;
			_hoverX.SetPosition(_hoverX.value);
			_hoverY.SetPosition(_hoverY.value);
			break;
		case dude_state::k_FlyToAnim:
		{
			if (_state == dude_state::k_FlyToAnim)
			{
				break;
			}
			const float dx = _hoverX.value - _targetX;
			const float dy = _hoverY.value - _targetY;
			float seconds = std::sqrt(dy * dy + dx * dx);
			if (seconds > 0.05f)
			{
				if (seconds > 1.5f)
				{
					seconds = 1.5f;
				}
				else if (seconds < 0.5f)
				{
					seconds = 0.5f;
				}
			}
			_inWorldTarget = 0.0f;
			_hoverX.SetPosition(_hoverX.value);
			_hoverY.SetPosition(_hoverY.value);
			SetHoverX(_targetX, seconds, true);
			SetHoverY(_targetY, seconds, true);
			break;
		}
		case dude_state::k_Cling:
			if ((_state & 0x100) != 0)
			{
				next = dude_state::k_Cling;
				break;
			}
			_clingClock = 0.0f;
			next = dude_state::k_ClingArrive;
			SetHoverX(_clingX, 1.0f, false);
			SetHoverY(_clingY, 1.0f, false);
			break;
		default:
			break;
		}
	}
	if (_state != next)
	{
		_stateTime = 0.0f;
	}
	_state = next;
}

void AdvisorSpirit::RandomisePuffParticle(PuffParticle& particle) const
{
	// Random (the CRT stream) six times: the grey first, then vx, vy, vz, the size and the spin
	const auto g = static_cast<uint32_t>(static_cast<int32_t>(_control.Random(116.0f, 250.0f))); // truncated
	particle.grey = ((g << 8u | g) << 8u) | g;
	particle.velocity.x = _control.Random(-0.8f, 0.8f);
	particle.velocity.y = _control.Random(-1.9f, 1.5f);
	particle.velocity.z = _control.Random(-1.0f, 1.0f);
	particle.k = (1.0f - particle.velocity.y) + 1.0f;
	particle.sizeBase = _control.Random(4.0f, 8.0f);
	particle.spin = _control.Random(-2.0f, 2.0f);
	particle.age = 0.0f;
}

void AdvisorSpirit::StartPuff()
{
	// the 16 particles drawn again with Random only once the draw has made them; the first puff of a dude draws them
	// when UpdateDraw makes them
	if (_puffParticles)
	{
		for (auto& particle : *_puffParticles)
		{
			RandomisePuffParticle(particle);
		}
	}
	_puffRunning = true;
	_puffTime = 0.0f;
	_alphaTarget = _alpha > 0.5f ? 0.0f : 1.0f;
}

glm::vec3 AdvisorSpirit::HoverTo3D(float hx, float hy, float k, bool nearFlag) const
{
	const Screen& screen = _control.GetScreen();
	const auto halfW = static_cast<float>(screen.HalfWidth());
	const auto px = static_cast<int32_t>((hx + 1.0f) * halfW);
	const auto py = static_cast<int32_t>(hy * halfW + static_cast<float>(screen.HalfHeight()));
	const Queries& q = _control.GetQueries();
	float depth;
	if (nearFlag)
	{
		const float nearClip = q.nearClip ? q.nearClip() : 0.0f;
		depth = nearClip + nearClip;
	}
	else
	{
		depth = ((_data.farDepth - _data.nearDepth) * Smooth(_closeness) + _data.nearDepth) * (k * 0.3f + 1.0f);
	}
	const glm::vec2 pixel(static_cast<float>(px), static_cast<float>(py));
	return q.pointFromScreen ? q.pointFromScreen(pixel, depth) : glm::vec3(pixel, depth);
}

std::optional<glm::vec2> AdvisorSpirit::WorldToHover(const glm::vec3& p, bool force) const
{
	const Queries& q = _control.GetQueries();
	std::optional<glm::vec2> pixel;
	if (q.worldToPixel)
	{
		pixel = q.worldToPixel(p, force);
	}
	else
	{
		pixel = glm::vec2(p.x, p.y);
	}
	if (!pixel)
	{
		return std::nullopt;
	}
	const Screen& screen = _control.GetScreen();
	const auto halfW = static_cast<float>(screen.HalfWidth());
	return glm::vec2((pixel->x - halfW) / halfW, (pixel->y - static_cast<float>(screen.HalfHeight())) / halfW);
}

float AdvisorSpirit::Feel(float x, float y) const
{
	const float limit = _control.Frame().wideScreen ? 0.45f : 0.6f;
	float e = 0.0f;
	const float ax = std::abs(x);
	const float ay = std::abs(y) * 1.33333337f;
	if (ax > 0.75f)
	{
		e = ax - 0.75f;
	}
	if (ay > limit)
	{
		e += (ay - limit) + (ay - limit);
	}
	float sum = e * e * -250.0f;
	for (const auto& zone : _zones)
	{
		sum = zone.Feel(x, y) + sum;
	}
	return sum;
}

void AdvisorSpirit::ApplyAnim(uint32_t anim, float phase, float referencePhase, bool wrap)
{
	const SpiritAnimClip* clip = Clip(anim);
	if (clip == nullptr)
	{
		return;
	}
	const ApplyAnimArgs args = ApplyAnimArguments(*clip, phase, referencePhase, wrap);
	if (anim == anim::k_GoInvisible) // the flicker windows
	{
		const float p = args.phase;
		if (_index == k_EvilDude)
		{
			_flicker = _flicker || (p > 0.13f && p < 0.25f) || (p > 0.55f && p < 0.7f);
		}
		else
		{
			_flicker = _flicker || (p > 0.16f && p < 0.25f);
		}
	}
	_position += ByRows(args.rootMove, _rows);
	_layers.push_back({AnimLayer::Kind::Add, anim, args.milliseconds, args.referenceKey});
}

void AdvisorSpirit::AddAt(uint32_t anim, float weight, uint32_t referenceKey)
{
	const SpiritAnimClip* clip = Clip(anim);
	if (clip == nullptr)
	{
		return;
	}
	auto ms = static_cast<int32_t>(static_cast<float>(clip->durationMs) * weight);
	if (ms < 0)
	{
		ms = 0;
	}
	if (ms >= clip->durationMs)
	{
		ms = clip->durationMs - 1;
	}
	_layers.push_back({AnimLayer::Kind::Add, anim, ms, referenceKey});
}

void AdvisorSpirit::Sound(uint32_t anim, float phase, bool sfx)
{
	if (sfx)
	{
		_sounds.push_back({anim, phase});
	}
}

void AdvisorSpirit::UpdateHoverPosition(float probability)
{
	if (_state == dude_state::k_Avoid || _state == dude_state::k_FlyToAnim || (_state & 0x100) != 0 ||
	    _state == dude_state::k_ScriptedAnim)
	{
		return;
	}
	if (_hoverX.time != _hoverX.duration || _hoverY.time != _hoverY.duration || _puffRunning)
	{
		return;
	}
	const float x0 = _hoverX.value;
	const float y0 = _hoverY.value;
	const float f0 = Feel(x0, y0);
	// each step rounded to a float (the double 1.0 is exact in float)
	float radius = (std::abs(f0) + 1.0f) * 0.3f;
	int tries = 1;
	bool urgent = false;
	if (f0 < -1.0f)
	{
		urgent = true;
		radius = 2.0f;
		tries = 25;
	}
	float best = f0;
	glm::vec2 bestPos(x0, y0);
	int32_t hop = -1;
	bool keepHop = false;

	if (partner != nullptr && _partnerDistance < 0.9f)
	{
		// the Avoid hop away from the partner
		const float dx = _hoverX.value - partner->_hoverX.value;
		const float dy = _hoverY.value - partner->_hoverY.value;
		uint32_t dir;
		if (std::abs(dy) < std::abs(dx))
		{
			dir = dx < 0.0f ? 0 : 1;
		}
		else
		{
			dir = dy < 0.0f ? 2 : 3;
		}
		if (const SpiritAnimClip* clip = Clip(anim::k_AvoidL + dir); clip != nullptr)
		{
			const glm::vec3 landing = ByRows(clip->displacement, _rows) + _position;
			if (const auto h = WorldToHover(landing, true))
			{
				const float f1 = (Feel(h->x, h->y) - f0) * 6.0f + f0;
				if (f1 > f0)
				{
					best = f1;
					bestPos = *h;
					hop = static_cast<int32_t>(dir);
					if (_control.LocalRand(10) >= 5)
					{
						keepHop = true;
					}
					else
					{
						hop = -1; // the search goes on from the hop's score (sic)
					}
				}
			}
		}
	}
	if (!keepHop)
	{
		for (int i = 0; i < tries; ++i)
		{
			const float span = radius + radius;
			float cx = _control.LocalFloatRand(span) + x0 - radius;
			float cy = _control.LocalFloatRand(span) + y0 - radius;
			if (cx < -1.0f)
			{
				cx = -1.0f;
			}
			else if (!(cx <= 1.0f))
			{
				cx = 1.0f;
			}
			if (cy < -0.75f)
			{
				cy = -0.75f;
			}
			else if (!(cy <= 0.75f))
			{
				cy = 0.75f;
			}
			const float f = Feel(cx, cy);
			if (f > best)
			{
				best = f;
				bestPos = {cx, cy};
				hop = -1;
			}
		}
	}
	if (!urgent)
	{
		float gain = 0.0f;
		if (best > f0)
		{
			gain = (best - f0) * probability;
		}
		if (_control.LocalFloatRand(1.0f) > gain)
		{
			return;
		}
	}
	if (hop >= 0)
	{
		_avoidDir = static_cast<uint32_t>(hop);
		SetState(dude_state::k_Avoid, false);
		return;
	}
	SetHoverX(bestPos.x, 1.0f, true);
	SetHoverY(bestPos.y, 1.0f, true);
}

void AdvisorSpirit::UpdateDepthSpacing(float zMin)
{
	if (_gimme || (_activeFlags & 0x40) != 0)
	{
		_depth.SetDestinationWithSpeedAndTime(0.0f, 0.0f, 0.5f);
		return;
	}
	if (partner != nullptr)
	{
		const float dy = _hoverY.value - partner->_hoverY.value;
		const float dx = _hoverX.value - partner->_hoverX.value;
		const float d2 = dx * dx + dy * dy;
		const float d = std::sqrt(d2);
		_partnerDistance = d;
		partner->_partnerDistance = d;
		if (d2 < 0.81f)
		{
			const float k = d * 1.11111116f;
			partner->_depth.SetDestinationWithSpeedAndTime((1.0f - k * k) * -0.5f, 0.0f, 1.0f);
			const float own = dy < zMin ? zMin : dy;
			_depth.SetDestinationWithSpeedAndTime(own, 0.0f, 1.0f);
			return;
		}
	}
	_depth.SetDestinationWithSpeedAndTime(0.0f, 0.0f, 1.0f);
}

void AdvisorSpirit::UpdateZones(float dt, bool focus)
{
	const FrameInput& frame = _control.Frame();
	Zone& partnerZone = _zones[1];
	if (partner != nullptr && partner->_inWorld == 0.0f)
	{
		partnerZone.strength = focus ? -0.5f : -5.0f;
		const float r = partner->_modelScale * partner->_data.scale * partner->_data.modelSize / partner->_data.nearDepth;
		partnerZone.outer = r + r;
		partnerZone.inner = partnerZone.outer * 0.1f;
		partnerZone.x = partner->_hoverX.value;
		partnerZone.y = partner->_hoverY.value;
	}
	else
	{
		partnerZone.strength = 0.0f;
	}
	if ((_state & 8) != 0)
	{
		_zones[2] = {4.0f, _pointX, _pointY, 0.2f, 0.6f};
		_zones[3] = {-1.0f, _pointX, _pointY, 0.08f, 0.16f};
	}
	else
	{
		_zones[2].strength = 0.0f;
		_zones[3].strength = 0.0f;
	}
	const Screen& screen = frame.screen;
	const auto halfW = static_cast<float>(screen.HalfWidth());
	const auto halfH = static_cast<float>(screen.HalfHeight());
	Zone& mouse = _zones[4];
	mouse.inner = 0.16f;
	mouse.outer = 0.6f;
	const float mdx = static_cast<float>(frame.mouse.x - _lastMouse.x) / halfW;
	const float mdy = static_cast<float>(frame.mouse.y - _lastMouse.y) / halfW;
	_lastMouse = frame.mouse;
	// (openblack) a frame of dt 0 gives speed 0 instead of the original's division by zero
	_mouseSpeed = dt > 0.0f ? std::sqrt(mdy * mdy + mdx * mdx) / dt : 0.0f;
	const float candidate = _mouseSpeed * -1.5f - 2.0f;
	if (candidate < mouse.strength)
	{
		mouse.strength = candidate;
	}
	else
	{
		mouse.strength = (candidate - mouse.strength) * 0.3f + mouse.strength;
	}
	mouse.x = (static_cast<float>(frame.mouse.x) - halfW) / halfW;
	mouse.y = (static_cast<float>(frame.mouse.y) - halfH) / halfW;
	if (frame.wideScreen)
	{
		mouse.strength = 0.0f;
	}
	_zones[5] = {-4.0f, 0.0f, 0.0f, 0.0f, 0.6f};
}

void AdvisorSpirit::UpdateMatrix(float dt)
{
	// the HUD pose
	const Queries& q = _control.GetQueries();
	const glm::mat3 camera = q.cameraAxes ? q.cameraAxes() : glm::mat3(1.0f);
	const float scale = _data.scale * _modelScale;
	glm::mat3 rows = camera;
	for (int i = 0; i < 3; ++i)
	{
		rows[i] *= scale;
	}
	// each turn below stores its cosine as a float and keeps the sine at full precision (as the original's FPU)
	const auto turn = [](float angle) {
		return std::pair<double, double>(static_cast<float>(std::cos(static_cast<double>(angle))),
		                                 std::sin(static_cast<double>(angle)));
	};
	float c = 0.0f;
	if (_state == dude_state::k_Cling)
	{
		c = 1.0f;
	}
	else if (_state == dude_state::k_ClingArrive)
	{
		c = _stateTime;
	}
	else if (_state == dude_state::k_ClingLeave)
	{
		c = 1.0f - _stateTime;
	}
	const float smooth = Smooth(c);
	const float k = 1.0f - smooth;
	{
		// the pitch: r1' = c r1 - s r2, r2' = c r2 + s r1
		const float a = (_depth.speed * 0.3f + _data.pitchOffset) * k;
		const auto [ca, sa] = turn(a);
		affine::RotateX(rows, ca, sa);
	}
	{
		// the yaw: r0' = c r0 + s r2, r2' = c r2 - s r0 (RotateY)
		const float b = _hoverX.value * -0.8f * k;
		const auto [cb, sb] = turn(b);
		affine::RotateY(rows, cb, sb);
	}
	{
		// the roll toward the cling edge, the short way at 3 rad/s
		float target = static_cast<float>(((static_cast<int32_t>(_edge) - 2) & 3) - 2) * k_HalfPi * smooth;
		if (_roll < 0.0f)
		{
			_roll += k_TwoPi;
		}
		if (_roll > k_TwoPi)
		{
			_roll -= k_TwoPi;
		}
		if (_state != dude_state::k_Cling && _state != dude_state::k_ClingArrive)
		{
			target = 0.0f;
		}
		float diff = target - _roll;
		if (diff > k_Pi)
		{
			diff -= k_TwoPi;
		}
		if (diff < -k_Pi)
		{
			diff += k_TwoPi;
		}
		const float goal = diff + _roll;
		const float rate = dt * 3.0f;
		if (goal < _roll)
		{
			_roll -= rate;
			if (goal > _roll)
			{
				_roll = goal;
			}
		}
		if (goal > _roll)
		{
			_roll += rate;
			if (goal < _roll)
			{
				_roll = goal;
			}
		}
		// r0' = c r0 - s r1, r1' = c r1 + s r0 (RotateZ)
		const auto [cr, sr] = turn(_roll);
		affine::RotateZ(rows, cr, sr);
	}
	_rows = rows;
	_position = HoverTo3D(_hoverX.value, _hoverY.value, -_depth.value, false);

	if (_inWorld == 0.0f)
	{
		return;
	}
	// into the world, next to the target
	const float sb = Smooth(_inWorld);
	_control.worldBob += dt * 0.3f;
	const glm::vec3 axisR0 = camera[0];
	glm::vec3 t = _worldTarget.GetCurrentValue();
	t.y += _worldHeight + std::sin(_control.worldBob * 2.3f);
	t -= _worldSide * axisR0;
	// the altitude at (trunc(x 65536 0.1), trunc(z 65536 0.1)) + 3
	const float ground = (q.altitude ? q.altitude(t.x, t.z) : 0.0f) + 3.0f;
	if (t.y < ground)
	{
		t.y = ground;
	}
	for (int i = 0; i < 3; ++i)
	{
		_rows[i] = (camera[i] - _rows[i]) * sb + _rows[i];
	}
	_position = (t - _position) * sb + _position;
	const float grow = (sb * 3.0f + 1.0f) * _data.scale * _modelScale;
	affine::NormaliseRows(_rows);
	for (int i = 0; i < 3; ++i)
	{
		_rows[i] *= grow;
	}
}

void AdvisorSpirit::UpdateBasePose(bool stable, float rate, bool sfx)
{
	if (stable)
	{
		if (_stableBlend < 1.0f)
		{
			_stableBlend += rate;
			if (_stableBlend > 1.0f)
			{
				_stableBlend = 1.0f;
			}
		}
	}
	else if (_stableBlend > 0.0f)
	{
		_stableBlend -= rate;
		if (_stableBlend < 0.0f)
		{
			_stableBlend = 0.0f;
		}
	}
	const SpiritAnimClip* hover = Clip(anim::k_Hover);
	const SpiritAnimClip* hoverStable = Clip(anim::k_HoverStable);
	const auto clock = static_cast<int32_t>(_hoverClock * 1000.0f);
	if (_stableBlend > 0.0f && hoverStable != nullptr)
	{
		if (!(_stableBlend < 1.0f))
		{
			const int32_t ms = clock % hoverStable->durationMs;
			_layers.push_back({AnimLayer::Kind::Set, anim::k_HoverStable, ms});
			Sound(anim::k_HoverStable, static_cast<float>(ms) / static_cast<float>(hoverStable->durationMs), sfx);
		}
		else if (hover != nullptr)
		{
			const int32_t msHover = clock % hover->durationMs;
			const int32_t msStable = clock % hoverStable->durationMs;
			AnimLayer layer {AnimLayer::Kind::SetBlend, anim::k_Hover, msHover};
			layer.clipB = anim::k_HoverStable;
			layer.millisecondsB = msStable;
			layer.blend = _stableBlend;
			_layers.push_back(layer);
			// sic: the stable clip's time over the hover clip's duration
			Sound(anim::k_Hover, static_cast<float>(msStable) / static_cast<float>(hover->durationMs), sfx);
		}
	}
	else if (hover != nullptr)
	{
		const int32_t ms = clock % hover->durationMs;
		_layers.push_back({AnimLayer::Kind::Set, anim::k_Hover, ms});
		Sound(anim::k_Hover, static_cast<float>(ms) / static_cast<float>(hover->durationMs), sfx);
	}
	if (const SpiritAnimClip* wing = Clip(anim::k_Wingflap); wing != nullptr) // empty in both files
	{
		const int32_t ms = clock % wing->durationMs;
		_layers.push_back({AnimLayer::Kind::Add, anim::k_Wingflap, ms, 0});
		Sound(anim::k_Wingflap, static_cast<float>(ms) / static_cast<float>(wing->durationMs), sfx);
	}
	const float step = (_index == k_EvilDude ? 1.01f : 0.999811f) * rate;
	_hoverClock += step;
	_hoverClock2 += step;
}

void AdvisorSpirit::UpdateFace(float dt)
{
	float w = _emotionWeight;
	if (!(w > 0.0f))
	{
		w = 0.0f;
	}
	else if (w > 1.0f)
	{
		w = 1.0f;
	}
	const auto& neutral = _data.faces[0];
	// (openblack) guard: the original indexes the face records by the emotion without a mask; the tags and the file
	// only give 0..7
	const auto& current = _data.faces[_emotion & 7];
	for (size_t i = 0; i < _face.size(); ++i)
	{
		_face[i] = (current[i] - neutral[i]) * w + neutral[i];
	}
	// the blink: rec[0] / rec[1] the wait, rec[2] its length
	_blinkTimer += dt;
	if (_blinkTimer > _face[2])
	{
		_blinkTimer = -(_control.LocalFloatRand(_face[1] - _face[0]) + _face[0]);
		if (_control.LocalRand(5) == 0)
		{
			_blinkTimer = -0.01f; // a double blink
		}
	}
	float b = 0.0f;
	if (_blinkTimer > 0.0f)
	{
		b = (_blinkTimer + _blinkTimer) / _face[2];
		if (b > 1.0f)
		{
			b = 2.0f - b;
		}
		b *= 1.2f;
		if (b > 1.0f)
		{
			b = 1.0f;
		}
	}
	const float lidL = (1.0f - _face[9]) * b + _face[9];
	const float lidR = (1.0f - _face[10]) * b + _face[10];
	if ((_activeFlags & 8) == 0)
	{
		// the eye bones scaled by rec[3] / rec[4] (renderer), then the two lids
		// ms clamped to [0, dur - 1], reference key 0
		AddAt(anim::k_LeftEyeShut, lidL, 0);
		AddAt(anim::k_RightEyeShut, lidR, 0);
	}
	// the pupils are the renderer's (rec[5..8])
	// clips[emotion + 10], no mask (Clip's bound is the 80 slots)
	if (Clip(anim::k_FirstEmotion + _emotion) != nullptr && w > 0.0f)
	{
		AddAt(anim::k_FirstEmotion + _emotion, w, 0);
	}
}

void AdvisorSpirit::UpdateAnimStack(float dt, bool sfx)
{
	_gimme = false;
	const Queries& q = _control.GetQueries();
	// IsTalking, the sentence check, the times and the lip sync key are audio::advisor's (LipSyncFrame); so is the
	// stop of a sentence whose position is still < 0 past 0.5 s
	if (const std::optional<LipSyncFrame> lip = q.lipSync ? q.lipSync(_index) : std::nullopt; lip)
	{
		if (lip->playing)
		{
			// the vowels 7..9 at ms = clamp(trunc(dur w), 0, dur - 1), key 0, for each weight > 0
			for (uint32_t i = 0; i < 3; ++i)
			{
				if (lip->weights.at(i) > 0.0f)
				{
					AddAt(anim::k_VowelE + i, lip->weights.at(i), 0);
				}
			}
		}
		// in both cases: every tag whose time the sentence has passed (it fires while t > the tag's time and stops on <=)
		while (_nextTag < _tags.size() && lip->time > _tags[_nextTag].time)
		{
			FireTag(_tags[_nextTag], true, true);
			++_nextTag;
		}
	}

	uint32_t flags = 0;
	for (uint32_t i = 0; i < k_AnimSlots; ++i)
	{
		int32_t& mode = _slotMode[i];
		if (mode == 0)
		{
			continue;
		}
		const SpiritAnimClip* clip = Clip(i);
		float step = dt;
		float start = 0.0f;
		float end = 0.0f;
		float window = 0.0f;
		if (clip != nullptr)
		{
			step = dt / (static_cast<float>(clip->durationMs) * 0.001f);
			start = _data.loopStart[i];
			end = _data.loopEnd[i];
			window = end - start;
		}
		const float old = _slotPhase[i];
		float phase = old;
		if (mode == 1 || mode == 4) // play once
		{
			phase = old + step;
			if (phase > 1.0f)
			{
				mode = 0;
				_slotLast[i] = -1.0f;
				continue;
			}
		}
		else if (mode == 2 || mode == 3) // loop the window
		{
			if (!(window > 0.0f))
			{
				mode = 1;
				phase = old + step;
				if (phase > 1.0f)
				{
					phase = 1.0f;
				}
			}
			else if (old < end)
			{
				phase = old + step;
				if (!(phase < end))
				{
					const float x = (phase - start) / window;
					phase = (x - static_cast<float>(static_cast<int32_t>(x))) * window + start;
				}
			}
			else
			{
				mode = 4;
				phase = old + step;
				if (phase > 1.0f)
				{
					phase = 1.0f;
				}
			}
		}
		else
		{
			continue;
		}
		if (i == anim::k_GimmeFive)
		{
			// fly to the gimme-five spot
			if (_index == k_EvilDude)
			{
				SetHoverX(0.15f, 0.4f, true);
				SetHoverY(0.0f, 0.4f, true);
			}
			else
			{
				SetHoverX(-0.15f, 0.4f, true);
				SetHoverY(0.1f, 0.4f, true);
			}
			_gimme = true;
		}
		const bool skip = (_data.flags[i] & 0x20) != 0 && (_state & 0x100) != 0;
		if (!skip)
		{
			ApplyAnim(i, phase, 0.0f, false);
			Sound(i, old, sfx);
		}
		_slotPhase[i] = phase;
		flags |= static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(_data.flags[i]))); // sign-extended
	}
	_activeFlags = flags;
}

void AdvisorSpirit::FireTag(const AudioTag& tag, bool resolve, bool apply)
{
	if (resolve)
	{
		AdvisorSpirit* target = nullptr;
		AdvisorSpirit* second = nullptr;
		switch (tag.who)
		{
		case 0:
			target = this;
			break;
		case 1:
			target = partner;
			break;
		case 2:
			target = _index == k_EvilDude ? partner : this;
			break;
		case 3:
			target = _index == k_EvilDude ? this : partner;
			break;
		case 4:
			target = this;
			second = partner;
			break;
		default:
			return;
		}
		if (target != nullptr)
		{
			target->FireTag(tag, false, true);
		}
		if (second != nullptr)
		{
			second->FireTag(tag, false, true);
		}
		return;
	}
	const auto index = static_cast<uint32_t>(tag.index % static_cast<int32_t>(k_AnimSlots));
	switch (tag.action)
	{
	case 1:
		if (apply)
		{
			_slotPhase[index] = 0.0f;
			_slotMode[index] = 1;
		}
		break;
	case 2:
		if (apply)
		{
			_slotPhase[index] = 0.0f;
			_slotMode[index] = 2;
		}
		break;
	case 3:
		if (_slotMode[index] == 2 || _slotMode[index] == 3)
		{
			_slotMode[index] = 4;
		}
		break;
	case 4:
		SetEmotion(index, static_cast<float>(tag.value) * 0.01f);
		break;
	case 5:
	case 6:
		_savedLookMode = _tagLookMode;
		_tagLookMode = static_cast<int32_t>(index);
		break;
	case 7:
		_tagLookMode = _savedLookMode;
		_savedLookMode = 0;
		break;
	default:
		break;
	}
}

void AdvisorSpirit::UpdateMotion(float dt, bool focus, float zMin, bool sfx)
{
	_totalTime += dt;
	_stateTime += dt;

	// 2. the in-world blend toward (state & 8 ? the in-world target : 0) at 1/s
	const float target = (_state & 8) != 0 ? _inWorldTarget : 0.0f;
	if (target > _inWorld)
	{
		_inWorld += dt;
		if (_inWorld > target)
		{
			_inWorld = target;
		}
	}
	else if (target < _inWorld)
	{
		_inWorld -= dt;
		if (_inWorld < target)
		{
			_inWorld = target;
		}
	}

	// 3. how close the spirit comes
	const Queries& q = _control.GetQueries();
	const bool talked = q.talkedRecently ? q.talkedRecently(_index) : (q.isTalking && q.isTalking(_index));
	// four rules in order, the last that applies wins
	if (talked)
	{
		_closenessTarget = 1.0f;
		_closenessRate = 2.0f;
	}
	else
	{
		_closenessTarget = 0.0f;
		_closenessRate = 1.5f;
	}
	if ((_activeFlags & 0x40) != 0)
	{
		_closenessTarget = 0.0f;
		_closenessRate = 2.5f;
	}
	if (_gimme)
	{
		_closenessTarget = 1.0f;
		_closenessRate = 0.5f;
	}
	if (_closeness < _closenessTarget)
	{
		_closeness += dt * _closenessRate;
		if (_closeness > _closenessTarget)
		{
			_closeness = _closenessTarget;
		}
	}
	if (_closeness > _closenessTarget)
	{
		_closeness -= dt * _closenessRate;
		if (_closeness < _closenessTarget)
		{
			_closeness = _closenessTarget;
		}
	}

	// 4. the hover search and the depth spacing
	UpdateHoverPosition(_state == dude_state::k_Hover ? 1.0f : k_Third);
	if (focus)
	{
		UpdateDepthSpacing(zMin);
	}

	// 5. the hover channels (Zoomer::Update, inline in the original) and the world target
	_hoverX.Update(dt);
	_hoverY.Update(dt);
	_depth.Update(dt);
	_worldTarget.Update(dt);

	// 6. the zones; the original also fills four more records here (pending: nothing reads them)
	UpdateZones(dt, focus);
	_emotionTime += dt;

	// 7. the emotion cross-fade
	const float fade = dt * 3.0f;
	if (_emotion != _emotionTarget)
	{
		_emotionWeight -= fade;
		if (_emotionWeight < 0.0f)
		{
			_emotionWeight = -_emotionWeight;
			if (_emotionWeight > _emotionPeak)
			{
				_emotionWeight = _emotionPeak;
			}
			_emotion = _emotionTarget;
		}
	}
	else
	{
		_emotionWeight += fade;
		if (_emotionWeight > _emotionPeak)
		{
			_emotionWeight = _emotionPeak;
		}
	}
	if (!(_emotionWeight > 0.0f))
	{
		_emotionWeight = 0.0f;
		_emotion = 0;
	}
	if (_emotionTarget != 0 && _emotionTime > 1.5f)
	{
		_emotionPeak -= dt * 0.3f;
		if (_emotionPeak < 0.0f)
		{
			_emotionPeak = 0.0f;
		}
	}

	// 8. / 9. the model matrix and the in-world pose
	UpdateMatrix(dt);

	// 10. the base pose
	if (_data.clips[anim::k_Stand] != nullptr)
	{
		_layers.push_back({AnimLayer::Kind::Set, anim::k_Stand, 0});
	}
	if (_state != dude_state::k_Cling)
	{
		const bool stable = !(_state == dude_state::k_Hover && (_activeFlags & 2) == 0);
		UpdateBasePose(stable, dt * _face[11], sfx);
	}
	// 11. the face
	if ((_activeFlags & 4) == 0)
	{
		UpdateFace(dt);
	}
	// 12. the mouth, the tags and the 80 slots
	UpdateAnimStack(dt, sfx);

	// 13. the state switch
	const float shown = 1.0f - _pointBlend;
	uint32_t next = _state;
	const bool noArm = (_activeFlags & 0x10) != 0;
	// 1.0, or `shown` in a point hold with the arm; the point arm's angle is scaled by it
	float pointWeight = 1.0f;
	switch (_state)
	{
	case dude_state::k_PointIntroL:
	case dude_state::k_PointIntroR:
		if (!noArm)
		{
			ApplyAnim(_state == dude_state::k_PointIntroL ? anim::k_PointLIn : anim::k_PointRIn, shown * _stateTime * 4.0f,
			          0.0f, false);
		}
		if (_stateTime > 0.25f)
		{
			next = _state == dude_state::k_PointIntroL ? dude_state::k_PointHoldL : dude_state::k_PointHoldR;
		}
		break;
	case dude_state::k_PointHoldL:
	case dude_state::k_PointHoldR:
		if (!noArm)
		{
			ApplyAnim(_state == dude_state::k_PointHoldL ? anim::k_PointLIn : anim::k_PointRIn, shown, 0.0f, false);
			pointWeight = shown;
		}
		break;
	case dude_state::k_PointOutroL:
	case dude_state::k_PointOutroR:
		if (!noArm)
		{
			ApplyAnim(_state == dude_state::k_PointOutroL ? anim::k_PointLIn : anim::k_PointRIn,
			          (1.0f - _stateTime * 4.0f) * shown, 0.0f, false); // 4 t (as the intro)
		}
		if (_stateTime > 0.25f) // 0.25 (as the intro)
		{
			next = 0;
		}
		break;
	case dude_state::k_Avoid:
	{
		const uint32_t clip = anim::k_AvoidL + _avoidDir;
		ApplyAnim(clip, _stateTime * 1.5f, 0.0f, false);
		Sound(clip, _stateTime * 1.5f, sfx);
		if (_stateTime > 0.666666687f)
		{
			next = 0;
			_slotLast[clip] = -1.0f;
		}
		break;
	}
	case dude_state::k_FlyToAnim:
		if (_hoverX.time == _hoverX.duration && _hoverY.time == _hoverY.duration)
		{
			next = _afterFly;
		}
		break;
	case dude_state::k_Cling:
		SnapClingEdge();
		SetHoverX(_clingX, 1.0f, false);
		SetHoverY(_clingY, 1.0f, false);
		break;
	case dude_state::k_ClingArrive:
		SnapClingEdge();
		if (!(_stateTime < 1.0f))
		{
			next = dude_state::k_Cling;
		}
		SetHoverX(_clingX, 1.0f, false);
		SetHoverY(_clingY, 1.0f, false);
		break;
	case dude_state::k_ClingLeave:
		if (!(_stateTime < 1.0f))
		{
			next = 0;
		}
		SetHoverX(_clingX, 1.0f, false);
		SetHoverY(_clingY, 1.0f, false);
		break;
	case dude_state::k_ScriptedAnim:
		if (const SpiritAnimClip* clip = Clip(_scriptAnim); clip != nullptr)
		{
			const float p = _animSpeed * _stateTime * 1000.0f / static_cast<float>(clip->durationMs);
			ApplyAnim(_scriptAnim, p, 0.0f, false);
			Sound(_scriptAnim, p, sfx);
			if (p > 1.0f)
			{
				next = 0;
				_slotLast[_scriptAnim] = -1.0f;
			}
		}
		break;
	default:
		break;
	}

	// 14. the cling clip
	if ((_state & 0x100) != 0)
	{
		static constexpr std::array<uint32_t, 4> k_ClingClips = {anim::k_ClingD, anim::k_ClingL, anim::k_ClingU,
		                                                         anim::k_ClingR};
		const uint32_t c = k_ClingClips[static_cast<size_t>(_edge) & 3];
		if (const SpiritAnimClip* clip = Clip(c); clip != nullptr)
		{
			float w = 1.0f;
			if (_state == dude_state::k_ClingArrive)
			{
				w = _stateTime;
			}
			if (_state == dude_state::k_ClingLeave)
			{
				w = 1.0f - _stateTime;
			}
			const float loopEnd = _data.loopEnd[c];
			if (!clip->frames.empty() && !clip->frames[0].positions.empty())
			{
				// keep the gripping hand still: the first position channel at the loop's end minus at key 0
				auto key = static_cast<size_t>(static_cast<int32_t>(static_cast<float>(clip->frameCount) * loopEnd));
				key = std::min(key, clip->frames.size() - 1); // (openblack) the original reads past the last key
				const glm::vec3 d = (clip->frames[key].positions[0] - clip->frames[0].positions[0]) * w;
				_position -= ByRows(d, _rows);
			}
			const auto duration = static_cast<float>(clip->durationMs);
			float p = _clingClock * 1000.0f / duration;
			if (_state == dude_state::k_Cling)
			{
				if (p > loopEnd)
				{
					p = loopEnd;
					_clingClock = loopEnd * duration * 0.001f;
				}
				else
				{
					_clingClock += dt;
				}
			}
			if (_state == dude_state::k_ClingLeave)
			{
				p = (1.0f - loopEnd) * _stateTime + loopEnd;
			}
			if (p < 0.0f)
			{
				p = 0.0f;
			}
			else if (p > 1.0f)
			{
				p = 1.0f;
			}
			ApplyAnim(c, p, 0.0f, false);
			Sound(c, p, sfx);
		}
	}

	// 15. the bank lean
	{
		// the scale 0.025, the stable damping 0.8, the dead zone 0.01
		float w = _hoverX.speed / (_data.scale * _modelScale) * 0.025f * (1.0f - _stableBlend * 0.8f);
		if (std::abs(w) > 0.01f)
		{
			uint32_t clip = anim::k_HoverRight;
			if (w < 0.0f)
			{
				clip = anim::k_HoverLeft;
				w = -w;
			}
			if (w != 0.0f && _inWorld == 0.0f)
			{
				ApplyAnim(clip, w, 0.0f, false);
			}
		}
	}

	// 16. the point arm
	if ((_state & 8) != 0)
	{
		const glm::vec3 tip = q.fingertip ? q.fingertip(_index, _rows, _position) : _position;
		glm::vec2 tipHover;
		if (const auto h = WorldToHover(tip, true))
		{
			tipHover = *h;
		}
		else
		{
			tipHover = {_hoverX.value, _hoverY.value};
		}
		float dx = tipHover.x - _pointX;
		const float dy = tipHover.y - _pointY;
		uint32_t arm = anim::k_PointR;
		if ((_state & 1) != 0)
		{
			arm = anim::k_PointL;
			dx = -dx;
		}
		// atan2 at full precision, times 0.57295777918682045 (degrees / 100) rounded to a float, then times pointWeight
		// and plus 0.5, each rounded to a float
		const auto degrees = static_cast<float>(static_cast<double>(std::atan2(dy, dx)) * 0.57295777918682045);
		float p = degrees * pointWeight + 0.5f;
		if (_inWorld != 0.0f)
		{
			p = 0.2f;
			arm = anim::k_PointL;
		}
		else if (p < 0.0f)
		{
			p = 0.0f;
			SetHoverY(_pointY, 1.0f, true);
		}
		else if (p > 1.0f)
		{
			p = 1.0f;
			SetHoverY(_pointY, 1.0f, true);
		}
		const float side = _hoverX.value - _pointX;
		if (std::abs(side) < 0.16f || std::abs(side) > 0.3f)
		{
			SetHoverX(side < 0.0f ? _pointX - 0.2f : _pointX + 0.2f, 1.0f, true);
		}
		if (_pointOffScreen)
		{
			SetHoverX(_pointX, 1.0f, true);
			SetHoverY(_pointY, 1.0f, true);
			ApplyAnim(arm, (1.0f - _pointBlend) * p + _pointBlend * 0.5f, 0.5f, false);
			ApplyAnim(anim::k_PointAtCamera, _pointBlend * 0.5f, 0.0f, false);
			_pointBlend += dt * 0.5f;
			if (_pointBlend > 1.0f)
			{
				_pointBlend = 1.0f;
			}
		}
		else
		{
			ApplyAnim(arm, p, 0.5f, false);
			_pointBlend -= dt * 0.5f;
			if (_pointBlend < 0.0f)
			{
				_pointBlend = 0.0f;
			}
		}
	}

	// 17.
	SetState(next, true);
}

void AdvisorSpirit::UpdateLookTarget(float dt, bool engaged, bool partnerOut, bool talkOrPoint, const glm::vec3* lookPosition)
{
	const FrameInput& frame = _control.Frame();
	const Queries& q = _control.GetQueries();
	const int32_t halfW = frame.screen.HalfWidth();
	const int32_t halfH = frame.screen.HalfHeight();
	// the mouse in hover space by integer division: -1, 0 or 1 per axis (sic)
	const float mx = static_cast<float>((frame.mouse.x - halfW) / halfW) - _hoverX.value;
	const float my = static_cast<float>((frame.mouse.y - halfH) / halfH) - _hoverY.value;
	const float distance = std::sqrt(my * my + mx * mx);

	int32_t mode = _tagLookMode;
	int32_t alternate = 2;
	switch (_tagLookMode)
	{
	case 0:
		if (engaged)
		{
			mode = 2;
			alternate = 1;
		}
		else
		{
			mode = 1;
			alternate = 2;
		}
		if ((_state & 8) != 0)
		{
			alternate = mode;
			mode = 3;
		}
		break;
	case 2:
		alternate = 1;
		break;
	case 4:
		mode = 4;
		alternate = 4;
		break;
	default:
		break;
	}
	if (lookPosition != nullptr)
	{
		mode = 5;
		alternate = partnerOut ? 1 : 2;
	}
	// the interest in a fast mouse: speed 4, distance 0.3, Random < 0.3, 0.2 and 2, the decay 0.5 / s, the clamp
	// [0, 3]
	if (_mouseSpeed > 4.0f && distance < 0.3f && _mouseInterest < 1.0f && _control.Random(0.0f, 1.0f) < 0.3f)
	{
		_mouseInterest = (_mouseSpeed - 4.0f) * 0.2f + 2.0f;
	}
	else
	{
		_mouseInterest -= dt * 0.5f;
	}
	if (_mouseInterest < 0.0f)
	{
		_mouseInterest = 0.0f;
	}
	if (_mouseInterest > 3.0f)
	{
		_mouseInterest = 3.0f;
	}
	const bool talked = q.talkedRecently ? q.talkedRecently(_index) : (q.isTalking && q.isTalking(_index));
	if (!engaged && _mouseInterest > 1.0f && !talked)
	{
		alternate = mode;
		mode = 4;
	}
	if (talked || _state == dude_state::k_ScriptedAnim)
	{
		mode = 2;
		alternate = 2;
	}
	// the alternation: Random(2, 4) / Random(3, 6)
	_lookTimer -= dt;
	if (_lookTimer < 0.0f)
	{
		_lookToggle = !_lookToggle;
		_lookTimer += _lookToggle ? _control.Random(2.0f, 4.0f) : _control.Random(3.0f, 6.0f);
	}
	if (_lookToggle)
	{
		mode = alternate;
	}
	if ((mode == 1 && !partnerOut) || (mode == 3 && (_state & 8) == 0))
	{
		mode = 2;
	}
	_lookMode = mode;

	switch (mode)
	{
	case 1: // the partner's model point
		if (partner != nullptr)
		{
			_lookTarget = partner->_position;
		}
		break;
	case 3:
		_lookTarget = HoverTo3D(_pointX, _pointY, -_depth.value - 0.4f, false);
		break;
	case 4:
	{
		const glm::vec2 pixel(static_cast<float>(frame.mouse.x), static_cast<float>(frame.mouse.y));
		const float depth = _data.nearDepth * 0.8f;
		_lookTarget = q.pointFromScreen ? q.pointFromScreen(pixel, depth) : glm::vec3(pixel, depth);
		break;
	}
	case 5:
		if (lookPosition != nullptr)
		{
			// the look position is converted and then not used (sic): the point target instead
			_lookTarget = HoverTo3D(_pointX, _pointY, -_depth.value - 0.5f, false);
		}
		break;
	default: // 2 and anything else: the camera while talking or pointing, else nothing
		if (talkOrPoint)
		{
			_lookTarget = q.cameraPosition ? q.cameraPosition() : glm::vec3(0.0f);
		}
		else
		{
			_lookTarget.reset();
		}
		break;
	}
}

void AdvisorSpirit::UpdateHead(float dt)
{
	const Queries& q = _control.GetQueries();
	if (_lookTarget && (_activeFlags & 1) == 0)
	{
		// the head angles only, nothing when the target is behind
		const std::optional<glm::vec2> angles =
		    q.headAngles ? q.headAngles(_index, _rows, _position, *_lookTarget) : std::optional<glm::vec2>(glm::vec2(0.0f));
		if (angles)
		{
			_headTarget.x = angles->x;
			_headTarget.y = angles->y;
		}
	}
	else
	{
		_headTarget = glm::vec3(0.0f); // the three zeroed
	}
	const float rate = (_index == k_EvilDude ? 4.5f : 1.35f) * dt;
	for (int i = 0; i < 3; ++i)
	{
		float d = (_headTarget[i] - _head[i]) * 0.95f;
		if (d > rate)
		{
			d = rate;
		}
		else if (d < -rate)
		{
			d = -rate;
		}
		_head[i] += d;
	}
	if (_inWorld != 0.0f)
	{
		return;
	}
	// the look layers; the noise amplitudes rec[13] / rec[14] are 0 in both files
	auto noise = [](float x) {
		// the constants are doubles, each step rounded to a float by the FPU's precision; sin / cos are not rounded,
		// the product after them is
		const auto r = [](double v) { return static_cast<float>(v); };
		const auto xd = static_cast<double>(x);
		float sum = r(std::cos(static_cast<double>(r(r(xd * 0.95325) + 53.0))) * 0.75);
		sum = r(static_cast<double>(sum) - r(std::sin(static_cast<double>(r(r(xd * 2.2335) - 53.0))) * 0.2));
		sum = r(static_cast<double>(sum) + std::sin(static_cast<double>(r(r(xd * 0.7647) + 1.0))));
		sum = r(static_cast<double>(sum) - r(std::cos(static_cast<double>(r(22.0 - r(xd * 0.13)))) * 0.53));
		return r(static_cast<double>(sum) - r(std::sin(static_cast<double>(r(r(xd * 6.2335) - 53.0))) * 0.1));
	};
	float x = _face[13] * noise(3.0f * (_totalTime * 1.24f + _hoverClock2 * 0.95f)) + _head.x + 0.5f;
	float y = _face[14] * noise(3.0f * (_totalTime * 0.935f + _hoverClock2 + 245.0f)) + _head.y + 0.5f;
	x = std::clamp(x, 0.0f, 1.0f);
	y = std::clamp(y, 0.0f, 1.0f);
	const bool pointing = (_state & 8) != 0;
	const uint32_t clipY = pointing ? anim::k_LookUDStable : anim::k_LookUD;
	const uint32_t clipX = pointing ? anim::k_LookLRStable : anim::k_LookLR;
	if (const SpiritAnimClip* clip = Clip(clipY); clip != nullptr)
	{
		AddAt(clipY, y, clip->frameCount / 2);
	}
	if (const SpiritAnimClip* clip = Clip(clipX); clip != nullptr)
	{
		AddAt(clipX, x, clip->frameCount / 2);
	}
}

void AdvisorSpirit::UpdateDraw(int32_t frameMs, uint32_t tickMs)
{
	// the alpha byte, then the fade
	int32_t a = static_cast<int32_t>(_alpha * 255.0f);
	if (_flicker)
	{
		if (static_cast<int32_t>(tickMs) > static_cast<int32_t>(_control._flickerNext))
		{
			_control._flickerNext = tickMs + 10 + _control.LocalRand(50);
			_control._flickerMultiplier = _control.LocalRand(255) >= 64 ? 256 : 0;
		}
		a = (_control._flickerMultiplier * a) / 256;
		_flicker = false;
	}
	_alphaByte = a;
	const float seconds = static_cast<float>(frameMs) * 0.001f;
	if (_alphaTarget > _alpha)
	{
		_alpha += seconds * 3.0f;
		if (_alpha > _alphaTarget)
		{
			_alpha = _alphaTarget;
		}
	}
	else if (_alphaTarget < _alpha)
	{
		_alpha -= seconds + seconds;
		if (_alpha < _alphaTarget)
		{
			_alpha = _alphaTarget;
		}
	}
	// the puff: time unit ms x 0.0032
	if (_puffRunning)
	{
		const float u = static_cast<float>(frameMs) * 0.0032f;
		_puffTime += u;
		if (!_puffParticles)
		{
			// the first draw of a puff makes the 16 particles, each with the six Random draws in StartPuff's order
			_puffParticles.emplace();
			for (auto& particle : *_puffParticles)
			{
				RandomisePuffParticle(particle);
			}
		}
		// the fade in, 0 for <= 0, 1 for >= 1
		float fadeIn = _puffTime * 4.0f;
		if (!(fadeIn > 0.0f))
		{
			fadeIn = 0.0f;
		}
		else if (!(fadeIn < 1.0f))
		{
			fadeIn = 1.0f;
		}
		_puffFade = fadeIn;
		bool drawn = false;
		for (auto& particle : *_puffParticles)
		{
			const auto base = static_cast<int32_t>(255.0f - particle.age * 32.0f);
			particle.age += u;
			float alpha = static_cast<float>(base) * fadeIn;
			if (!(alpha > 0.0f))
			{
				alpha = 0.0f;
			}
			else if (!(alpha < 255.0f))
			{
				alpha = 255.0f;
			}
			particle.drawAlpha = static_cast<int32_t>(alpha);
			particle.drawVelocityY = particle.velocity.y;
			if (particle.drawAlpha <= 0)
			{
				continue;
			}
			drawn = true;
			// after the sprite: vy -= (spin + 1) u 0.3
			particle.velocity.y -= (particle.spin + 1.0f) * u * 0.3f;
		}
		_puffRunning = drawn;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// AdvisorSpiritController

AdvisorSpiritController::AdvisorSpiritController(const DudeData& good, const DudeData& evil, Queries queries, Screen screen)
    : _queries(std::move(queries))
{
	_frame.screen = screen;
	_dudes[k_GoodDude] = std::make_unique<AdvisorSpirit>(k_GoodDude, good, *this);
	_dudes[k_EvilDude] = std::make_unique<AdvisorSpirit>(k_EvilDude, evil, *this);
	_spirits[k_GoodDude].type = 1;
	_spirits[k_EvilDude].type = 2;
	// both at their home point, state 0
	for (int i = 0; i < k_Dudes; ++i)
	{
		_state[i] = ControlState::Home;
		_timer[i] = 0.0f;
		_dudes[i]->SetPosition(HomePoint(i));
	}
	_focus = 0;
}

uint32_t AdvisorSpiritController::LocalRand(int32_t n) const
{
	return _queries.localRand ? _queries.localRand(n) : game_random::LocalRand(n);
}

float AdvisorSpiritController::LocalFloatRand(float x) const
{
	return _queries.localFloatRand ? _queries.localFloatRand(x) : game_random::LocalFloatRand(x);
}

float AdvisorSpiritController::Random(float a, float b) const
{
	return _queries.random ? _queries.random(a, b) : game_random::crt::Random(a, b);
}

glm::ivec2 AdvisorSpiritController::Anchor(int edge) const
{
	const int32_t halfW = _frame.screen.HalfWidth();
	const int32_t halfH = _frame.screen.HalfHeight();
	switch (edge)
	{
	case 0:
		return {halfW, halfH * 3};
	case 1:
		return {-halfW, halfH};
	case 2:
		return {halfW, -halfH};
	default:
		return {halfW * 3, halfH};
	}
}

glm::ivec2 AdvisorSpiritController::HomePoint(int dude) const
{
	const int32_t w = _frame.screen.width;
	const int32_t h = _frame.screen.height;
	const AdvisorSpirit& d = *_dudes[dude];
	const auto halfW = static_cast<float>(_frame.screen.HalfWidth());
	const auto halfH = static_cast<float>(_frame.screen.HalfHeight());
	const auto px = static_cast<int32_t>((d.HoverX().value + 1.0f) * halfW);
	const auto py = static_cast<int32_t>(d.HoverY().value * halfW + halfH);
	int32_t best = 10000;
	glm::ivec2 nearest(0);
	for (int i = 0; i < 4; ++i)
	{
		const glm::ivec2 anchor = Anchor(i);
		const int32_t dx = anchor.x - px;
		const int32_t dy = anchor.y - py;
		const int32_t d2 = dx * dx + dy * dy;
		if (i == 0 || d2 < best)
		{
			best = d2;
			nearest = anchor;
		}
	}
	if (best < 0x40)
	{
		// already there: the top corner of its side
		if (dude == 0)
		{
			return {(w * 3) / 2, -(h / 2)};
		}
		return {-(w / 2), -(h / 2)};
	}
	return nearest;
}

bool AdvisorSpiritController::IsHome(int dude) const
{
	return _state[dude] == ControlState::Home || _state[dude] == ControlState::GoingHome;
}

void AdvisorSpiritController::Eject(int dude)
{
	const int32_t w = _frame.screen.width;
	const int32_t h = _frame.screen.height;
	AdvisorSpirit& d = *_dudes[dude];
	d.SetAlphaTarget(1.0f);
	if (IsHome(dude))
	{
		_state[dude] = ControlState::Out;
		_timer[dude] = 0.0f;
		d.SetPosition(HomePoint(dude));
		const int32_t x = static_cast<int32_t>(LocalRand(w / 2)) + w / 4;
		const int32_t y = static_cast<int32_t>(LocalRand(h / 2)) + h / 4;
		d.FlyTo({x, y}, 1.0f, true);
		d.SetEmotion(0, 1.0f);
	}
	else if (_state[dude] == ControlState::Clinging)
	{
		const int32_t x = static_cast<int32_t>(LocalRand(w / 3)) + w / 3;
		const int32_t y = static_cast<int32_t>(LocalRand(h / 3)) + h / 3;
		d.FlyTo({x, y}, 1.0f, true);
		d.SetEmotion(0, 1.0f);
	}
	d.ResetTrail();
}

void AdvisorSpiritController::Appear(int dude)
{
	const int32_t w = _frame.screen.width;
	const int32_t h = _frame.screen.height;
	const glm::ivec2 spot = dude == 0 ? glm::ivec2(w / 4, h / 2) : glm::ivec2((w * 3) / 4, h / 2);
	AdvisorSpirit& d = *_dudes[dude];
	if (IsHome(dude))
	{
		_state[dude] = ControlState::Out;
		_timer[dude] = 0.0f;
		d.SetPosition(spot);
		d.SetAlpha(0.0f);
		d.StartPuff();
		d.SetAlphaTarget(1.0f);
		d.SetEmotion(0, 1.0f);
		d.SetInWorld(0.0f);
		d.SetInWorldTarget(0.0f);
	}
	else if (_state[dude] == ControlState::Clinging)
	{
		d.FlyTo(spot, 1.0f, true);
		d.SetEmotion(0, 1.0f);
		d.SetInWorld(0.0f);
		d.SetInWorldTarget(0.0f);
	}
	d.ResetTrail();
}

void AdvisorSpiritController::Home(int dude)
{
	if (IsHome(dude))
	{
		return;
	}
	StopPointing(dude);
	StopLooking(dude);
	_dudes[dude]->FlyTo(HomePoint(dude), 1.0f, false);
	_state[dude] = ControlState::GoingHome;
	_timer[dude] = 0.0f;
}

void AdvisorSpiritController::Vanish(int dude)
{
	if (IsHome(dude))
	{
		return;
	}
	StopPointing(dude);
	StopLooking(dude);
	_dudes[dude]->StartPuff();
	_dudes[dude]->SetAlphaTarget(0.0f);
	_state[dude] = ControlState::GoingHome;
	_timer[dude] = 0.0f;
}

void AdvisorSpiritController::Cling(int dude, float px, float py)
{
	AdvisorSpirit& d = *_dudes[dude];
	d.SetAlphaTarget(1.0f);
	bool fromHome = false;
	if (_state[dude] != ControlState::Clinging)
	{
		fromHome = _state[dude] == ControlState::Home;
		_state[dude] = ControlState::Clinging;
		_timer[dude] = 0.0f;
		d.SetEmotion(0, 1.0f);
	}
	const auto halfW = static_cast<float>(_frame.screen.HalfWidth());
	float hx;
	float hy;
	if (px == 0.0f && py == 0.0f)
	{
		hy = 0.0f;
		hx = dude == 0 ? 1.0f : -1.0f; // the default edge: good right, evil left
	}
	else
	{
		hx = px / halfW - 1.0f;
		hy = (py - static_cast<float>(_frame.screen.HalfHeight())) / halfW;
	}
	d.Cling(hx, hy, fromHome);
}

void AdvisorSpiritController::Fly(int dude, float px, float py)
{
	if (_state[dude] == ControlState::Home)
	{
		return;
	}
	_dudes[dude]->FlyTo({static_cast<int32_t>(px), static_cast<int32_t>(py)}, 1.0f, true);
}

void AdvisorSpiritController::PlayAnim(int dude, float px, float py, uint32_t anim, float speed)
{
	if (_state[dude] == ControlState::Home)
	{
		return;
	}
	AdvisorSpirit& d = *_dudes[dude];
	if (anim == 0)
	{
		if (d.IsPlayingAnim())
		{
			d.SetState(dude_state::k_Hover, false);
		}
		return;
	}
	const auto halfW = static_cast<float>(_frame.screen.HalfWidth());
	const auto halfH = static_cast<float>(_frame.screen.HalfHeight());
	d.PlayAnim((px - halfW) / halfW, (py - halfH) / halfW, anim, speed);
}

void AdvisorSpiritController::PointAtPosition(int dude, const glm::vec3& position, bool inWorld, float side, float height)
{
	_pointMode[dude] = 1;
	_pointPosition[dude] = position;
	_pointInWorld[dude] = inWorld;
	_pointSide[dude] = side;
	_pointHeight[dude] = height;
}

void AdvisorSpiritController::PointAtPixel(int dude, glm::ivec2 pixel)
{
	_pointMode[dude] = 2;
	_pointPosition[dude].x = static_cast<float>(pixel.x);
	_pointPosition[dude].y = static_cast<float>(pixel.y);
	_pointInWorld[dude] = false;
}

void AdvisorSpiritController::LookAt(int dude, const glm::vec3& position)
{
	_lookOn[dude] = true;
	_lookPosition[dude] = position;
}

uint32_t AdvisorSpiritController::SayDelayMs(int dude) const
{
	// each step rounded to a float: fabs, minus 0.95f as a double (the float subtraction), the test against 0, plus 1,
	// times 250 (as audio::advisor::Say)
	const float v = std::abs(_dudes[dude]->HoverX().value) - 0.95f;
	if (v < 0.0f)
	{
		return 0;
	}
	const float ms = (v + 1.0f) * 250.0f;
	return static_cast<uint32_t>(ms > 500.0f ? 500.0f : ms);
}

void AdvisorSpiritController::SpiritEject(int32_t type, bool isHelp)
{
	const int dude = DudeOf(type);
	_spirits[dude].lookObject = 0;
	_spirits[dude].pointObject = 0;
	if (isHelp)
	{
		Appear(dude);
	}
	else
	{
		Eject(dude);
	}
}

void AdvisorSpiritController::SpiritHome(int32_t type, bool isHelp)
{
	const int dude = DudeOf(type);
	_spirits[dude].pointObject = 0;
	_spirits[dude].lookObject = 0;
	if (isHelp)
	{
		Vanish(dude);
	}
	else
	{
		Home(dude);
	}
}

void AdvisorSpiritController::SpiritPointPosition(int32_t type, const glm::vec3& position, bool inWorld)
{
	const int dude = DudeOf(type);
	Eject(dude);
	PointAtPosition(dude, position, inWorld, 8.0f, 5.0f);
	_spirits[dude].pointInWorld = inWorld;
	_spirits[dude].pointObject = 0;
}

void AdvisorSpiritController::SpiritPointObject(int32_t type, uint32_t object, bool inWorld)
{
	if (object == 0 || !_queries.object)
	{
		return;
	}
	const auto info = _queries.object(object);
	if (!info)
	{
		return;
	}
	const int dude = DudeOf(type);
	Eject(dude);
	glm::vec3 position = info->position;
	position.y += info->height; // the object's height
	PointAtPosition(dude, position, inWorld, 8.0f, 5.0f);
	_spirits[dude].pointInWorld = inWorld;
	_spirits[dude].pointObject = object;
}

void AdvisorSpiritController::SpiritScreenPoint(int32_t type, glm::ivec2 pixel)
{
	const int dude = DudeOf(type);
	Eject(dude);
	PointAtPixel(dude, pixel);
	_spirits[dude].pointInWorld = false;
	_spirits[dude].pointObject = 0;
}

void AdvisorSpiritController::SpiritStopPointing(int32_t type)
{
	const int dude = DudeOf(type);
	_spirits[dude].pointObject = 0;
	StopPointing(dude);
}

void AdvisorSpiritController::SpiritLookAtPosition(int32_t type, const glm::vec3& position)
{
	const int dude = DudeOf(type);
	Eject(dude);
	LookAt(dude, position);
	_spirits[dude].lookObject = 0;
}

void AdvisorSpiritController::SpiritLookObject(int32_t type, uint32_t object)
{
	if (object == 0 || !_queries.object)
	{
		return;
	}
	const auto info = _queries.object(object);
	if (!info)
	{
		return;
	}
	const int dude = DudeOf(type);
	Eject(dude);
	LookAt(dude, info->position); // no height here
	_spirits[dude].lookObject = object;
}

void AdvisorSpiritController::SpiritStopLooking(int32_t type)
{
	const int dude = DudeOf(type);
	_spirits[dude].lookObject = 0;
	StopLooking(dude);
}

void AdvisorSpiritController::SpiritPlayAnim(int32_t type, float x, float y, uint32_t anim, float speed)
{
	const int dude = DudeOf(type);
	Eject(dude);
	PlayAnim(dude, x * static_cast<float>(_frame.screen.width), y * static_cast<float>(_frame.screen.height), anim, speed);
}

bool AdvisorSpiritController::SpiritPlayingAnim(int32_t type) const
{
	return _dudes[DudeOf(type)]->IsPlayingAnim();
}

void AdvisorSpiritController::SpiritCling(int32_t type, float x, float y)
{
	// no eject
	Cling(DudeOf(type), x * static_cast<float>(_frame.screen.width), y * static_cast<float>(_frame.screen.height));
}

void AdvisorSpiritController::SpiritFly(int32_t type, float x, float y)
{
	Fly(DudeOf(type), x * static_cast<float>(_frame.screen.width), y * static_cast<float>(_frame.screen.height));
}

void AdvisorSpiritController::ProcessTurn()
{
	// one spirit after the other: the help system takes the evil one (dude 1) first, then the good one (dude 0)
	for (int dude = k_Dudes - 1; dude >= 0; --dude)
	{
		Spirit& spirit = _spirits[dude];
		if (spirit.pointObject != 0)
		{
			const auto info = _queries.object ? _queries.object(spirit.pointObject) : std::nullopt;
			if (!info)
			{
				SpiritStopPointing(spirit.type);
			}
			else
			{
				glm::vec3 position = info->position;
				position.y += info->height;
				PointAtPosition(dude, position, spirit.pointInWorld, 8.0f, 5.0f);
			}
		}
		if (spirit.lookObject != 0) // re-sent through the point function (original bug, kept)
		{
			const auto info = _queries.object ? _queries.object(spirit.lookObject) : std::nullopt;
			if (!info)
			{
				SpiritStopLooking(spirit.type);
			}
			else
			{
				PointAtPosition(dude, info->position, false, 8.0f, 5.0f);
			}
		}
	}
}

void AdvisorSpiritController::SetSentenceTags(int dude, std::vector<AudioTag> tags)
{
	_dudes[dude]->_tags = std::move(tags);
	_dudes[dude]->_nextTag = 0;
}

void AdvisorSpiritController::Process(float dt, float focusBias)
{
	auto talked = [this](int dude) {
		if (_queries.talkedRecently)
		{
			return _queries.talkedRecently(dude);
		}
		return _queries.isTalking && _queries.isTalking(dude);
	};
	int focus = _pointMode[0] != 0 ? 1 : 0;
	if (_pointMode[1] != 0)
	{
		focus |= 2;
	}
	if (talked(0))
	{
		focus |= 1;
	}
	if (talked(1))
	{
		focus |= 2;
	}
	if (focus == 0 || focus == 3)
	{
		focus = focusBias < 0.5f ? 0 : 1;
	}
	else
	{
		focus -= 1;
	}
	_focus = focus;
	// the model scales 0.8 e^(-+0.5 (bias - 0.5)): 0.841 / 0.761 for the constant 0.4. Each step rounded to a float:
	// bias - 0.5f, x -0.5f / x 0.5f, the inline exp (ExpSinglePrecision), times the double 0.80000001192092896
	const float bias = focusBias - 0.5f;
	_dudes[0]->SetModelScale(
	    static_cast<float>(static_cast<double>(gutils::ExpSinglePrecision(bias * -0.5f)) * 0.80000001192092896));
	_dudes[1]->SetModelScale(
	    static_cast<float>(static_cast<double>(gutils::ExpSinglePrecision(bias * 0.5f)) * 0.80000001192092896));

	for (int i = 0; i < k_Dudes; ++i)
	{
		AdvisorSpirit& d = *_dudes[i];
		d._layers.clear();
		d._sounds.clear();
		_timer[i] += dt;
		if (_state[i] == ControlState::GoingHome)
		{
			if (_timer[i] > 1.0f && !d.PuffRunning())
			{
				_state[i] = ControlState::Home;
				d.SetPosition(HomePoint(i));
				d.ClearAnims();
			}
			else if (!d.PuffRunning())
			{
				d.FlyTo(HomePoint(i), 1.0f, false);
			}
		}
		if (_state[i] != ControlState::Home)
		{
			if (_pointMode[i] == 1)
			{
				d.PointAt(_pointPosition[i], _pointInWorld[i], _pointSide[i], _pointHeight[i]);
			}
			else if (_pointMode[i] == 2)
			{
				d.ScreenPoint({static_cast<int32_t>(_pointPosition[i].x), static_cast<int32_t>(_pointPosition[i].y)});
			}
			else if ((d.State() & 8) != 0 && (d.QueuedState() & 0x100) == 0)
			{
				d.SetState(dude_state::k_Hover, false);
			}
			// the rest zone: good right, evil left
			d._zones[0] = {2.0f, i == 0 ? 0.66f : -0.66f, 0.0f, 0.4f, 0.6f};
			d.partner = _dudes[i == 0 ? 1 : 0].get();
			d.UpdateMotion(dt, focus == i, 0.0f, true);
		}
		else if (d.State() != dude_state::k_Hover)
		{
			// UpdateMotion and UpdateHead
			d.UpdateMotion(dt, focus == i, 0.0f, true);
			d.UpdateHead(dt);
		}
		// else only the sentence update (audio::advisor)
	}
	for (int i = 0; i < k_Dudes; ++i)
	{
		if (_state[i] == ControlState::Home)
		{
			continue;
		}
		const bool talking = (_queries.sayActive && _queries.sayActive(i)) && (_queries.isTalking && _queries.isTalking(i));
		const glm::vec3* lookPosition = _lookOn[i] ? &_lookPosition[i] : nullptr;
		const bool talkOrPoint = talking || _pointMode[i] != 0;
		const bool engaged = talking || (focus == i && _pointMode[i] != 0);
		const bool partnerOut = _state[i == 0 ? 1 : 0] != ControlState::Home;
		_dudes[i]->UpdateLookTarget(dt, engaged, partnerOut, talkOrPoint, lookPosition);
		_dudes[i]->UpdateHead(dt);
	}
}

void AdvisorSpiritController::Update(const FrameInput& input)
{
	_frame = input;
	Process(input.dt, 0.4f);
	// the draws: the overlay (blend < 0.5) and the 3D draw (>= 0.5) for dudes out of home
	for (int i = 0; i < k_Dudes; ++i)
	{
		if (_state[i] != ControlState::Home)
		{
			_dudes[i]->UpdateDraw(input.frameMs, input.tickMs);
		}
	}
}

} // namespace openblack::help::spirits
