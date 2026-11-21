$input v_texcoord0

#include <bgfx_shader.sh>

#include "screen_glow.sh"

// One step of the 16 by 16 points of the glow towards the frame as the screen shows it, with the glow added. Each
// channel moves an eighth of the way in whole numbers, cut towards zero. It stays a little under the top level of 4
// bits, thus the top level never overflows.
void main()
{
	vec2 cell = floor(v_texcoord0.xy * 16.0);
	// The columns are a sixteenth of the width apart, from the middle of the first sixteenth. The rows are at a
	// sixteenth of the height each, moved down by a thirty-second.
	vec2 size = u_screenGlow.xy;
	float across = floor(size.x / 16.0);
	vec2 pixel = vec2(floor(across / 2.0) + cell.x * across, floor(cell.y * size.y / 16.0) + floor(floor(size.y / 16.0) / 2.0));
#if BGFX_SHADER_LANGUAGE_GLSL
	pixel.y = size.y - 1.0 - pixel.y;
#endif
	vec2 fragCoord = pixel + vec2_splat(0.5);
	vec3 scene = texture2DLod(s_screenGlowScene, fragCoord / size, 0.0).rgb;
	vec3 shown = floor(WithScreenGlow(scene, fragCoord) * 255.0 + 0.5);
	vec3 value = ScreenGlowValues(cell);
	vec3 difference = shown * 4096.0 - value;
	vec3 stepped = value + sign(difference) * floor(abs(difference) / 8.0);
	gl_FragColor = vec4(clamp(stepped, 0.0, 1011712.0), 1.0);
}
