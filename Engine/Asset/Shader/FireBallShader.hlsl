
cbuffer PerObject : register(b1)
{
    row_major float4x4 gWorld;
    row_major float4x4 gViewProj;
    // ...
}

// b2: FireBall 파라미터
cbuffer FireBallCB : register(b2)
{
    float3 gColor;
    float gIntensity; // 색/세기
    
    float3 gCenterWS;
    float gRadius; // 중심/반경(월드)
    
    float4 gCenterClip; // CPU에서 mul(float4(gCenterWS,1), ViewProj)
    
    float gProjRadiusNDC; // CPU에서 계산한 "NDC 반경"
    float gFeather;
    float gHardness;
    float _pad;
};

struct VSIn { float3 pos : POSITION; };

struct VSOut { float4 Position : SV_POSITION; float3 WorldPos : TEXCOORD0; };

VSOut VS_Sphere(VSIn i)
{
    VSOut o;
    float4 wpos = mul(float4(i.pos, 1.0), gWorld);
    o.WorldPos = wpos.xyz;
    o.Position = mul(wpos, gViewProj);
    return o;
}

// 화면 공간 원형 falloff (NDC에서 중심까지 거리 기반)
// feather: 0~1 (가장자리 폭), hardness: 1.5~3 추천
float SmoothCircleNDC(float2 ndc, float2 centerNdc, float projRadiusNdc, float feather, float hardness)
{
    // NDC 거리(0=중심, projRadiusNdc=외곽)
    float r = length(ndc - centerNdc);
    float x = r / max(projRadiusNdc, 1e-5);

    float edge0 = 1.0 - saturate(feather); // 0.6~0.8 권장
    float t = saturate((x - edge0) / max(1.0 - edge0, 1e-5));
    return pow(1.0 - t, hardness);
}

float4 PS_Sphere(VSOut i) : SV_Target
{
    float d = distance(i.WorldPos, gCenterWS);
    float R0 = gRadius * (1.0 - saturate(gFeather));
    float t  = saturate((d - R0) / max(gRadius - R0, 1e-5));
    float a  = pow(1.0 - t, max(gHardness, 0.1));
    return float4(gColor * (gIntensity * a), 1.0);
}



// Default entry points expected by factory
VSOut mainVS(VSIn i) { return VS_Sphere(i); }
float4 mainPS(VSOut i) : SV_Target { return PS_Sphere(i); }
