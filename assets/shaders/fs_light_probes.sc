$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "clouds_shadow.sh"
#include "lighting.sh"
#include "sun_visibility.sh"

// EXPERIMENT (not for merge): one band of rows of the grid of light probes (see global_illumination). Each probe stands
// a little over the ground or the sea and sends 64 rays against the land's heights only. A ray that hits the land
// brings back the light of the ground there: its colour, lit by the sun or the moon through the hills' and the clouds'
// shadows and by the open part of the sky. A ray that hits the sea brings back a little of the sky that the water
// reflects and of the light under its surface. A ray that hits nothing brings back nothing: the sky's light is added
// apart. The texel keeps one colour's four harmonics of that light.

// The shore map: w is the height of the ground at each corner of the land
SAMPLER2D(s_shore, 0);
// The land's block textures and the layer of each block (as the post pass reads them)
SAMPLER2DARRAY(s_blockTextures, 1);
SAMPLER2D(s_blockLayers, 2);

// rgb: the light of the sun or the moon at the ground, for a light of strength 1 at the top of the air; w: unused
uniform vec4 u_lightSun;
// xyz: the direction towards the sun or the moon; w: the sea's level
uniform vec4 u_lightTowards;

float GroundAt(vec2 xz)
{
	return texture2DLod(s_shore, (xz / 10.0f + 0.5f) / 513.0f, 0.0f).w;
}

vec3 GroundNormal(vec2 xz)
{
	float dx = GroundAt(xz + vec2(10.0f, 0.0f)) - GroundAt(xz - vec2(10.0f, 0.0f));
	float dz = GroundAt(xz + vec2(0.0f, 10.0f)) - GroundAt(xz - vec2(0.0f, 10.0f));
	return normalize(vec3(-dx, 20.0f, -dz));
}

// The colour of the land at a point, from its block's texture at a coarse level, in linear light; grey where there is
// no block
vec3 GroundAlbedo(vec2 xz)
{
	vec2 block = floor(xz / 160.0f);
	if (block.x < 0.0f || block.y < 0.0f || block.x > 31.0f || block.y > 31.0f)
	{
		return vec3_splat(0.25f);
	}
	float layer = texture2DLod(s_blockLayers, (block + 0.5f) / 32.0f, 0.0f).r;
	if (layer < 0.5f)
	{
		return vec3_splat(0.25f);
	}
	vec2 inside = (xz - block * 160.0f).yx / 160.0f;
	vec3 colour = texture2DArrayLod(s_blockTextures, vec3(inside, layer - 1.0f), 4.0f).rgb;
	return pow(max(colour, vec3_splat(0.0f)), vec3_splat(2.2f));
}

void main()
{
	vec2 texel = floor(gl_FragCoord.xy);
	float side = u_lightParams.w;
	float channel = floor(texel.x / side);
	vec2 probe = vec2(texel.x - channel * side, texel.y);
	vec2 centre = (probe + 0.5f) * u_lightParams.z;
	float sea = u_lightTowards.w;
	vec3 origin = vec3(centre.x, max(GroundAt(centre), sea) + 3.0f, centre.y);
	vec3 towardsLight = u_lightTowards.xyz;

	vec4 sh = vec4_splat(0.0f);
	float golden = LIGHT_PI * (3.0f - sqrt(5.0f));
	for (int i = 0; i < 64; ++i)
	{
		float y = 1.0f - (2.0f * float(i) + 1.0f) / 64.0f;
		float r = sqrt(max(1.0f - y * y, 0.0f));
		float angle = golden * float(i);
		vec3 dir = vec3(r * cos(angle), y, r * sin(angle));

		// March out along the ray in growing steps, then halve the last step three times
		float before = 0.0f;
		float stride = 4.0f;
		float t = 2.0f;
		bool hit = false;
		for (int s = 0; s < 16; ++s)
		{
			vec3 p = origin + dir * t;
			if (p.y < max(GroundAt(p.xz), sea))
			{
				hit = true;
				break;
			}
			before = t;
			t += stride;
			stride *= 1.35f;
		}
		if (!hit)
		{
			continue;
		}
		float after = t;
		for (int b = 0; b < 3; ++b)
		{
			float middle = 0.5f * (before + after);
			vec3 p = origin + dir * middle;
			if (p.y < max(GroundAt(p.xz), sea))
			{
				after = middle;
			}
			else
			{
				before = middle;
			}
		}
		vec3 q = origin + dir * after;
		float ground = GroundAt(q.xz);
		vec3 radiance;
		if (ground > sea + 0.2f)
		{
			vec3 n = GroundNormal(q.xz);
			float sun = max(dot(n, towardsLight), 0.0f) * SunVisibilityAt(q.xz) * CloudShadeAt(q.xz);
			vec3 lit = u_lightSun.rgb * sun + LightSkyAt(n) * SkyOpennessAt(q.xz);
			radiance = GroundAlbedo(q.xz) / LIGHT_PI * lit;
		}
		else
		{
			// The sea: the sky that it reflects, and a little of the light under its surface, sea green
			vec3 reflected = reflect(dir, vec3(0.0f, 1.0f, 0.0f));
			vec3 sky = LightRadiance(u_lightSky[0], u_lightSky[1], u_lightSky[2], reflected);
			float sun = max(towardsLight.y, 0.0f) * CloudShadeAt(q.xz);
			radiance = sky * 0.06f + u_lightSun.rgb * sun * vec3(0.01f, 0.03f, 0.03f);
		}
		vec4 basis = vec4(LIGHT_Y0, LIGHT_Y1 * dir.y, LIGHT_Y1 * dir.z, LIGHT_Y1 * dir.x) * (4.0f * LIGHT_PI / 64.0f);
		float value = channel < 0.5f ? radiance.r : (channel < 1.5f ? radiance.g : radiance.b);
		sh += basis * value;
	}
	gl_FragColor = sh;
}
