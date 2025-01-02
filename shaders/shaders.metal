#include <metal_stdlib>

using namespace metal;

// MARK util impls

float2 get_unit_vertex(uint vertex_id) {
	return float2(float(vertex_id & 1u), 0.5 * float(vertex_id & 2u));
}

float4 to_device_pos(float2 pos, float2 viewport_size) {
	float2 device_pos = pos / viewport_size * float2(2, -2) + float2(-1, 1);
	return float4(device_pos, 0, 1);
}

// MARK shared defs

struct Globals {
	float2 viewport_size;
};

// MARK quad impls

struct Quad {
	float2 origin, size;
	float4 background_color;
};

struct QuadVertex {
	float4 position [[position]];
	float4 background_color;
};

vertex QuadVertex quad_vertex(
	uint vertex_id [[vertex_id]],
	uint quad_id [[instance_id]],
	constant Globals &globals [[buffer(0)]],
	constant Quad *quads [[buffer(1)]]
) {
	float2 unit_vertex = get_unit_vertex(vertex_id);
	Quad quad = quads[quad_id];

	float2 pos = unit_vertex * quad.size + quad.origin;

	return QuadVertex{
		to_device_pos(pos, globals.viewport_size),
		quad.background_color
	};
}

fragment float4 quad_fragment(QuadVertex in [[stage_in]]) {
	return in.background_color;
}

// MARK sprite impls

struct Sprite {
	float2 origin, size;
	float2 uv_origin, uv_size;
	float4 color;
};

struct SpriteVertex {
	float4 position [[position]];
	float2 uv;
	float4 color;
};

vertex SpriteVertex sprite_vertex(
	uint vertex_id [[vertex_id]],
	uint sprite_id [[instance_id]],
	constant Globals &globals [[buffer(0)]],
	constant Sprite *sprites [[buffer(1)]]
) {
	float2 unit_vertex = get_unit_vertex(vertex_id);
	Sprite sprite = sprites[sprite_id];

	float2 pos = unit_vertex * sprite.size + sprite.origin;
	float2 uv = unit_vertex * sprite.uv_size + sprite.uv_origin;

	return SpriteVertex{
		to_device_pos(pos, globals.viewport_size),
		uv,
		sprite.color
	};
}

constexpr sampler sprite_sampler(filter::linear, address::clamp_to_edge, lod_clamp(0, 32));

fragment float4 sprite_fragment(
	SpriteVertex in [[stage_in]],
	texture2d<float> atlas [[texture(0)]]
) {
	float4 sample = atlas.sample(sprite_sampler, in.uv);
	float4 color = in.color;
	color.a *= sample.a;
	return color;
}
