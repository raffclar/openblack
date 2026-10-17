$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "fog.sh"

// EXPERIMENT (not for merge): the fog summed from the eye to each slice of the grid. Each cell adds the light that the
// slices before it scatter, each dimmed by the fog between it and the eye, and keeps how much of the scene behind
// shows through. rgb: the light that the fog adds; a: how much of the scene shows through.

SAMPLER2D(s_fogScatter, 0);

void main()
{
	vec2 cells = u_fogGrid.xy;
	vec2 texel = floor(gl_FragCoord.xy);
	vec2 tile = floor(texel / cells);
	vec2 cell = texel - tile * cells;
	float slice = tile.y * FOG_TILES_ACROSS + tile.x;
	vec2 uv = (cell + 0.5f) / cells;

	vec3 light = vec3_splat(0.0f);
	float through = 1.0f;
	float start = 0.0f;
	for (int i = 0; i < 64; ++i)
	{
		if (float(i) > slice)
		{
			break;
		}
		float end = FogSliceDistance(float(i) + 0.5f);
		float thickness = end - start;
		start = end;
		vec4 scatter = texture2DLod(s_fogScatter, FogTileUv(uv, float(i)), 0.0f);
		float sigma = max(scatter.a, 1e-7f);
		float dimmed = exp(-sigma * thickness);
		// The light scattered over the slice, as the slice's own fog dims it
		light += through * scatter.rgb * (1.0f - dimmed) / sigma;
		through *= dimmed;
	}
	gl_FragColor = vec4(light, through);
}
