// The soft glow of the screen under a good sky, with the weather option (see Graphics/ScreenGlow.h). The draw that adds
// the glow and the step of its points use the same rules here, thus the step reads the screen as the draw makes it.

// The frame drawn before the glow
SAMPLER2D(s_screenGlowScene, 0);
// The 16 by 16 points: the red, green and blue values of each, as whole numbers
SAMPLER2D(s_screenGlowPoints, 1);
// x, y: the screen's size in pixels. z: the glow's strength, of 1. w: 1 when the points have values, 0 before their
// first step.
uniform vec4 u_screenGlow;

// The pixel from the top left of the screen, at a position of the target as gl_FragCoord gives it
vec2 ScreenGlowPixel(vec2 fragCoord)
{
	vec2 pixel = fragCoord - vec2_splat(0.5);
#if BGFX_SHADER_LANGUAGE_GLSL
	pixel.y = u_screenGlow.y - 1.0 - pixel.y;
#endif
	return pixel;
}

// The values of a cell, red, green and blue. They are 0 before the first step.
vec3 ScreenGlowValues(vec2 cell)
{
	if (u_screenGlow.w < 0.5)
	{
		return vec3_splat(0.0);
	}
	return texture2DLod(s_screenGlowPoints, (cell + vec2_splat(0.5)) / 16.0, 0.0).rgb;
}

// The levels of 4 bits of a cell, as the game writes them into its texture of the glow. Blue goes to the nearest
// level. Red and green keep their top bits after a much smaller addition (a fault of the game that openblack keeps).
vec3 ScreenGlowLevels(vec2 cell)
{
	return floor((ScreenGlowValues(cell) + vec3(128.0, 2048.0, 32768.0)) / 65536.0);
}

// The colour of a pixel with the glow added, as the screen keeps it. The points spread from the first pixel to the far
// edge of the screen. The glow between them is filtered.
vec3 WithScreenGlow(vec3 scene, vec2 fragCoord)
{
	vec2 at = ScreenGlowPixel(fragCoord) * 15.0 / u_screenGlow.xy;
	vec2 low = floor(at);
	vec2 f = at - low;
	vec2 high = min(low + vec2_splat(1.0), vec2_splat(15.0));
	vec3 top = mix(ScreenGlowLevels(low), ScreenGlowLevels(vec2(high.x, low.y)), f.x);
	vec3 bottom = mix(ScreenGlowLevels(vec2(low.x, high.y)), ScreenGlowLevels(high), f.x);
	vec3 levels = mix(top, bottom, f.y);
	return saturate(scene + (levels / 15.0) * u_screenGlow.z);
}
