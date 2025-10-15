cbuffer PerObject : register(b0)
{
    row_major float4x4 World;
};

cbuffer PerFrame : register(b1)
{
    row_major float4x4 View;
    row_major float4x4 Projection;
};

cbuffer LightProperties : register(b2)
{
    float3 LightPosition;       // 월드 공간 라이트 위치 (offset 0)
    float Intensity;            // 조명 강도 (offset 12)

    float3 LightColor;          // RGB 색상 (offset 16)
    float Radius;               // 영향 반경 (offset 28)

    float3 CameraPosition;      // 카메라 위치 (offset 32)
    float RadiusFalloff;        // 감쇠 지수 (offset 44)

    float2 ViewportTopLeft;     // Viewport 시작 위치 (offset 48)
    float2 ViewportSize;        // Viewport 크기 (offset 56)

    float2 SceneRTSize;         // Scene RT 전체 크기 (offset 64)
    float2 Padding2;            // PADDING (offset 72)

    row_major float4x4 InverseViewProj;  // World Position 재구성용 (offset 80)
};

// Scene Depth Texture (PostProcess에서 생성된 것)
Texture2D SceneDepthTexture : register(t0);
SamplerState DepthSampler : register(s0);

struct VS_INPUT
{
    float4 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 TexCoord : TEXCOORD;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;     // Light Volume의 월드 위치
    float4 ScreenPos : TEXCOORD1;    // Screen Position for depth sampling
};

// Vertex Shader: Sphere를 라이트 위치/반경으로 변환
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT Output;

    // Sphere Vertex를 월드 공간으로 변환
    float4 WorldPosition = mul(input.Position, World);
    Output.WorldPos = WorldPosition.xyz;

    // View-Projection 변환
    float4 viewPos = mul(WorldPosition, View);
    Output.Position = mul(viewPos, Projection);

    // Screen Position 저장 (Depth 샘플링용)
    Output.ScreenPos = Output.Position;

    return Output;
}

// World Position 재구성 함수
float3 ReconstructWorldPosition(float2 ViewportUV, float Depth)
{
    // PostProcess shader와 동일한 방식 사용
    // viewportUV는 viewport 내에서의 UV (0~1)

    // Viewport UV -> NDC space (-1~1)
    float2 ScreenPosition = ViewportUV * 2.0 - 1.0;
    ScreenPosition.y = -ScreenPosition.y;  // DX11의 Y축 반전 처리 (UV는 top-down, NDC는 bottom-up)

    // Clip space position 구성
    float4 ClipPosition = float4(ScreenPosition, Depth, 1.0);

    // Inverse View-Projection으로 world space 변환
    float4 WorldPosition = mul(ClipPosition, InverseViewProj);

    // Perspective divide
    WorldPosition.xyz /= WorldPosition.w;

    return WorldPosition.xyz;
}

// Pixel Shader: Additive Point Light 계산
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // Viewport 내에서의 UV 계산
    float2 ViewportUV = input.ScreenPos.xy / input.ScreenPos.w;
    ViewportUV = ViewportUV * 0.5 + 0.5;
    ViewportUV.y = 1.0 - ViewportUV.y;

    // PostProcess shader와 동일: Viewport UV -> Scene RT UV 변환
    float2 sceneUV = (ViewportTopLeft + ViewportUV * ViewportSize) / SceneRTSize;

    // Scene의 depth texture에서 깊이 샘플링 (Scene RT 전체 좌표 사용)
    float Depth = SceneDepthTexture.Sample(DepthSampler, sceneUV).r;

    // Scene geometry의 실제 world position 재구성 (Viewport UV 사용)
    float3 SceneWorldPosition = ReconstructWorldPosition(ViewportUV, Depth);

    // Scene geometry가 Light 중심으로부터 얼마나 떨어져 있는지 계산
    float3 LightToScene = SceneWorldPosition - LightPosition;
    float DistFromLight = length(LightToScene);

    // Radius 밖이면 조명 없음
    if (DistFromLight > Radius)
    {
        discard;
    }

    // 거리 감쇠 처리, DecalShader에서 썼던 형태로 구현
    float NormalizedDistance = DistFromLight / Radius;
    float Attenuation = 1.0 - (NormalizedDistance * NormalizedDistance);

    // Additive Light 계산 (Intensity로 Light 세기 조정)
    float3 lightContribution = LightColor * Intensity * Attenuation;

    // Alpha도 attenuation에 따라 조절 (블랜딩 강도 제어)
    return float4(lightContribution, Attenuation);
}
