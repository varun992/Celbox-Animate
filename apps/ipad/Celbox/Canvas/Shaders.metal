#include <metal_stdlib>
using namespace metal;

// Matches CbxVertex: 6 tightly packed floats.
struct CbxVertexIn {
    packed_float2 position;  // screen pixels
    packed_float4 color;     // straight alpha
};

struct VSOut {
    float4 position [[position]];
    float4 color;
};

vertex VSOut celbox_vertex(const device CbxVertexIn* vertices [[buffer(0)]],
                           constant float2& viewport [[buffer(1)]],
                           uint vid [[vertex_id]]) {
    CbxVertexIn v = vertices[vid];
    float2 p = float2(v.position);
    VSOut out;
    out.position = float4(p.x / viewport.x * 2.0 - 1.0, 1.0 - p.y / viewport.y * 2.0, 0.0, 1.0);
    out.color = float4(v.color);
    return out;
}

fragment float4 celbox_fragment(VSOut in [[stage_in]]) {
    return float4(in.color.rgb * in.color.a, in.color.a);  // premultiplied output
}
