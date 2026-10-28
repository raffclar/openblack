#ifndef SNOW_OBJECT_SH
#define SNOW_OBJECT_SH

// The snow objects show, and its alpha that says where it shows first
SAMPLER2D(s_snow, 12);
SAMPLER2D(s_snowAlpha, 13);

// Whether a texel of an object shows snow at a level: where the alpha of the snow texture is at least 255 less the
// level. All objects and the faces of broken shapes use this rule. Where the snow shows, it replaces the colour of the
// object. Graphics/ObjectSnow.h has the same rule.
bool SnowShows(float level, float snowAlpha)
{
	float threshold = 255.0f - clamp(level, 0.0f, 255.0f);
	return level > 0.0f && floor(snowAlpha * 255.0f + 0.5f) >= threshold;
}

#endif // SNOW_OBJECT_SH
