cbuffer FrameConstants : register(b0)
{
    row_major float4x4 gViewProjection[2];
    float4 gWorkload; // x = pixel iterations, y = vertex iterations
};

cbuffer ObjectConstants : register(b1)
{
    row_major float4x4 gModel;
    float4 gObjectColor;
};

cbuffer EyeConstants : register(b2)
{
    uint gEyeIndex;
    uint3 gEyePadding;
};

struct ObjectData
{
    row_major float4x4 model;
    float4 color;
};

StructuredBuffer<ObjectData> gObjects : register(t0);

struct VertexInput
{
    float3 position : POSITION;
    float3 color : COLOR0;
};

struct PixelInput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
    float seamClip : SV_ClipDistance0;
};

struct WorldOutput
{
    float4 worldPosition : WORLDPOS;
    float4 color : COLOR0;
};

struct GeometryOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
    float seamClip : SV_ClipDistance0;
    // Keep pixel-consumed fields in the same declaration order as PixelInput.
    // Otherwise the D3D11 linker assigns SV_ClipDistance a different hardware
    // register even though the semantic names match.
    uint viewport : SV_ViewportArrayIndex;
};

float3 ApplyVertexWork(float3 position)
{
    float3 value = position;
    [loop]
    for (uint i = 0; i < (uint)gWorkload.y; ++i)
    {
        float phase = value.x * 0.17 + value.y * 0.11 + value.z * 0.07 + (float)i * 0.013;
        value += float3(sin(phase), cos(phase * 1.1), sin(phase * 0.9)) * 0.000001;
    }
    return value;
}

PixelInput VSReference(VertexInput input)
{
    PixelInput output;
    float4 world = mul(float4(ApplyVertexWork(input.position), 1.0), gModel);
    output.position = mul(world, gViewProjection[gEyeIndex]);
    output.color = float4(input.color, 1.0) * gObjectColor;
    output.seamClip = 1.0;
    return output;
}

PixelInput VSInstancedStereo(VertexInput input, uint combinedInstance : SV_InstanceID)
{
    PixelInput output;
    uint objectIndex = combinedInstance / 2;
    uint eyeIndex = combinedInstance % 2;
    ObjectData objectData = gObjects[objectIndex];
    float4 world = mul(float4(ApplyVertexWork(input.position), 1.0), objectData.model);
    float4 clip = mul(world, gViewProjection[eyeIndex]);
    float originalX = clip.x;

    // Pack the two eye clip volumes into the left/right halves of one viewport.
    // This route is core D3D11 and does not depend on optional VS viewport routing.
    clip.x = clip.x * 0.5 + (eyeIndex == 0 ? -0.5 : 0.5) * clip.w;
    output.position = clip;
    output.color = float4(input.color, 1.0) * objectData.color;
    // Full-viewport packing removes the inner clip plane; restore it explicitly
    // so primitives crossing the left/right seam match two native eye viewports.
    output.seamClip = eyeIndex == 0 ? clip.w - originalX : originalX + clip.w;
    return output;
}

WorldOutput VSWorld(VertexInput input, uint objectIndex : SV_InstanceID)
{
    WorldOutput output;
    ObjectData objectData = gObjects[objectIndex];
    output.worldPosition = mul(float4(ApplyVertexWork(input.position), 1.0), objectData.model);
    output.color = float4(input.color, 1.0) * objectData.color;
    return output;
}

[maxvertexcount(6)]
void GSStereo(triangle WorldOutput input[3], inout TriangleStream<GeometryOutput> stream)
{
    [unroll]
    for (uint eye = 0; eye < 2; ++eye)
    {
        [unroll]
        for (uint vertex = 0; vertex < 3; ++vertex)
        {
            GeometryOutput output;
            output.position = mul(input[vertex].worldPosition, gViewProjection[eye]);
            output.color = input[vertex].color;
            output.viewport = eye;
            output.seamClip = 1.0;
            stream.Append(output);
        }
        stream.RestartStrip();
    }
}

float4 PSMain(PixelInput input) : SV_Target
{
    float3 color = saturate(input.color.rgb);
    [loop]
    for (uint i = 0; i < (uint)gWorkload.x; ++i)
    {
        float phase = dot(color, float3(0.31, 0.47, 0.73)) + (float)i * 0.071;
        color = frac(color * 1.013 + sin(phase) * 0.017 + float3(0.003, 0.005, 0.007));
    }
    return float4(color, 1.0);
}
