$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "clouds.sh"

// EXPERIMENT (not for merge): the clouds in one face of the cube of the far scene round the eye, which the water
// reflects. The face is drawn once in some frames, so fewer samples and no detail are enough. Blended over the face's
// sky as rgb + the sky times a.

void main()
{
	vec2 uv = gl_FragCoord.xy * u_viewTexel.xy;
	vec3 dir = CloudRayAt(uv);
	float jitter = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898f, 78.233f))) * 43758.547f);
	float depth;
	vec4 clouds = MarchClouds(u_cloudEye.xyz, dir, 1e9f, jitter, 64, false, u_cloudAmbient.rgb * 1.3f, depth);
	// The haze hides far clouds: more of the face's sky behind them shows
	float haze = CloudHaze(u_cloudEye.xyz, dir, depth);
	gl_FragColor = vec4(clouds.rgb * (1.0f - haze), mix(clouds.a, 1.0f, haze));
}
