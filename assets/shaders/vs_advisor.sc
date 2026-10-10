$input a_position, a_texcoord0, a_normal, a_indices
$output v_position, v_texcoord0, v_color0, v_haze

// The advisors, skinned by their bones' matrices, which are already in the world
#ifndef BGFX_CONFIG_MAX_BONES
#if BGFX_SHADER_LANGUAGE_HLSL == 3
#define BGFX_CONFIG_MAX_BONES 48
#else
#define BGFX_CONFIG_MAX_BONES 128
#endif
#endif // BGFX_CONFIG_MAX_BONES

#include <bgfx_shader.sh>

#include "model_light.sh"

uniform vec4 u_islandExtent;
#include "land_light.sh"

// x: how far out in the world the advisor is, of 255, by which the land's light where it is colours it; yz: where the
// land's light is taken, across x and z; w: its alpha, of 1
uniform vec4 u_advisorLook;
// Each eye's pupil: x its bone, yz how far its texture moves across and down with the vertices, w 1 when the face has
// set them
uniform vec4 u_advisorPupil0;
uniform vec4 u_advisorPupil1;
// xy: the pupils' texture centre, z: 1 when their down runs along the eye's z rather than its y
uniform vec4 u_advisorPupilCentre;

void main()
{
	uint bone = uint(max(0, a_indices.x));
	mat4 model = u_model[bone];
	v_position = mul(model, vec4(a_position.xyz, 1.0f));
	gl_Position = mul(u_viewProj, v_position);

	// The pupils' texture moves over the eyes' vertices, read off where they are on their eye
	v_texcoord0 = vec4(a_texcoord0, 0.0f, 0.0f);
	float boneIndex = float(bone);
	vec4 pupil = boneIndex == u_advisorPupil0.x ? u_advisorPupil0 : (boneIndex == u_advisorPupil1.x ? u_advisorPupil1 : vec4_splat(0.0f));
	if (pupil.w > 0.5f && (boneIndex == u_advisorPupil0.x || boneIndex == u_advisorPupil1.x))
	{
		float down = u_advisorPupilCentre.z > 0.5f ? a_position.z : a_position.y;
		v_texcoord0.xy = vec2((a_position.x * 10.0f) * pupil.y + u_advisorPupilCentre.x, (down * 10.0f) * pupil.z + u_advisorPupilCentre.y);
	}

	// The light, from the bone's origin, meets the vertex's normal in the bone's own space
	vec3 origin = mul(model, vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
	vec3 localLight = ModelLightLocal(mul(model, vec4(1.0f, 0.0f, 0.0f, 0.0f)).xyz, mul(model, vec4(0.0f, 1.0f, 0.0f, 0.0f)).xyz,
	                                  mul(model, vec4(0.0f, 0.0f, 1.0f, 0.0f)).xyz, origin);
	float facing = dot(a_normal, localLight);
	float lit = min((facing < 0.0f ? 0.0f : facing * 1.3f) + 0.4f, 1.0f);
	float intensity = floor(lit * 255.0f);

	// Out in the world it takes the land's light, white near the screen, and the land's colour added
	float shade = u_advisorLook.x;
	vec3 colour = vec3_splat(255.0f);
	vec3 added = vec3_splat(0.0f);
	if (shade > 0.0f)
	{
		colour = vec3_splat(255.0f) + floor((LandLightAt(u_advisorLook.yz) - 255.0f) * shade / 256.0f);
		added = floor(LandColourAt(u_advisorLook.yz) * shade / 256.0f);
	}
	v_color0 = vec4(floor(colour * intensity / 256.0f) / 255.0f, u_advisorLook.w);
	v_haze = vec4(added / 255.0f, 0.0f);
}
