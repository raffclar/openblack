$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "clouds.sh"

// EXPERIMENT (not for merge): the clouds of this frame blended with those of the frames before. Each pixel finds where
// its cloud was on the screen in the last frame and takes most of the colour from there. The colour from there is
// held inside the range of this frame's pixels round it, so that no old cloud trails behind a moving one.

SAMPLER2D(s_cloudCurrent, 3);
SAMPLER2D(s_cloudCurrentDepth, 4);
SAMPLER2D(s_cloudHistory, 5);

// The view-projection of the last frame
uniform mat4 u_cloudPreviousViewProj;
// x: 1 while the history holds the clouds of the last frame; y: how much of the history to keep; zw: how far the wind
// moved the clouds since the last frame, across the ground
uniform vec4 u_cloudResolve;

void main()
{
	vec2 uv = gl_FragCoord.xy * u_viewTexel.xy;
	vec4 current = texture2DLod(s_cloudCurrent, uv, 0.0f);
	float depth = texture2DLod(s_cloudCurrentDepth, uv, 0.0f).r;

	// The range of the pixels round this one in this frame
	vec4 low = current;
	vec4 high = current;
	for (int y = -1; y <= 1; ++y)
	{
		for (int x = -1; x <= 1; ++x)
		{
			vec4 near = texture2DLod(s_cloudCurrent, uv + vec2(float(x), float(y)) * u_viewTexel.xy, 0.0f);
			low = min(low, near);
			high = max(high, near);
		}
	}

	vec4 result = current;
	if (u_cloudResolve.x > 0.5f)
	{
		// Where the cloud was in the last frame: the wind has moved it on since then
		vec3 world = u_cloudEye.xyz + CloudRayAt(uv) * depth - vec3(u_cloudResolve.z, 0.0f, u_cloudResolve.w);
		vec3 before = CloudScreenAt(u_cloudPreviousViewProj, world);
		if (before.z > 0.0f && before.x > 0.0f && before.y > 0.0f && before.x < 1.0f && before.y < 1.0f)
		{
			// A little room round the range keeps the slow change of the jitter, which the range of one frame lacks
			vec4 margin = (high - low) * 0.5f + 0.02f;
			vec4 history = clamp(texture2DLod(s_cloudHistory, before.xy, 0.0f), low - margin, high + margin);
			result = mix(current, history, u_cloudResolve.y);
		}
	}
	gl_FragColor = result;
}
