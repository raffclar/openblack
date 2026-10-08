#ifdef USE_LIGHTMAP
$input a_position, a_texcoord0, a_texcoord1, i_data0, i_data1, i_data2, i_data3
#else
$input a_position, a_texcoord0, a_normal, i_data0, i_data1, i_data2, i_data3
#endif // USE_LIGHTMAP
$output v_texcoord0, v_color0

// The temple's rooms and what the renderer draws in them (src/Graphics/RendererTemple.cpp). Their meshes have no
// bones: a door's leaf turns by the one model matrix of its draw.
#define BGFX_CONFIG_MAX_BONES 1

#include <bgfx_shader.sh>

#ifndef USE_LIGHTMAP
// ModelLightLocal / ModelLightI / ModelLightFactor / ModelLightDiffuse and u_modelLight: the GPU side of
// src/Graphics/ModelLight.h
#include "model_light.sh"
#endif // USE_LIGHTMAP

// x: how far back, as a fraction of its depth, the mesh is pushed towards the far plane at 0: the temple's rooms other
// than the one the player is in, which overlap it at the doorways
uniform vec4 u_depthBias;
// xy: how far the texture has slid across the mesh, as the creature's room's waterfall does
uniform vec4 u_uvOffset;

void main()
{
	mat4 model;
	model[0] = vec4(i_data0.xyz, 0.0f);
	model[1] = vec4(i_data1.xyz, 0.0f);
	model[2] = vec4(i_data2.xyz, 0.0f);
	model[3] = vec4(i_data3.xyz, 1.0f);
	vec4 world = instMul(model, mul(u_model[0], vec4(a_position.xyz, 1.0f)));

#ifdef USE_LIGHTMAP
	// Lit by its lightmap alone, read with the second texture coordinates
	v_texcoord0 = vec4(a_texcoord0 + u_uvOffset.xy, a_texcoord1.xy);
	v_color0 = vec4_splat(1.0f);
#else
	// A white object shaded by the game's one light, from the origin of the mesh, against the vertex's own normal
	vec3 axisX = instMul(model, vec4(mul(u_model[0], vec4(1.0f, 0.0f, 0.0f, 0.0f)).xyz, 0.0f)).xyz;
	vec3 axisY = instMul(model, vec4(mul(u_model[0], vec4(0.0f, 1.0f, 0.0f, 0.0f)).xyz, 0.0f)).xyz;
	vec3 axisZ = instMul(model, vec4(mul(u_model[0], vec4(0.0f, 0.0f, 1.0f, 0.0f)).xyz, 0.0f)).xyz;
	vec3 origin = instMul(model, mul(u_model[0], vec4(0.0f, 0.0f, 0.0f, 1.0f))).xyz;
	vec3 lightLocal = ModelLightLocal(axisX, axisY, axisZ, origin, u_modelLight.xyz);
	float factor = ModelLightFactor(ModelLightI(a_normal.xyz, lightLocal, false), u_modelLight.w);
	v_texcoord0 = vec4(a_texcoord0 + u_uvOffset.xy, 0.0f, 0.0f);
	v_color0 = vec4(ModelLightDiffuse(vec3_splat(255.0f), factor) / 255.0f, 1.0f);
#endif // USE_LIGHTMAP

	gl_Position = mul(u_viewProj, world);
	gl_Position.z *= 1.0f - u_depthBias.x;
}
