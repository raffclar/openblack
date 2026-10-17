$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "clouds.sh"

// EXPERIMENT (not for merge): the rays through the clouds, at half the screen's size each way. Each ray stops at the
// scene, and at the water when the eye is above it. The blue noise moves the start of each ray a little, other each
// frame, so that the next pass can blend the frames into a smooth result.
// Target 0: rgb the light that the clouds scatter towards the eye, a how much of the light behind them goes through.
// Target 1: x where the cloud is along the ray, y how far the scene is.

SAMPLER2D(s_cloudBlueNoise, 3);
SAMPLER2D(s_sceneDepth, 4);
// The sky alone round the eye (the cube that the water reflects), for the light of the sky on the clouds
SAMPLERCUBE(s_cloudSky, 5);

// xy: the size of the full screen in pixels; z: 1 while the cube of the sky is ready; w: unused
uniform vec4 u_cloudScreen;

// The light of the sky from above: the mean of the cube of the sky over the upper half, or the haze's colour before
// the cube is ready
vec3 SkyLight()
{
	if (u_cloudScreen.z < 0.5f)
	{
		return u_cloudAmbient.rgb * 1.3f;
	}
	vec3 sum = textureCubeLod(s_cloudSky, vec3(0.0f, 1.0f, 0.0f), 0.0f).rgb * 2.0f;
	for (int i = 0; i < 8; ++i)
	{
		float angle = float(i) * 0.785398f;
		sum += textureCubeLod(s_cloudSky, vec3(cos(angle) * 0.8f, 0.6f, sin(angle) * 0.8f), 0.0f).rgb;
	}
	// Never darker than the haze: the cube holds the clouds too, which would darken their own light
	return max(sum / 10.0f, u_cloudAmbient.rgb * 1.1f);
}

void main()
{
	vec2 uv = gl_FragCoord.xy * u_viewTexel.xy;
	vec3 eye = u_cloudEye.xyz;
	vec3 dir = CloudRayAt(uv);

	// The nearest scene of the four pixels of the full screen under this one, so that the cloud never shows in front of
	// a near hill at its edge
	vec2 pixel = 1.0f / u_cloudScreen.xy;
	vec2 corner = (floor(gl_FragCoord.xy) * 2.0f + 0.5f) * pixel;
	float nearest = max(max(texture2DLod(s_sceneDepth, corner, 0.0f).r, texture2DLod(s_sceneDepth, corner + vec2(pixel.x, 0.0f), 0.0f).r),
	                    max(texture2DLod(s_sceneDepth, corner + vec2(0.0f, pixel.y), 0.0f).r,
	                        texture2DLod(s_sceneDepth, corner + pixel, 0.0f).r));
	float scene = CloudSceneDistance(uv, nearest);
	float limit = scene;
	// Above the water, the water's surface hides what is under it
	if (u_cloudEye.w > 0.5f && dir.y < 0.0f)
	{
		limit = min(limit, (eye.y - u_cloudLayer.w) / -dir.y);
	}

	// Blue noise, moved on each frame by the golden ratio, so that each pixel samples other places over a few frames
	vec2 noiseUv = (gl_FragCoord.xy + 0.5f) / 64.0f;
	float jitter = fract(texture2DLod(s_cloudBlueNoise, noiseUv, 0.0f).r + u_cloudDrift.w * 0.61803398875f);

	float depth;
	// More samples for the long rays near the horizon
	int steps = int(mix(96.0f, 56.0f, clamp(abs(dir.y) * 3.0f, 0.0f, 1.0f)));
	vec3 behind = u_cloudScreen.z > 0.5f ? textureCubeLod(s_cloudSky, dir, 0.0f).rgb : u_cloudAmbient.rgb * 1.6f;
	vec4 clouds = MarchClouds(eye, dir, limit, jitter, steps, true, SkyLight(), behind, depth);
	gl_FragData[0] = clouds;
	gl_FragData[1] = vec4(depth, min(scene, 60000.0f), 0.0f, 1.0f);
}
