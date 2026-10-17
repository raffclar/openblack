$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "atmosphere.sh"
#include "air_sky.sh"

// EXPERIMENT (not for merge): the aurora at half the screen's size, before the sky's pass. Each pixel marches the ray
// through its middle to the planet (AirAurora). The sky and the haze over the sea then read it with a bilinear filter.
// The pass does not read the scene's depth, so it has no edges of the scene in it: the land hides the sky at the full
// size of the screen.

void main()
{
	vec2 uv = gl_FragCoord.xy * u_viewTexel.xy;
	vec3 eye = mul(u_invView, vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
	vec3 dir = ViewRayAt(uv);
	gl_FragColor = vec4(AirAurora(eye, dir, 1e9f, gl_FragCoord.xy, AIR_AURORA_STEPS, true), 1.0f);
}
