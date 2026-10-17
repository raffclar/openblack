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
	gl_FragColor =
	    MarchClouds(u_cloudEye.xyz, dir, 1e9f, jitter, 40, false, u_cloudAmbient.rgb * 1.3f, u_cloudAmbient.rgb * 1.6f, depth);
}
