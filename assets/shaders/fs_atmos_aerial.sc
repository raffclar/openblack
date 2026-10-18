$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#define ATMOS_MULTIPLE_TABLE
#include "atmosphere.sh"
#include "clouds_shadow.sh"
#include "sun_visibility.sh"
#include "world_curve.sh"

// EXPERIMENT (not for merge): the haze's grid that follows the view, 32 cells across and down and 32 slices into the
// view (see atmosphere::HazeAlong). Each texel sums the light that the air scatters towards the eye from the eye to its
// slice's distance, at atmosphere::HazeScaleAt times the real air's density. The light of the sun or the moon reaches
// each point through the hills' shadow and the clouds' shadow. rgb: the light that the haze adds, for a light of
// strength 1; a: the part of the scene behind that shows through.

// x: 1 while the hills' and the clouds' shadows dim the light; y: the steps along each ray; zw: unused
uniform vec4 u_atmosAerial;
// xyz: the direction towards the game's own sun or moon, where it shows in the sky. The haze glows round it.
uniform vec4 u_atmosGameLight;

vec3 WorldAt(vec2 uv, float depth)
{
#if BGFX_SHADER_LANGUAGE_GLSL
	vec3 ndc = vec3(uv * 2.0f - 1.0f, depth * 2.0f - 1.0f);
#else
	vec3 ndc = vec3(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth);
#endif
	vec4 p = mul(u_invViewProj, vec4(ndc, 1.0f));
	return p.xyz / p.w;
}

void main()
{
	vec2 texel = floor(gl_FragCoord.xy);
	vec2 tile = floor(texel / ATMOS_AERIAL_CELLS);
	vec2 cell = texel - tile * ATMOS_AERIAL_CELLS;
	float slice = tile.y * ATMOS_AERIAL_TILES_ACROSS + tile.x;
	vec2 uv = (cell + 0.5f) / ATMOS_AERIAL_CELLS;

	vec3 eye = mul(u_invView, vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
	vec3 dir = ViewRayAt(uv);
	float reach = AtmosSliceDistance(slice);
	float steps = max(u_atmosAerial.y, 1.0f);
	float stride = reach / steps;
	float strideKm = stride / ATMOS_UNITS_PER_KM;
	float sea = u_atmosParams.w;
	// The dust's glow is round the sun that the sky shows
	float cosAngle = dot(dir, normalize(u_atmosGameLight.xyz));
	// From high up the eye can be far round the planet: there the height and the light's height come from the ball
	bool overBall = WorldEyeHeight(eye.y - sea) > ATMOS_HIGH_EYE;
	vec3 light = vec3_splat(0.0f);
	vec3 through = vec3_splat(1.0f);
	for (int i = 0; i < 16; ++i)
	{
		if (float(i) >= steps)
		{
			break;
		}
		vec3 p = eye + dir * ((float(i) + 0.5f) * stride);
		float altitude = max((overBall ? WorldAltitude(p, sea) : p.y) - sea, 0.0f) / ATMOS_UNITS_PER_KM;
		// From high up each place has the air's light of its own sun height: far round the planet the day side has the
		// light of day while the island has its night. Near the island the air keeps the island's light.
		float lightY = u_atmosLight.y;
		vec3 placeScale = vec3_splat(1.0f);
		if (overBall)
		{
			vec3 up = WorldUpAt(p - eye, eye.y - sea);
			float island = PlaceIslandShare(up);
			lightY = mix(PlaceAirLightHeight(up), dot(up, u_atmosLight.xyz), island);
			placeScale = mix(PlaceAirScale(up) / max(u_atmosSun.rgb, vec3_splat(1e-4f)), vec3_splat(1.0f), island);
		}
		float lit = 1.0f;
		if (u_atmosAerial.x > 0.5f)
		{
			lit = SunVisibilityAbove(p) * CloudShadeInAir(p, normalize(u_atmosGameLight.xyz), sea);
		}
		float scale = AtmosHazeScaleAt(altitude);
		vec3 scattered = AtmosScatterAt(altitude, lightY, cosAngle, lit) * scale * placeScale;
		vec3 extinction = max(AtmosExtinction(altitude) * scale, vec3_splat(1e-7f));
		vec3 dimmed = exp(-extinction * strideKm);
		light += through * scattered * (1.0f - dimmed) / extinction;
		through *= dimmed;
	}
	gl_FragColor = vec4(light, dot(through, vec3_splat(1.0f / 3.0f)));
}
