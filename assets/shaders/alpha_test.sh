#ifndef ALPHA_TEST_SH
#define ALPHA_TEST_SH

// The alpha test of the game's render modes (src/Visual/RenderModes.h, PassesAlphaTest). The game keeps a texel when its
// alpha is the reference or more, in whole steps of 0 to 255. A reference of 0 is a material with no test: this code
// then drops only the texels with no alpha. A reference below 0 is an opaque or blended material with no test that
// writes the depth (see KeepsTexelsWithNoAlpha): it keeps all of its texels.
bool FailsAlphaTest(float alpha, float threshold)
{
	if (threshold < 0.0f)
	{
		return false;
	}
	float reference = floor(threshold * 255.0f + 0.5f);
	if (reference < 0.5f)
	{
		return alpha <= 0.0f;
	}
	return floor(alpha * 255.0f + 0.5f) < reference;
}

#endif // ALPHA_TEST_SH
