#ifndef SNOW_OBJECT_SH
#define SNOW_OBJECT_SH

// The snow objects show, and its alpha that says where it shows first
SAMPLER2D(s_snow, 12);
SAMPLER2D(s_snowAlpha, 13);
// z: 1 for the faces of a broken shape (see snow.sh)
uniform vec4 u_snow;

// Whether a texel of an object shows snow at a level: where the snow texture's alpha reaches what the level asks for.
// The faces of a broken shape show snow where the alpha is at least 255 less the level.
bool SnowShows(float level, float snowAlpha)
{
	float most = u_snow.z > 0.5f ? 255.0f : 250.0f;
	float threshold = max(most - min(level, 255.0f), 0.0f);
	return level > 0.0f && floor(snowAlpha * 255.0f + 0.5f) >= threshold;
}

#endif // SNOW_OBJECT_SH
