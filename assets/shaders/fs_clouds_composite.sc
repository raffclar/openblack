$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "clouds.sh"

// EXPERIMENT (not for merge): the clouds over the scene at the full size of the screen. Of the four pixels of the half
// size round each pixel, those whose scene is as far as this pixel's count the most, so that no cloud spills over the
// edge of a near hill. The shafts of the sun's light come through the gaps in the clouds. Blended as rgb + the scene
// times a.

SAMPLER2D(s_cloudHistory, 3);
SAMPLER2D(s_cloudCurrentDepth, 4);
SAMPLER2D(s_sceneDepth, 5);

// xy: the size of the clouds' target in pixels; z: how strong the shafts of light are; w: unused
uniform vec4 u_cloudComposite;

// How far a ray goes before the scene, or the water when the eye is above it
float RayLimit(vec2 uv, vec3 dir)
{
	float scene = CloudSceneDistance(uv, texture2DLod(s_sceneDepth, uv, 0.0f).r);
	if (u_cloudEye.w > 0.5f && dir.y < 0.0f)
	{
		scene = min(scene, (u_cloudEye.y - u_cloudLayer.w) / -dir.y);
	}
	return min(scene, 60000.0f);
}

void main()
{
	vec2 uv = gl_FragCoord.xy * u_viewTexel.xy;
	vec3 dir = CloudRayAt(uv);
	float here = RayLimit(uv, dir);

	// The four pixels of the half size round this one, weighted by place and by how near their scene is to this one's
	vec2 size = u_cloudComposite.xy;
	vec2 texel = uv * size - 0.5f;
	vec2 base = floor(texel);
	vec2 f = texel - base;
	vec4 sum = vec4_splat(0.0f);
	float weights = 0.0f;
	for (int y = 0; y < 2; ++y)
	{
		for (int x = 0; x < 2; ++x)
		{
			vec2 offset = vec2(float(x), float(y));
			vec2 at = (base + offset + 0.5f) / size;
			float place = (x == 0 ? 1.0f - f.x : f.x) * (y == 0 ? 1.0f - f.y : f.y);
			float scene = min(texture2DLod(s_cloudCurrentDepth, at, 0.0f).g, 60000.0f);
			float alike = 1.0f / (0.02f + abs(scene - here) / max(here, 1.0f));
			float weight = place * alike + 1e-5f;
			sum += texture2DLod(s_cloudHistory, at, 0.0f) * weight;
			weights += weight;
		}
	}
	vec4 clouds = sum / weights;
	// A scene nearer than the lowest cloud has no cloud in front of it: the half size's pixels round a near hill's edge
	// can hold the sky's clouds, which would show as a bright line along the hill
	if (here < CloudNearestAlong(dir) && !CloudOnDome(dir, here))
	{
		clouds = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	// The shafts of light: along the line to the sun on the screen, the share of the samples where the sky shows through
	// the clouds
	vec3 shafts = vec3_splat(0.0f);
	vec3 sunAt = CloudScreenAt(u_viewProj, u_cloudEye.xyz + u_cloudLight.xyz * 100000.0f);
	if (u_cloudComposite.z > 0.0f && sunAt.z > 0.0f && u_cloudLight.y > -0.05f)
	{
		vec2 toSun = sunAt.xy - uv;
		float reach = length(toSun * vec2(u_viewTexel.y / u_viewTexel.x, 1.0f));
		float jitter = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898f, 78.233f))) * 43758.547f);
		float open = 0.0f;
		float decay = 1.0f;
		float total = 0.0f;
		for (int i = 0; i < 16; ++i)
		{
			vec2 at = uv + toSun * ((float(i) + jitter) / 16.0f) * min(1.0f, 0.7f / max(reach, 1e-3f));
			if (at.x > 0.0f && at.y > 0.0f && at.x < 1.0f && at.y < 1.0f)
			{
				float sky = step(50000.0f, texture2DLod(s_cloudCurrentDepth, at, 0.0f).g);
				open += texture2DLod(s_cloudHistory, at, 0.0f).a * sky * decay;
			}
			total += decay;
			decay *= 0.94f;
		}
		open /= total;
		float glow = exp(-reach * 3.2f);
		shafts = u_cloudLightColour.rgb * u_cloudLightColour.w * open * glow * u_cloudComposite.z * min(1.0f, here / 2500.0f);
	}
	gl_FragColor = vec4(clouds.rgb + shafts * clouds.a, clouds.a);
}
