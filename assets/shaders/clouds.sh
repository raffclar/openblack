#ifndef CLOUDS_SH
#define CLOUDS_SH

#include "sky_dome.sh"
#include "world_curve.sh"

// 1 to make the code of the thunderstorm cells (see CloudHasCells)
#ifndef CLOUD_CELLS
#define CLOUD_CELLS 1
#endif

// EXPERIMENT (not for merge): the volumetric clouds. The rays go through a layer of cloud over the land and add the
// light that the cloud scatters towards the eye. The original game has billboards for its clouds. The formulas match
// the CPU twins in src/Graphics/VolumetricClouds.cpp, which the tests check.

// x: the bottom of the layer; y: not used (world_curve.sh has the curve of the world); z: the farthest cloud; w: the
// sea's level
uniform vec4 u_cloudLayer;
// x: how much of the sky has cloud; y: the type of cloud (0 stratus, 0.5 cumulus, 1 cumulonimbus); z: the rain; w: the
// number of storms
uniform vec4 u_cloudCover;
// xy: how far the wind has moved the clouds; z: how far their shapes have changed; w: the frame's number
uniform vec4 u_cloudDrift;
// xyz: the direction towards the sun or the moon; w: how much of the light is the moon's
uniform vec4 u_cloudLight;
// rgb: the colour and the strength of the light; w: 1 while there is light
uniform vec4 u_cloudLightColour;
// rgb: the light of the sky; w: the flash of the lightning in the sky, 0 to 1
uniform vec4 u_cloudAmbient;
// xyz: the eye; w: 1 while the eye is above the water
uniform vec4 u_cloudEye;
// The flashes of the lightning inside the clouds (cloud_lightning::Pack): [i] xyz the start of the channel, w its light
// now; [4 + i] xyz the end of the channel, w how far its light spreads through the cloud
uniform vec4 u_cloudFlashes[8];
// The storms: [i] xy the middle, z the inner radius, w the outer radius; [8 + i] x the strength, y the rain
uniform vec4 u_cloudStorms[16];
// The thunderstorm cells (cumulonimbus::Pack): [i] xy the middle of the tower at its bottom, z the tower's radius, w its
// height as a share of the height from its bottom to the tropopause; [4 + i] xy the direction downwind, z how much of
// the cell is still there, w the rain under its core; [8 + i] the upright cylinder that holds the tower and its rain
// (xy the middle, z the radius, w the top), down to the sea; [12 + i] the flat cylinder that holds the anvil (xy the
// middle, z the radius, 0 with no anvil, w the bottom), up to the tower's top. A cell with no height or no fade is not
// there.
uniform vec4 u_cloudCells[16];

// The world units that one tile of each noise covers (volumetric_clouds::k_BaseNoiseTile and the others)
#define CLOUD_BASE_TILE 1500.0f
#define CLOUD_DETAIL_TILE 210.0f
#define CLOUD_WEATHER_TILE 14000.0f
// The units that one tile of the wide weather covers (volumetric_clouds::k_WideWeatherTile). This slow field over tens
// of kilometres moves the cells and changes their cover, so that no repeat of the weather tile shows from high up
#define CLOUD_WIDE_TILE 47000.0f
// The top of the layer for each type of cloud, over its bottom
#define CLOUD_STRATUS_DEPTH 160.0f
#define CLOUD_CUMULUS_DEPTH 520.0f
#define CLOUD_CUMULONIMBUS_DEPTH 1500.0f
// The heights of the eye over the sea between which the layer turns into a thin sheet (CloudSheet)
#define CLOUD_SHEET_LOW 40000.0f
#define CLOUD_SHEET_HIGH 90000.0f
// How far under the bottom of the layer the ragged bottom of a rain cloud hangs at most
#define CLOUD_LOWEST_DROP 200.0f
// The distance past which a ray through the clouds leaves out their small shapes
#define CLOUD_DETAIL_REACH 9000.0f
// The tallest type of the layer's own clouds, and the top of the layer over its bottom for it. Only the thunderstorm
// cells go higher.
#define CLOUD_FIELD_TYPE 0.8f
#define CLOUD_FIELD_DEPTH 1108.0f
// The thunderstorm cells (cumulonimbus::k_Base and the others): the flat bottom and the tropopause over the sea, and how
// far the dome over the strongest updraft goes up through the top
#define CB_BASE 1000.0f
#define CB_TOP 10439.0f
#define CB_DOME 900.0f
// How far the top of a tower leans downwind in the wind's shear, in radii of the tower
#define CB_LEAN 0.8f
// The anvil, in radii of the tower: how far it reaches upwind of the tower's top, how far downwind, and how wide it
// is at its downwind end
#define CB_ANVIL_UP 2.0f
#define CB_ANVIL_DOWN 5.5f
#define CB_ANVIL_WIDE 3.0f
// The depth of the anvil under its top, at its upwind end
#define CB_ANVIL_DEPTH 1800.0f
// The density of the rain under the core of a cell, as a part of the cloud's density
#define CB_RAIN_DENSITY 0.035f
// The noise of the towers is larger than the noise of the layer: large turrets
#define CB_NOISE_TILE 3600.0f
// The farthest that a ray goes to a thunderstorm cell: a tower shows from much farther than the layer's clouds
#define CB_FARTHEST 60000.0f
// How much light the cloud takes out for each unit, at a density of 1
#define CLOUD_EXTINCTION 0.06f
// The colour of the flash of the lightning
#define CLOUD_FLASH_COLOUR vec3(0.78f, 0.84f, 1.0f)
#define CLOUD_PI 3.14159265f

// The noise volumes, the weather noise and the blue noise
SAMPLER3D(s_cloudBase, 0);
SAMPLER3D(s_cloudDetail, 1);
SAMPLER2D(s_cloudWeather, 2);

float CloudRemap(float value, float fromLow, float fromHigh, float toLow, float toHigh)
{
	return toLow + (value - fromLow) / (fromHigh - fromLow) * (toHigh - toLow);
}

// The top of the layer, over its bottom, for a type of cloud
float CloudLayerDepth(float type)
{
	return type < 0.5f ? mix(CLOUD_STRATUS_DEPTH, CLOUD_CUMULUS_DEPTH, type * 2.0f)
	                   : mix(CLOUD_CUMULUS_DEPTH, CLOUD_CUMULONIMBUS_DEPTH, (type - 0.5f) * 2.0f);
}

// The density of a type of cloud at a height in the layer (0 at its bottom, 1 at the top of the type)
float CloudHeightProfile(float type, float h)
{
	if (h <= 0.0f || h >= 1.0f)
	{
		return 0.0f;
	}
	float stratus = smoothstep(0.0f, 0.08f, h) * (1.0f - smoothstep(0.3f, 0.9f, h));
	float cumulus = smoothstep(0.0f, 0.12f, h) * (1.0f - smoothstep(0.45f, 1.0f, h));
	float cumulonimbus = smoothstep(0.0f, 0.06f, h) * (1.0f - smoothstep(0.82f, 1.0f, h));
	float t = clamp(type, 0.0f, 1.0f);
	return t < 0.5f ? mix(stratus, cumulus, t * 2.0f) : mix(cumulus, cumulonimbus, (t - 0.5f) * 2.0f);
}

float CloudHenyeyGreenstein(float cosAngle, float g)
{
	float g2 = g * g;
	float d = 1.0f + g2 - 2.0f * g * cosAngle;
	return (1.0f - g2) / (4.0f * CLOUD_PI * d * sqrt(d));
}

// The light that reaches a point through an optical depth towards the light, with the light that scatters many times:
// octaves of less extinction, less energy and a wider phase of two lobes
float CloudMultipleScattering(float opticalDepth, float cosAngle)
{
	float sum = 0.0f;
	float extinction = 1.0f;
	float energy = 1.0f;
	float asymmetry = 1.0f;
	for (int octave = 0; octave < 3; ++octave)
	{
		float phase = mix(CloudHenyeyGreenstein(cosAngle, 0.8f * asymmetry), CloudHenyeyGreenstein(cosAngle, -0.35f * asymmetry),
		                  0.35f);
		sum += energy * exp(-opticalDepth * extinction) * phase;
		extinction *= 0.5f;
		energy *= 0.5f;
		asymmetry *= 0.5f;
	}
	return sum;
}

// The powder term: the thin edges of a cloud are darker when the eye sees the side that faces the light
float CloudPowder(float localDepth, float cosAngle)
{
	float powder = 1.0f - exp(-2.0f * localDepth);
	return mix(1.0f, powder, 0.8f * (0.5f - 0.5f * cosAngle));
}

// The height of a point over the curved world under the eye: the layer is a shell round the planet, as the sea is
float CloudAltitude(vec3 p)
{
	return WorldAltitude(p, u_cloudLayer.w);
}

// The weather at a point across the land: the cloud's cover there (0 none), its type and its rain
float CloudWeatherAt(vec2 xz, out float type, out float rain)
{
	vec2 q = xz - u_cloudDrift.xy;
	// The wide weather reads the noise at a large scale, on axes that turn about 30 degrees from the cells' axes. Each
	// read has a different turn, so that no two grids align.
	vec2 wideAt = vec2(0.866f * q.x - 0.5f * q.y, 0.5f * q.x + 0.866f * q.y) / CLOUD_WIDE_TILE;
	vec2 otherAt = vec2(0.574f * q.x + 0.819f * q.y, -0.819f * q.x + 0.574f * q.y) / CLOUD_WIDE_TILE;
	vec4 wide = texture2DLod(s_cloudWeather, wideAt + vec2(0.37f, 0.71f), 0.0f);
	float wideOther = texture2DLod(s_cloudWeather, otherAt + vec2(0.13f, 0.52f), 0.0f).g;
	// The wide field moves the cells by some kilometres. Near the eye all the cells move together, so their shapes stay.
	// From one tile to the next the move is different, so the tiles do not repeat.
	q += (vec2(wide.g, wideOther) - 0.5f) * 5000.0f;
	// A smooth copy of the noise bends the place that the weather is read at, so that the cells are not round. The bend
	// changes slowly across the land. A fast bend would fold the cells into thin columns, which show as streaks on the
	// sides of the clouds.
	vec2 bend = texture2DLod(s_cloudWeather, q * (2.3f / CLOUD_WEATHER_TILE), 0.0f).gb - 0.5f;
	vec4 w = texture2DLod(s_cloudWeather, (q + bend * 500.0f) / CLOUD_WEATHER_TILE, 0.0f);
	// The field changes slowly between two patterns, so that the clouds form and break up as they move
	float turn = 0.5f + 0.5f * sin(u_cloudDrift.z * 0.0035f);
	float field = mix(w.r, w.b, turn);
	// The wide field also makes large areas of more cloud and of less cloud
	float cover = min(u_cloudCover.x * (0.65f + 0.7f * wide.r), 1.0f);
	// The type of cloud changes at its own scale, on its own axes
	vec2 typeAt = vec2(0.94f * q.x + 0.342f * q.y, -0.342f * q.x + 0.94f * q.y) / (CLOUD_WEATHER_TILE * 1.9f);
	float typeNoise = texture2DLod(s_cloudWeather, typeAt + vec2(0.61f, 0.27f), 0.0f).g;
	type = clamp(u_cloudCover.y + (typeNoise - 0.5f) * 0.3f, 0.0f, 1.0f);
	rain = u_cloudCover.z;
	// Each storm builds its own towers round its middle
	for (int i = 0; i < 8; ++i)
	{
		if (float(i) >= u_cloudCover.w)
		{
			break;
		}
		vec4 storm = u_cloudStorms[i];
		vec4 strength = u_cloudStorms[i + 8];
		float reach = 1.0f - smoothstep(storm.z, storm.w, length(xz - storm.xy));
		float s = strength.x * reach;
		// Round the thunderstorm cells the layer is broken and low, so that the towers and their bottoms show
		cover = mix(cover, 0.75f, s * 0.55f);
		type = mix(type, 0.65f, s);
		rain = max(rain, strength.y * reach);
	}
	// The layer's own clouds stay lower than the thunderstorm cells, which go up to the tropopause
	type = min(type, CLOUD_FIELD_TYPE);
	return smoothstep(1.0f - cover - 0.12f, 1.0f - cover + 0.3f, field) * smoothstep(0.0f, 0.05f, cover);
}

// Whether a thunderstorm cell is there now. A shader made with CLOUD_CELLS 0 has none of the cells' code: the
// cells' code takes registers that slow down the rays when no cell is there.
bool CloudHasCells()
{
#if CLOUD_CELLS
	return u_cloudCells[4].z * u_cloudCells[0].w + u_cloudCells[5].z * u_cloudCells[1].w + u_cloudCells[6].z * u_cloudCells[2].w +
	           u_cloudCells[7].z * u_cloudCells[3].w >
	       0.0f;
#else
	return false;
#endif
}

// The thunderstorm cells at a point at a height over the curved world: x how much the point is inside a cell's cloud,
// 0 to 1; y the height in that cell's tower, 0 at its bottom and 1 at the tropopause; z how much rain falls there, 0
// to 1; w the rain of that cell. Each cell is a tower from a flat bottom that leans downwind with the height, with a
// dome over its top and an anvil that spreads out downwind. The anvil has a sharp edge upwind and a soft plume
// downwind. `soft` is 0 in the tower, which boils, and goes up to 1 in the thin plume of the anvil. `plain` leaves out the
// bulges of the towers and the roughness of the anvils, for the rays towards the light.
vec4 CumulonimbusAt(vec3 p, float altitude, bool plain, out float soft)
{
	vec4 result = vec4_splat(0.0f);
	soft = 0.0f;
	float span = CB_TOP - CB_BASE;
	float h = (altitude - CB_BASE) / span;
	for (int i = 0; i < 4; ++i)
	{
		vec4 cell = u_cloudCells[i];
		vec4 more = u_cloudCells[4 + i];
		if (cell.w * more.z <= 0.0f)
		{
			continue;
		}
		float radius = cell.z;
		vec2 down = more.xy;
		float topAltitude = CB_BASE + span * cell.w;
		// Only the points inside the cylinder of the tower or of the anvil
		vec4 towerBound = u_cloudCells[8 + i];
		vec4 anvilBound = u_cloudCells[12 + i];
		vec2 toTower = p.xz - towerBound.xy;
		vec2 toAnvil = p.xz - anvilBound.xy;
		bool inTower = dot(toTower, toTower) < towerBound.z * towerBound.z;
		bool inAnvil = altitude > anvilBound.w && dot(toAnvil, toAnvil) < anvilBound.z * anvilBound.z;
		if (altitude > towerBound.w || !(inTower || inAnvil))
		{
			continue;
		}
		// The rain falls from the bottom of the tower under its core, a little downwind
		if (h < 0.0f)
		{
			float off = length(p.xz - cell.xy - down * (0.15f * radius)) / (0.55f * radius);
			float shaft = (1.0f - smoothstep(0.35f, 1.0f, off)) * more.w * more.z * smoothstep(0.3f, 0.8f, cell.w);
			result.z = max(result.z, shaft);
			continue;
		}
		// The wind's shear moves the top of the tower downwind
		float top = cell.w;
		vec2 axisTop = cell.xy + down * (CB_LEAN * radius * top * top);
		vec2 axis = cell.xy + down * (CB_LEAN * radius * h * h);
		float across = length(p.xz - axis);
		// The tower is widest a third of the way up and narrower near its top. Its bottom is flat.
		float towerRadius = radius * (0.85f + 0.2f * smoothstep(0.0f, 0.3f, h) - 0.3f * smoothstep(0.55f, 1.0f, h));
		// The towers are a group of turrets, so their sides bulge in and out up the height and round the axis. A circle
		// in the noise round the axis has no seam.
		if (!plain)
		{
			vec2 offAxis = (p.xz - axis) / radius;
			vec2 ring = offAxis / max(across / radius, 1e-3f);
			float bulge = texture2DLod(s_cloudWeather, vec2(h * 1.6f + float(i) * 0.31f, 0.37f) + ring * 0.12f, 0.0f).g;
			towerRadius *= 0.7f + 0.6f * bulge;
		}
		float tower = (1.0f - smoothstep(0.55f, 1.0f, across / towerRadius)) * (1.0f - smoothstep(top - 0.05f, top, h)) *
		              smoothstep(0.0f, 0.004f, h);
		// The dome over the strongest updraft, while the cell is at its full height
		float overTop = (altitude - topAltitude) / CB_DOME;
		float domeRadius = 0.45f * radius * sqrt(max(1.0f - overTop * overTop, 0.0f));
		float dome = overTop > -0.2f ? (1.0f - smoothstep(0.5f, 1.0f, length(p.xz - axisTop) / max(domeRadius, 1.0f))) *
		                                   smoothstep(0.9f, 1.0f, cell.w)
		                             : 0.0f;
		// The anvil grows as the tower reaches the tropopause
		float grown = smoothstep(0.75f, 1.0f, cell.w);
		float anvil = 0.0f;
		float anvilSoft = 0.0f;
		if (grown > 0.0f)
		{
			vec2 fromTop = p.xz - axisTop;
			float along = dot(fromTop, down);
			float side = dot(fromTop, vec2(-down.y, down.x));
			float upwind = CB_ANVIL_UP * radius * mix(0.7f, 1.0f, grown);
			float reach = CB_ANVIL_DOWN * radius * grown;
			float share = clamp(along / max(reach, 1.0f), 0.0f, 1.0f);
			float halfWidth = upwind * (1.0f + (CB_ANVIL_WIDE / CB_ANVIL_UP - 1.0f) * share);
			float outside = along < 0.0f ? length(vec2(along, side)) / upwind : length(vec2(along / max(reach, 1.0f), side / halfWidth));
			// A sharp edge upwind, softer and thinner downwind
			float level = (1.0f - smoothstep(mix(0.9f, 0.0f, share), 1.0f, outside)) * (1.0f - 0.8f * smoothstep(0.1f, 1.0f, share));
			anvilSoft = smoothstep(-0.25f, 0.45f, share);
			// The top of the anvil and its ragged bottom rise and fall a little
			float rough = plain ? 0.0f : texture2DLod(s_cloudWeather, fromTop / 21000.0f + vec2(float(i) * 0.29f, 0.61f), 0.0f).b - 0.5f;
			float roof = topAltitude + rough * 300.0f;
			float bottom = topAltitude - CB_ANVIL_DEPTH * grown * (1.0f - 0.7f * share) - rough * 500.0f;
			anvil = level * smoothstep(bottom, bottom + 300.0f, altitude) * (1.0f - smoothstep(roof - 200.0f, roof + 50.0f, altitude));
		}
		float inside = max(max(tower, dome), anvil) * more.z;
		if (inside > result.x)
		{
			result.x = inside;
			result.y = h;
			result.w = more.w;
			soft = anvil > max(tower, dome) ? anvilSoft : 0.0f;
		}
	}
	return result;
}

// The density of the cloud at a point at a height over the curved world. `detail` adds the small shapes that wear away
// the edges; the light's rays leave them out. `h` is the height in the cloud for its light, 0 at its bottom and 1 at its
// top, and `rain` its rain. `shaft` is how much rain falls at the point under a thunderstorm cell, 0 to 1. `tower` is
// 1 where the cloud is a thunderstorm cell's, else 0. `layerShare` thins the layer's own clouds, and not the cells.
float CloudDensityAt(vec3 p, float altitude, bool detail, float layerShare, out float h, out float rain, out float shaft,
                     out float tower)
{
	float soft = 0.0f;
	vec4 storm = vec4_splat(0.0f);
	if (CloudHasCells())
	{
		storm = CumulonimbusAt(p, altitude, false, soft);
	}
	shaft = storm.z;
	tower = 0.0f;
	h = 0.0f;
	rain = 0.0f;
	// The height in the cloud for its shape, which is not the same as the height for its light in the layer
	float shapeH = 0.0f;
	// The layer's own clouds, under the top of their tallest type
	float profile = 0.0f;
	float cover = 0.0f;
	float type = 0.0f;
	float fieldH = 0.0f;
	float fieldRain = 0.0f;
	if (altitude < u_cloudLayer.x + CLOUD_FIELD_DEPTH && altitude > u_cloudLayer.x - CLOUD_LOWEST_DROP)
	{
		cover = CloudWeatherAt(p.xz, type, fieldRain);
		if (cover > 0.001f)
		{
			// The bottom of the cloud rises and falls a little; under rain it hangs lower, in soft lumps. The lumps are
			// some hundred units wide: narrow lumps made the bottoms of the rain clouds into teeth.
			float lumps = texture2DLod(s_cloudWeather, (p.xz - u_cloudDrift.xy) * (2.2f / CLOUD_WEATHER_TILE), 0.0f).a - 0.5f;
			float bottom = u_cloudLayer.x - fieldRain * 70.0f + lumps * (40.0f + fieldRain * 120.0f);
			// Each cloud is tallest where its cover is densest, at its middle, and low at its edges: round domes
			fieldH = (altitude - bottom) / (CloudLayerDepth(type) * mix(0.3f, 1.0f, cover));
			profile = CloudHeightProfile(type, fieldH);
		}
	}
	if (profile <= 0.0f && storm.x <= 0.0f)
	{
		return 0.0f;
	}
	// The clouds move with the wind, their tops a little ahead, and change shape slowly
	vec3 q = p;
	q.xz -= u_cloudDrift.xy * (1.0f + fieldH * 0.15f);
	q.y -= u_cloudDrift.z;
	float shape = 0.0f;
	// The layer's noise, which the towers also take for their small turrets
	vec4 n = vec4_splat(0.0f);
	vec3 fromEye = p - u_cloudEye.xyz;
	bool turrets = detail && dot(fromEye, fromEye) < CLOUD_DETAIL_REACH * CLOUD_DETAIL_REACH;
	if (profile > 0.0f || (storm.x > 0.0f && turrets))
	{
		n = texture3DLod(s_cloudBase, q / CLOUD_BASE_TILE, 0.0f);
	}
	if (profile > 0.0f)
	{
		float low = n.g * 0.625f + n.b * 0.25f + n.a * 0.125f;
		float base = clamp(CloudRemap(n.r, low - 1.0f, 1.0f, 0.0f, 1.0f), 0.0f, 1.0f);
		// The shape spreads over the whole range, so that the tops rise and fall with it
		base = clamp((base - 0.42f) / 0.45f, 0.0f, 1.0f);
		// The more cover, the lower the threshold that the shape must pass. Inside a full cell the noise still makes
		// gaps, and the tops follow the noise as the profile falls.
		// The threshold also rises with the height, so that the sides lean in to round tops and no cell has walls
		float threshold = mix(0.85f, 0.25f, cover) + fieldH * 0.15f;
		shape = clamp(CloudRemap(base * profile, threshold, 1.0f, 0.0f, 1.0f), 0.0f, 1.0f) * smoothstep(0.0f, 0.3f, cover) * layerShare;
		shapeH = fieldH;
		h = clamp((altitude - u_cloudLayer.x) / CloudLayerDepth(type), 0.0f, 1.0f);
		rain = fieldRain;
	}
	if (storm.x > 0.0f)
	{
		// The towers boil in large turrets. Deep inside a cell nearly all of the noise passes; at its edges only the
		// tops of the noise pass, so the edges are lumpy.
		vec3 boil = p - vec3(u_cloudDrift.x, u_cloudDrift.z * 2.5f, u_cloudDrift.y);
		vec4 large = texture3DLod(s_cloudBase, boil / CB_NOISE_TILE, 0.0f);
		float low = large.g * 0.625f + large.b * 0.25f + large.a * 0.125f;
		float base = clamp(CloudRemap(large.r, low - 1.0f, 1.0f, 0.0f, 1.0f), 0.0f, 1.0f);
		// The small turrets on the large ones, near the eye
		float small = turrets ? n.g * 0.625f + n.b * 0.25f + n.a * 0.125f : 0.5f;
		base = clamp((base * 0.7f + small * 0.3f - 0.3f) / 0.55f, 0.0f, 1.0f);
		float boiling = clamp(CloudRemap(base, mix(0.92f, 0.04f, storm.x), 1.0f, 0.0f, 1.0f), 0.0f, 1.0f) * smoothstep(0.0f, 0.2f, storm.x);
		// The plume of the anvil is a thin, smooth veil that the light goes through
		float veil = storm.x * (0.06f + 0.12f * base);
		float tall = mix(boiling, veil, soft);
		if (tall > shape)
		{
			shape = tall;
			h = clamp(storm.y, 0.0f, 1.0f);
			shapeH = h;
			tower = 1.0f;
			// The rain darkens the low part of the tower; its top is in the sun over the storm
			rain = storm.w * (1.0f - smoothstep(0.1f, 0.5f, h));
		}
	}
	if (shape <= 0.0f)
	{
		return 0.0f;
	}
	if (detail)
	{
		vec3 d = texture3DLod(s_cloudDetail, (q - vec3(0.0f, u_cloudDrift.z * 2.0f, 0.0f)) / CLOUD_DETAIL_TILE, 0.0f).rgb;
		float fine = d.r * 0.625f + d.g * 0.25f + d.b * 0.125f;
		// Wisps at the bottom, billows at the top
		float wear = mix(fine, 1.0f - fine, clamp(shapeH * 5.0f, 0.0f, 1.0f));
		shape = clamp(CloudRemap(shape, wear * mix(0.4f, 0.6f, shapeH), 1.0f, 0.0f, 1.0f), 0.0f, 1.0f);
	}
	// Rain clouds are denser and darker at the bottom
	return shape * (1.0f + rain * 0.8f * (1.0f - shapeH));
}

// The density of the cloud at a point at a height over the curved world, with no small shapes, for the rays towards the
// light and the map of the shadow. The layer's clouds have the shape of CloudDensityAt. In the thunderstorm cells only
// the large turrets shade the light.
float CloudDensity(vec3 p, float altitude, bool detail)
{
	float density = 0.0f;
	if (altitude < u_cloudLayer.x + CLOUD_FIELD_DEPTH && altitude > u_cloudLayer.x - CLOUD_LOWEST_DROP)
	{
		float type;
		float rain;
		float cover = CloudWeatherAt(p.xz, type, rain);
		if (cover > 0.001f)
		{
			float lumps = texture2DLod(s_cloudWeather, (p.xz - u_cloudDrift.xy) * (2.2f / CLOUD_WEATHER_TILE), 0.0f).a - 0.5f;
			float bottom = u_cloudLayer.x - rain * 70.0f + lumps * (40.0f + rain * 120.0f);
			float h = (altitude - bottom) / (CloudLayerDepth(type) * mix(0.3f, 1.0f, cover));
			float profile = CloudHeightProfile(type, h);
			if (profile > 0.0f)
			{
				vec3 q = p;
				q.xz -= u_cloudDrift.xy * (1.0f + h * 0.15f);
				q.y -= u_cloudDrift.z;
				vec4 n = texture3DLod(s_cloudBase, q / CLOUD_BASE_TILE, 0.0f);
				float low = n.g * 0.625f + n.b * 0.25f + n.a * 0.125f;
				float base = clamp((clamp(CloudRemap(n.r, low - 1.0f, 1.0f, 0.0f, 1.0f), 0.0f, 1.0f) - 0.42f) / 0.45f, 0.0f, 1.0f);
				float threshold = mix(0.85f, 0.25f, cover) + h * 0.15f;
				density = clamp(CloudRemap(base * profile, threshold, 1.0f, 0.0f, 1.0f), 0.0f, 1.0f) * smoothstep(0.0f, 0.3f, cover) *
				          (1.0f + rain * 0.8f * (1.0f - h));
			}
		}
	}
	if (CloudHasCells())
	{
		float soft;
		vec4 storm = CumulonimbusAt(p, altitude, true, soft);
		if (storm.x > 0.0f)
		{
			// The large turrets shade each other: the light goes through the gaps between them
			vec3 q = p - vec3(u_cloudDrift.x, u_cloudDrift.z * 2.5f, u_cloudDrift.y);
			vec4 large = texture3DLod(s_cloudBase, q / CB_NOISE_TILE, 0.0f);
			float low = large.g * 0.625f + large.b * 0.25f + large.a * 0.125f;
			float base = clamp((clamp(CloudRemap(large.r, low - 1.0f, 1.0f, 0.0f, 1.0f), 0.0f, 1.0f) - 0.3f) / 0.55f, 0.0f, 1.0f);
			float boiling = clamp(CloudRemap(base, mix(0.92f, 0.04f, storm.x), 1.0f, 0.0f, 1.0f), 0.0f, 1.0f);
			density = max(density, mix(boiling, storm.x * 0.15f, soft));
		}
	}
	return density;
}

// The two roots of a t^2 + b t + c = 0, the smaller first; none (x > y) when there are none
vec2 CloudRoots(float a, float b, float c)
{
	if (a < 1e-9f)
	{
		if (abs(b) < 1e-9f)
		{
			return vec2(1.0f, -1.0f);
		}
		float t = -c / b;
		return b > 0.0f ? vec2(-1e9f, t) : vec2(t, 1e9f);
	}
	float disc = b * b - 4.0f * a * c;
	if (disc < 0.0f)
	{
		return vec2(1.0f, -1.0f);
	}
	float s = sqrt(disc);
	return vec2((-b - s) / (2.0f * a), (-b + s) / (2.0f * a));
}

// The part of a ray from the eye inside an upright cylinder (xy the middle across the ground, z the radius) between
// two heights over the sea, up to a limit. None when x >= y.
vec2 CloudCylinderSpan(vec3 eye, vec3 dir, vec3 cylinder, float low, float high, float limit)
{
	// Across the ground: inside the circle
	vec2 o = eye.xz - cylinder.xy;
	vec2 across = CloudRoots(dot(dir.xz, dir.xz), 2.0f * dot(o, dir.xz), dot(o, o) - cylinder.z * cylinder.z);
	// Up and down: between the heights, with a margin for the world's curve
	vec2 upright = vec2(-1e9f, 1e9f);
	if (abs(dir.y) > 1e-6f)
	{
		float t0 = (low - 120.0f - eye.y) / dir.y;
		float t1 = (high + 120.0f - eye.y) / dir.y;
		upright = vec2(min(t0, t1), max(t0, t1));
	}
	else if (eye.y < low - 120.0f || eye.y > high + 120.0f)
	{
		return vec2(1.0f, 0.0f);
	}
	return vec2(max(max(across.x, upright.x), 0.0f), min(min(across.y, upright.y), limit));
}

// The parts of a ray from the eye that can meet the thunderstorm cells, up to a limit: `towers` through the towers and
// their rain, `anvils` through the anvils. Each is from the first cell that the ray goes into to the last that it comes
// out of. None when x >= y.
void CumulonimbusSpans(vec3 eye, vec3 dir, float limit, out vec2 towers, out vec2 anvils)
{
	towers = vec2(1e9f, -1e9f);
	anvils = vec2(1e9f, -1e9f);
	for (int i = 0; i < 4; ++i)
	{
		vec4 cell = u_cloudCells[i];
		vec4 more = u_cloudCells[4 + i];
		if (cell.w * more.z <= 0.0f)
		{
			continue;
		}
		vec4 towerBound = u_cloudCells[8 + i];
		vec2 tower = CloudCylinderSpan(eye, dir, towerBound.xyz, u_cloudLayer.w, towerBound.w, limit);
		if (tower.x < tower.y)
		{
			towers = vec2(min(towers.x, tower.x), max(towers.y, tower.y));
		}
		vec4 anvilBound = u_cloudCells[12 + i];
		if (anvilBound.z > 0.0f)
		{
			vec2 anvil = CloudCylinderSpan(eye, dir, anvilBound.xyz, anvilBound.w, towerBound.w, limit);
			if (anvil.x < anvil.y)
			{
				anvils = vec2(min(anvils.x, anvil.x), max(anvils.y, anvil.y));
			}
		}
	}
}

// Three parts of a ray in the order of their starts, the parts with nothing at the end. Two parts that overlap join.
void CloudOrderSpans(inout vec2 a, inout vec2 b, inout vec2 c)
{
	// A part with nothing starts at the end
	a = a.x < a.y ? a : vec2(1e9f, 1e9f);
	b = b.x < b.y ? b : vec2(1e9f, 1e9f);
	c = c.x < c.y ? c : vec2(1e9f, 1e9f);
	vec2 swap;
	if (b.x < a.x)
	{
		swap = a;
		a = b;
		b = swap;
	}
	if (c.x < b.x)
	{
		swap = b;
		b = c;
		c = swap;
	}
	if (b.x < a.x)
	{
		swap = a;
		a = b;
		b = swap;
	}
	// Join the parts that overlap
	if (b.x <= a.y)
	{
		a.y = max(a.y, b.y);
		b = c;
		c = vec2(1e9f, 1e9f);
	}
	if (c.x <= b.y)
	{
		b.y = max(b.y, c.y);
		c = vec2(1e9f, 1e9f);
	}
	if (b.x <= a.y)
	{
		a.y = max(a.y, b.y);
		b = c;
		c = vec2(1e9f, 1e9f);
	}
}

// The part of a ray from the eye where it is under the height eye.y - c over the curved world. Over the flat cap the
// height changes in a line, and past it the layer is a shell round the ball of the world (WorldBallSpan). The world
// bends down smoothly, so the parts make one span.
vec2 CloudUnder(float c, vec3 dir)
{
	if (u_worldCurve.w <= 0.0f)
	{
		return abs(dir.y) < 1e-9f ? (c < 0.0f ? vec2(-1e9f, 1e9f) : vec2(1.0f, -1.0f)) : CloudRoots(0.0f, dir.y, c);
	}
	float overSea = u_cloudEye.y - u_cloudLayer.w;
	return WorldBallSpan(overSea, dir, overSea - c);
}

// The part of a ray inside the layer, from the eye, up to a limit: x where it goes in, y where it comes out. None when
// x >= y. The world's curve brings every ray up into the layer far away.
vec2 CloudSpan(vec3 eye, vec3 dir, float limit)
{
	// The lowest that a rain cloud's ragged bottom hangs
	float bottom = u_cloudLayer.x - CLOUD_LOWEST_DROP;
	float top = u_cloudLayer.x + CLOUD_FIELD_DEPTH;
	// Under the top between these, over the bottom outside these
	vec2 underTop = CloudUnder(eye.y - top, dir);
	vec2 underBottom = CloudUnder(eye.y - bottom, dir);
	if (underTop.x >= underTop.y)
	{
		return vec2(1.0f, 0.0f);
	}
	float far = min(limit, u_cloudLayer.z);
	if (underBottom.x >= underBottom.y)
	{
		return vec2(max(underTop.x, 0.0f), min(underTop.y, far));
	}
	vec2 first = vec2(max(underTop.x, 0.0f), min(min(underTop.y, underBottom.x), far));
	if (first.x < first.y)
	{
		return first;
	}
	return vec2(max(max(underTop.x, underBottom.y), 0.0f), min(underTop.y, far));
}

// The optical depth from a point towards the light through the cloud, from a few samples that go farther apart. The
// first sample is `firstStep` from the point.
float CloudLightDepth(vec3 p, int samples, float firstStep)
{
	vec3 towards = u_cloudLight.xyz;
	float depth = 0.0f;
	float along = 0.0f;
	float step = firstStep;
	for (int i = 0; i < 6; ++i)
	{
		if (i >= samples)
		{
			break;
		}
		along += step;
		vec3 at = p + towards * (along - step * 0.5f);
		depth += CloudDensity(at, CloudAltitude(at), false) * step;
		step *= 2.0f;
	}
	return depth * CLOUD_EXTINCTION;
}

// The light of the lightning inside a storm's cloud at a point. The flash of the sky lights all of the storm's cloud a
// little. Each flash lights the cloud round its channel: brightly near the channel, and dimly farther away, where more
// cloud is between the point and the channel. The cloud round the channel, under it and over it thus lights up from
// inside.
vec3 CloudLightning(vec3 p, float rain)
{
	float glow = u_cloudAmbient.w * (0.03f + 0.08f * rain);
	for (int i = 0; i < 4; ++i)
	{
		vec4 start = u_cloudFlashes[i];
		if (start.w <= 0.0f)
		{
			continue;
		}
		vec4 end = u_cloudFlashes[4 + i];
		// The distance from the point to the nearest point of the channel
		vec3 along = end.xyz - start.xyz;
		float share = clamp(dot(p - start.xyz, along) / max(dot(along, along), 1.0f), 0.0f, 1.0f);
		vec3 offset = p - start.xyz - along * share;
		float distance = length(offset);
		// The light goes up and down through the tall cloud more easily than across it, so that it shows through the
		// tops and the bottom
		float spread = length(vec3(offset.x, offset.y * 0.35f, offset.z));
		// A thin bright line at the channel, a bright core round it, and a wide glow through the cloud. The wide glow
		// reaches the sides of a thunderstorm tower, kilometres from its channel.
		float thin = 2.5f * exp(-distance / (0.09f * end.w));
		float core = 2.0f * exp(-spread / (0.28f * end.w));
		float wide = 1.8f * exp(-spread / end.w);
		glow += start.w * (thin + core + wide);
	}
	return CLOUD_FLASH_COLOUR * glow;
}

// The light of the flashes inside the clouds on the ground at a point. The lit bottom of each cloud lights a wide area
// under it, most of all straight under the flash.
float CloudFlashOnGround(vec3 p)
{
	float light = 0.0f;
	for (int i = 0; i < 4; ++i)
	{
		vec4 start = u_cloudFlashes[i];
		if (start.w <= 0.0f)
		{
			continue;
		}
		vec3 middle = (start.xyz + u_cloudFlashes[4 + i].xyz) * 0.5f;
		vec3 toFlash = middle - p;
		float height = max(toFlash.y, 100.0f);
		light += start.w * height * height / (height * height + dot(toFlash.xz, toFlash.xz));
	}
	return light;
}

// The haze that hides far clouds: the distance through the air at the sea's level that hides all but a third of a
// cloud (near the haze's own, see atmosphere.sh), and the height over which the air thins to a third
#define CLOUD_HAZE_REACH 7000.0f
#define CLOUD_HAZE_HEIGHT 2000.0f

// The clouds along a ray from the eye up to a limit: rgb the light that they scatter towards the eye, a how much of the
// light behind them goes through. `depth` is where the cloud is along the ray, weighted by how much each part shows,
// or the end of the ray when there is no cloud. `steps` is the most samples, `jitter` (0 to 1) moves the first one.
// `sky` is the light of the sky from above.
vec4 MarchCloudVolume(vec3 eye, vec3 dir, float limit, float jitter, int steps, bool detail, vec3 sky, out float depth)
{
	depth = min(limit, CloudHasCells() ? max(CB_FARTHEST, u_cloudLayer.z) : u_cloudLayer.z);
	// The layer's part of the ray, the towers' part and the anvils' part. The ray goes through each part in turn and
	// jumps over the clear air between them.
	vec2 first = CloudSpan(eye, dir, limit);
	float cellsFarthest = max(CB_FARTHEST, u_cloudLayer.z);
	vec2 second = vec2(1.0f, 0.0f);
	vec2 third = vec2(1.0f, 0.0f);
	if (CloudHasCells())
	{
		CumulonimbusSpans(eye, dir, min(limit, cellsFarthest), second, third);
	}
	CloudOrderSpans(first, second, third);
	if (first.x >= 1e9f)
	{
		return vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}
	// The end of the last part
	vec2 span = vec2(first.x, third.x < 1e9f ? third.y : (second.x < 1e9f ? second.y : first.y));
	// The steps grow with the distance: small near the eye, where the shapes show, and large far away. A short span
	// keeps at least a few dozen steps.
	float spanLength = span.y - span.x;
	float growth = 6.4f / float(steps);
	float shortest = min(6.0f, spanLength / 24.0f);
	float longest = max(spanLength / 24.0f, 6.0f);
	float dt = clamp(span.x * growth, shortest, longest);
	float t = span.x + dt * jitter;
	float cosAngle = dot(dir, u_cloudLight.xyz);
	vec3 sun = u_cloudLightColour.rgb * u_cloudLightColour.w;
	// The light from below: the land and the sea give back a little of the sky's and the sun's light
	vec3 ground = sky * 0.5f + u_cloudAmbient.rgb * 0.3f + sun * 0.12f * max(u_cloudLight.y, 0.0f);
	vec3 colour = vec3_splat(0.0f);
	float transmittance = 1.0f;
	float weightedDepth = 0.0f;
	float weights = 0.0f;
	// A far cloud is a few pixels across: past CLOUD_DETAIL_REACH the ray leaves out the small shapes and takes fewer
	// samples towards the light. From high up every ray starts far away, so the clouds cost less there.
	detail = detail && span.x < CLOUD_DETAIL_REACH;
	int lightSamples = detail ? 6 : 4;
	// Over the layer and under the anvils, in the clear air before the first cloud, the ray takes double steps. When a
	// double step finds cloud, the ray goes back half of it and takes a few single steps from there. The thin plumes of
	// the anvils get only single steps, so that no step jumps over them.
	float layerTop = u_cloudLayer.x + CLOUD_FIELD_DEPTH + 50.0f;
	float anvilLow = 1e9f;
	for (int k = 0; k < 4; ++k)
	{
		anvilLow = u_cloudCells[12 + k].z > 0.0f ? min(anvilLow, u_cloudCells[12 + k].w) : anvilLow;
	}
	bool doubled = false;
	int singles = 0;
	for (int i = 0; i < 160; ++i)
	{
		if (i >= steps || t > span.y || transmittance < 0.01f)
		{
			break;
		}
		// Past the end of a part, the ray goes on in the next part, and jumps over the clear air before it
		if (t > first.y)
		{
			if (second.x >= 1e9f)
			{
				break;
			}
			if (t < second.x)
			{
				t = second.x + dt * jitter;
				dt = clamp(t * growth, shortest, longest);
			}
			first = second;
			second = third;
			third = vec2(1e9f, 1e9f);
		}
		vec3 p = eye + dir * t;
		float altitude = CloudAltitude(p);
		// The clouds thin out over the last part of their reach, so that the layer has no hard edge there
		float far = 1.0f - smoothstep(u_cloudLayer.z * 0.7f, u_cloudLayer.z, t);
		float h;
		float rain;
		float shaft;
		float tower;
		float density =
		    CloudDensityAt(p, altitude, detail, far, h, rain, shaft, tower) * (1.0f - smoothstep(cellsFarthest * 0.7f, cellsFarthest, t));
		bool empty = density <= 0.0f && shaft <= 0.0f;
		if (!empty && doubled)
		{
			t -= dt;
			dt = clamp(t * growth, shortest, longest);
			doubled = false;
			singles = 4;
			continue;
		}
		if (shaft > 0.0f)
		{
			// The rain under a thunderstorm cell: a thin grey haze that the bottom of the cell shades
			float sigma = shaft * CB_RAIN_DENSITY * CLOUD_EXTINCTION;
			vec3 grey = (ground * 0.7f + sky * 0.25f) * 0.75f + CloudLightning(p, 1.0f) * 0.5f;
			float through = exp(-sigma * dt);
			colour += transmittance * grey * (1.0f - through);
			transmittance *= through;
		}
		if (density > 0.0f)
		{
			float sigma = density * CLOUD_EXTINCTION;
			// The sun's or the moon's light that comes through the cloud, scattered once and many times. A tower is
			// kilometres wide, so fewer samples farther apart find its shade.
			float lightDepth = tower > 0.5f ? CloudLightDepth(p, 4, 25.0f) : CloudLightDepth(p, lightSamples, 18.0f);
			vec3 direct = sun * CloudMultipleScattering(lightDepth, cosAngle) * CloudPowder(sigma * 40.0f, cosAngle) * 32.0f;
			// The sky lights the tops more than the bottoms; the bottoms of rain clouds are dark
			// The game darkens its sky and its haze in a storm, so the storm's clouds take more of the dim light they
			// have, to stay grey and not black
			vec3 ambient = mix(ground, sky, h * 0.7f + 0.3f) * (1.0f + rain * (1.3f - 0.4f * (1.0f - h)));
			if (tower > 0.5f)
			{
				// Inside a tower less of the sky's light comes down to the low parts
				ambient *= mix(0.75f, 1.0f, h);
			}
			else if (detail)
			{
				// The sky's light comes down through the cloud over the point: where the cloud is thinner, its
				// bottom is brighter
				float above = 0.0f;
				for (int k = 1; k <= 3; ++k)
				{
					vec3 up = p + vec3(0.0f, float(k * k) * 45.0f, 0.0f);
					above += CloudDensity(up, CloudAltitude(up), false) * float(k * 2 - 1) * 45.0f;
				}
				ambient *= 0.55f + 0.45f * exp(-above * CLOUD_EXTINCTION * 0.35f);
			}
			// The sun's light that a thick deck lets through after it scatters many times: grey and soft
			vec3 diffused = sun * 0.35f * exp(-lightDepth * 0.015f) * (0.5f + 0.5f * h);
			vec3 scattered = direct + ambient + diffused + CloudLightning(p, rain);
			float through = exp(-sigma * dt);
			// The light scattered over the step, as the step's own cloud dims it (energy-conserving)
			colour += transmittance * scattered * (1.0f - through);
			weightedDepth += t * transmittance * (1.0f - through);
			weights += transmittance * (1.0f - through);
			transmittance *= through;
		}
		doubled = empty && singles <= 0 && weights <= 0.0f && altitude > layerTop && altitude + 2.0f * dt * dir.y < anvilLow;
		singles -= 1;
		t += doubled ? 2.0f * dt : dt;
		dt = clamp(t * growth, shortest, longest);
		// The plumes of the anvils are thin: short steps there, so that each ray finds them
		if (altitude > anvilLow - 200.0f)
		{
			dt = min(dt, 450.0f);
		}
	}
	if (weights > 0.0f)
	{
		depth = weightedDepth / weights;
	}
	return vec4(colour, transmittance);
}

// A smooth noise from 0 to 1 over a point across the land, with cells of size 1
float CloudSmoothNoise(vec2 x)
{
	vec2 i = floor(x);
	vec2 f = x - i;
	vec2 u = f * f * (3.0f - 2.0f * f);
	vec4 corners = vec4(dot(i, vec2(127.1f, 311.7f)), dot(i + vec2(1.0f, 0.0f), vec2(127.1f, 311.7f)),
	                    dot(i + vec2(0.0f, 1.0f), vec2(127.1f, 311.7f)), dot(i + vec2(1.0f, 1.0f), vec2(127.1f, 311.7f)));
	vec4 h = fract(sin(mod(corners, 6283.0f)) * 43758.5453f);
	return mix(mix(h.x, h.y, u.x), mix(h.z, h.w, u.x), u.y);
}

// From high up a cloud is less than a pixel deep, so the layer is a thin sheet: its cover where the ray meets the middle
// of the layer, lit by the sun and the sky
vec4 CloudSheet(vec3 eye, vec3 dir, float limit, vec3 sky, out float depth)
{
	depth = min(limit, u_cloudLayer.z);
	float middle = u_cloudLayer.x + 0.5f * CLOUD_CUMULUS_DEPTH;
	vec2 under = CloudUnder(eye.y - middle, dir);
	float t = under.x;
	if (under.x >= under.y || t <= 0.0f || t > min(limit, u_cloudLayer.z))
	{
		return vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}
	vec3 p = eye + dir * t;
	float type;
	float rain;
	// A pixel far away covers many cells of cloud: there the sheet shows the mean cover of the wide weather, so the far
	// layer has no grain
	float footprint = t * 0.0012f;
	float fine = 1.0f - smoothstep(60.0f, 600.0f, footprint);
	// The large weather systems, hundreds of kilometres across, from a smooth noise with no repeat
	vec2 q = p.xz - u_cloudDrift.xy;
	// Each octave turns on its own axes, so that no grid shows
	vec2 turned = vec2(0.8f * q.x - 0.6f * q.y, 0.6f * q.x + 0.8f * q.y);
	vec2 other = vec2(0.28f * q.x + 0.96f * q.y, -0.96f * q.x + 0.28f * q.y);
	float systems = 0.5f * CloudSmoothNoise(q / 700000.0f) + 0.3f * CloudSmoothNoise(turned / 260000.0f + 7.3f) +
	                0.2f * CloudSmoothNoise(other / 90000.0f + 3.1f);
	float mean = clamp(u_cloudCover.x * (0.3f + 1.4f * systems) - 0.15f, 0.0f, 1.0f) * smoothstep(0.0f, 0.05f, u_cloudCover.x);
	float cover = fine > 0.0f ? mix(mean, CloudWeatherAt(p.xz, type, rain), fine) : mean;
	// The anvils of the thunderstorm cells show over the layer
	if (CloudHasCells())
	{
		float soft;
		cover = max(cover, CumulonimbusAt(p, CB_TOP - 600.0f, false, soft).x);
	}
	cover *= 1.0f - smoothstep(u_cloudLayer.z * 0.7f, u_cloudLayer.z, t);
	float shown = smoothstep(0.05f, 0.6f, cover);
	vec3 sun = u_cloudLightColour.rgb * u_cloudLightColour.w;
	vec3 colour = sun * (0.25f + 0.75f * clamp(u_cloudLight.y * 3.0f, 0.0f, 1.0f)) * 0.9f + sky * 0.6f;
	depth = t;
	return vec4(colour * shown, 1.0f - shown);
}

// The clouds along a ray (MarchCloudVolume). As the eye climbs to space the layer turns into a thin sheet, which costs
// one sample and has no grain far away.
vec4 MarchClouds(vec3 eye, vec3 dir, float limit, float jitter, int steps, bool detail, vec3 sky, out float depth)
{
	float fromAbove = smoothstep(CLOUD_SHEET_LOW, CLOUD_SHEET_HIGH, eye.y - u_cloudLayer.w);
	float sheetDepth;
	vec4 sheet = fromAbove > 0.0f ? CloudSheet(eye, dir, limit, sky, sheetDepth) : vec4(0.0f, 0.0f, 0.0f, 1.0f);
	if (fromAbove >= 1.0f)
	{
		depth = sheetDepth;
		return sheet;
	}
	vec4 volume = MarchCloudVolume(eye, dir, limit, jitter, steps, detail, sky, depth);
	if (fromAbove > 0.0f)
	{
		depth = mix(depth, sheetDepth, fromAbove);
		return mix(volume, sheet, fromAbove);
	}
	return volume;
}

// How much of a cloud at a distance along a ray the haze hides, 0 to 1. The air thins with the height, so a path high
// over the land holds less haze than one near the sea.
float CloudHaze(vec3 eye, vec3 dir, float depth)
{
	float low = max(eye.y - u_cloudLayer.w, 0.0f) / CLOUD_HAZE_HEIGHT;
	float high = max(eye.y + dir.y * depth - u_cloudLayer.w, 0.0f) / CLOUD_HAZE_HEIGHT;
	float rise = high - low;
	float meanAir = abs(rise) > 1e-3f ? (exp(-low) - exp(-high)) / rise : exp(-low);
	return 1.0f - exp(-depth * meanAir / CLOUD_HAZE_REACH);
}

// Where a point of the screen (0 to 1, as the textures are read) is in the world, at a depth of the depth buffer
vec3 CloudWorldAt(vec2 uv, float depth)
{
#if BGFX_SHADER_LANGUAGE_GLSL
	vec3 ndc = vec3(uv * 2.0f - 1.0f, depth * 2.0f - 1.0f);
#else
	vec3 ndc = vec3(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth);
#endif
	vec4 p = mul(u_invViewProj, vec4(ndc, 1.0f));
	return p.xyz / p.w;
}

// Where a point of the world is on the screen of a view-projection (xy, 0 to 1 as the textures are read); z is less
// than 0 behind the eye
vec3 CloudScreenAt(mat4 viewProj, vec3 world)
{
	vec4 clip = mul(viewProj, vec4(world, 1.0f));
	vec2 ndc = clip.xy / max(abs(clip.w), 1e-6f);
#if BGFX_SHADER_LANGUAGE_GLSL
	return vec3(ndc * 0.5f + 0.5f, clip.w);
#else
	return vec3(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f, clip.w);
#endif
}

// The direction of the ray from the eye through a point of the screen
vec3 CloudRayAt(vec2 uv)
{
	return ViewRayAt(uv);
}

// How far the scene is from the eye at a depth of the depth buffer; very far where the screen shows the sky
float CloudSceneDistance(vec2 uv, float depth)
{
	if (depth <= 1e-7f)
	{
		return 1e9f;
	}
	return length(CloudWorldAt(uv, depth) - u_cloudEye.xyz);
}

bool CloudOnDome(vec3 dir, float reach)
{
	vec3 p = u_cloudEye.xyz + dir * reach;
	return OnSkyDome(p);
}

// How far along a ray from the eye the lowest cloud can be: the ray meets the lowest bottom of the layer there, on the
// curved world. A scene nearer than this has no cloud in front of it. 0 when the eye is in or over the layer.
float CloudNearestAlong(vec3 dir)
{
	float lowest = u_cloudLayer.x - CLOUD_LOWEST_DROP - 60.0f;
	float below = lowest - u_cloudEye.y;
	if (below <= 0.0f)
	{
		return 0.0f;
	}
	// The eye is under the lowest bottom: the span under it starts behind the eye and ends where the ray meets it
	float nearest = max(CloudUnder(-below, dir).y, 0.0f);
	// The rain under a thunderstorm cell comes down to the ground
	if (CloudHasCells())
	{
		vec2 towers;
		vec2 anvils;
		CumulonimbusSpans(u_cloudEye.xyz, dir, 1e9f, towers, anvils);
		nearest = towers.x < towers.y ? min(nearest, towers.x) : nearest;
	}
	return nearest;
}

#endif // CLOUDS_SH
