cbuffer PerObject : register(b1)
{
    row_major float4x4 gWorld;
    row_major float4x4 gViewProj;
}

cbuffer FireBallCB : register(b2)
{
    float3 gColor;
    float gIntensity;
    float3 gCenterWS;
    float gRadius;
    float4 gCenterClip;
    
    float2 ViewportTopLeft;
    float2 ViewportSize;
    
    float2 SceneRTSize;
    
    float gProjRadiusNDC;
    float gFeather;
    
    float gHardness;
    float3 _pad;
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
    float4 Position : SV_POSITION; // 래스터용
    float2 Ndc : TEXCOORD0; // 우리가 계산해서 넘기는 NDC
};

PS_INPUT VS_Sphere(VS_INPUT i)
{
    PS_INPUT o;
    float4 wpos = mul(float4(i.Position, 1.0), gWorld); // 구의 월드 상 위치
    float4 clip = mul(wpos, gViewProj);                 // 구의 클립 공간 상 위치
    o.Position = clip;
    o.Ndc = clip.xy / clip.w;                           // 구의 점의 ndc
    return o;
}

float SmoothCircleNDC(float2 ndc, float2 centerNdc, float projRadiusNdc, float feather, float hardness)
{
    float r = length(ndc - centerNdc);
    float x = r / max(projRadiusNdc, 1e-5);
    //float edge0 = 1.0 - saturate(feather);
    //float t = saturate((x - edge0) / max(1.0 - edge0, 1e-5));
    return saturate(1.0 - x);
}

float3 ReconstructToWorldPos(float2 ndc, float depth01)
{
    float4 pNearH = mul(float4(ndc, 0.0f, 1.0f), gInvViewProj);
    float3 pNear = pNearH.xyz / pNearH.w;

    float4 pFarH = mul(float4(ndc, 1.0f, 1.0f), gInvViewProj);
    float3 pFar = pFarH.xyz / pFarH.w;

    return lerp(pNear, pFar, depth01);
}

float4 PS_Sphere(PS_INPUT i) : SV_Target
{
    float2 ndc = i.Ndc;
    float2 uv = ndc * 0.5 + 0.5;
    uv.y = 1 - uv.y;
   
    // 0..1 (D3D NDC)
    float depth01 = DepthTex.SampleLevel(DepthSamp, uv, 0).r;

    // 배경(미히트) early-out (역Z 쓰면 조건 반대로)
    if (depth01 > 0.9999f)
        discard;

    // 월드 위치 복원 (견고)
    float3 worldPos = ReconstructToWorldPos(ndc, depth01);

    // 3D 구 반경 마스크
    float distToCenter = distance(worldPos, gCenterWS);
    if (distToCenter >= gRadius)
        return 0; // 완전 밖이면 바로 종료

    float R0 = gRadius * (1.0 - saturate(gFeather));
    float t = saturate((distToCenter - R0) / max(gRadius - R0, 1e-5));
    float a3d = 1.0 - t;

    // 2D 화면 마스크 (NDC 기준)
    float2 cndc = gCenterClip.xy / gCenterClip.w;
    //float a2d = SmoothCircleNDC(ndc, cndc, gProjRadiusNDC, gFeather, gHardness);

    //float a = a3d * a2d;
    float a = a3d;

    return float4(gColor * (gIntensity * a), 1.0);
}

PS_INPUT mainVS(VS_INPUT i)
{
    return VS_Sphere(i);
}

float4 mainPS(PS_INPUT i) : SV_Target
{
    return PS_Sphere(i);
   // return float4(1, 0, 1, 1);
}