#include "ScriptingBindingUtils.h"

#include "3D/LandIsland.h"
#include "Game.h"

namespace openblack::lhscriptx
{

template <>
ParameterType ParameterTypeStaticLookUp<glm::vec3> = ParameterType::Vector;

template <>
ParameterType ParameterTypeStaticLookUp<const std::string&> = ParameterType::String;

template <>
ParameterType ParameterTypeStaticLookUp<float> = ParameterType::Float;

template <>
ParameterType ParameterTypeStaticLookUp<int32_t> = ParameterType::Number;

glm::vec2 GetHorizontalPosition(const std::string& str)
{
	const auto pos = str.find_first_of(',');
	const auto y = std::stof(str.substr(pos + 1));
	const auto x = std::stof(str.substr(0, pos));
	return glm::vec2(x, y);
}

template <>
glm::vec3 GetParamValue(const ScriptCommandContext& ctx, int index)
{
	const auto& param = ctx[index];
	const auto& island = ctx.GetGame().GetLandIsland();
	const auto pos = GetHorizontalPosition(param.GetString());
	return {pos.x, island.GetHeightAt(pos), pos.y};
}

} // namespace openblack::lhscriptx
