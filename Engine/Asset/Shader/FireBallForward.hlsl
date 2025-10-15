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
    float4 gCenterClip;
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
    float3 worldNrm  : TEXCOORD1;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT o;
    float4 wpos = mul(float4(input.position, 1.0f), gWorld);
    o.position = mul(mul(wpos, View), Projection);
    o.worldPos = wpos.xyz;
    float3 n = input.normal;
    if (dot(n, n) <= 1e-6)
    {
        n = normalize(input.position);
    }
    float3x3 m = (float3x3)gWorld;
    o.worldNrm = normalize(mul(n, m));
    return o;
}

float4 mainPS(PS_INPUT i) : SV_Target
{
    float d = distance(i.worldPos, gCenterWS);
    
    if (d > gRadius)
    {
        discard;
    }

    float R0 = gRadius * (1.0 - saturate(gFeather));
    float t = saturate((d - R0) / max(gRadius - R0, 1e-5));
    //float a3d = pow(1.0 - t, max(1.0, gHardness));
    
    float a3d = pow(1 - t, 2);

    float normD = d / max(gRadius, 1e-5);
    float attPhys = 1.0 / (1.0 + normD * normD);

    float3 Ldir = normalize(gCenterWS - i.worldPos);
    float len2 = dot(i.worldNrm, i.worldNrm);
    if (len2 > 1e-6)
    {
        float nl = dot(normalize(i.worldNrm), Ldir);
        if (nl <= 0.0f) discard;
        a3d *= nl;
    }
    float a = a3d * attPhys;
    return float4(gColor * (gIntensity * a), 1.0);
}
