cbuffer constants : register(b0)
{
    row_major float4x4 gWorld;
}

cbuffer PerFrame : register(b1)
{
    row_major float4x4 View;
    row_major float4x4 Projection;
}

cbuffer FireBallCB : register(b2)
{
    float3 gColor;   float gIntensity;
    float3 gCenterWS; float gRadius;
    float4 gCenterClip; // unused
    float gProjRadiusNDC; float gFeather; float gHardness; float _pad;
}

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
    float2 tex      : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position  : SV_POSITION;
    float3 worldPos  : TEXCOORD0;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT o;
    float4 wpos = mul(float4(input.position, 1.0f), gWorld);
    o.position = mul(mul(wpos, View), Projection);
    o.worldPos = wpos.xyz;
    return o;
}

float4 mainPS(PS_INPUT i) : SV_Target
{
    // World-space spherical volume test
    float d = distance(i.worldPos, gCenterWS);
    if (d > gRadius)
    {
        discard; // outside the light volume; no contribution
    }

    // Soft falloff using feather and hardness
    float R0 = gRadius * (1.0 - saturate(gFeather));
    float t = saturate((d - R0) / max(gRadius - R0, 1e-5));
    float a3d = pow(1.0 - t, max(1.0, gHardness));

    float a = a3d;
    return float4(gColor * (gIntensity * a), a);
}

