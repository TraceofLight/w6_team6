cbuffer PerObject : register(b1)
{
    row_major float4x4 gWorld;
    row_major float4x4 gViewProj;
}

cbuffer FireBallCB : register(b2)
{
    float3 gColor;   float gIntensity;
    float3 gCenterWS; float gRadius;
    float4 gCenterClip; // CPU: mul(float4(gCenterWS,1), ViewProj)
    float gProjRadiusNDC;
    float gFeather;
    float gHardness;
    float _pad;
}

cbuffer InvViewProjCB : register(b3)
{
    row_major float4x4 gInvViewProj;
}

Texture2D DepthTex : register(t5);
SamplerState DepthSamp : register(s5);

struct VS_INPUT
{
    float3 Position : POSITION;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION; // Post-viewport position (pixel space)
    float2 Ndc      : TEXCOORD0;   // Clip-space NDC
};

PS_INPUT VS_Sphere(VS_INPUT i)
{
    PS_INPUT o;
    float4 wpos = mul(float4(i.Position, 1.0), gWorld);
    float4 clip = mul(wpos, gViewProj);
    o.Position = clip;
    o.Ndc = clip.xy / clip.w;
    return o;
}

// Simple smooth circle in NDC for projected falloff
float SmoothCircleNDC(float2 ndc, float2 centerNdc, float projRadiusNdc, float feather, float hardness)
{
    float r = length(ndc - centerNdc);
    float x = r / max(projRadiusNdc, 1e-5);
    return saturate(1.0 - x);
}

float4 PS_Sphere(PS_INPUT i) : SV_Target
{
    // Map current pixel to Scene RT UV using SV_Position and texture size
    uint w, h;
    DepthTex.GetDimensions(w, h);
    float2 sceneSize = float2(w, h);
    float2 uv = i.Position.xy / sceneSize;

    // Reconstruct world position from depth (match fog convention)
    float depth = DepthTex.SampleLevel(DepthSamp, uv, 0).r;
    float2 screen = uv * 2.0 - 1.0; // to NDC
    screen.y = -screen.y;           // DX clip-space Y flip
    float4 clip = float4(screen, depth, 1.0);
    float4 world = mul(clip, gInvViewProj);
    world /= world.w;

    // 3D volume test (world-space)
    float d = distance(world.xyz, gCenterWS);
    if (d > gRadius)
    {
        discard; // outside light volume
    }

    // 3D falloff in world-space (soft edge using feather)
    float R0 = gRadius * (1.0 - saturate(gFeather));
    float t = saturate((d - R0) / max(gRadius - R0, 1e-5));
    float a3d = pow(1.0 - t, max(1.0, gHardness));

    // 2D projected falloff (optional screen-space shaping)
    float2 cndc = gCenterClip.xy / gCenterClip.w;
    float a2d = SmoothCircleNDC(i.Ndc, cndc, gProjRadiusNDC, gFeather, gHardness);

    float a = a3d * a2d;
    return float4(gColor * (gIntensity * a), 1.0);
    //return float4(depth, depth, depth, 1.0);
}

PS_INPUT mainVS(VS_INPUT i)
{
    return VS_Sphere(i);
}

float4 mainPS(PS_INPUT i) : SV_Target
{
    return PS_Sphere(i);
    //return float4(1, 0, 1, 1);
}
