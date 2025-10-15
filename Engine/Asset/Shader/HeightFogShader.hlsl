cbuffer FogConstants : register(b0)
{
    float FogDensity;
    float FogHeightFalloff;
    float StartDistance;
    float FogCutoffDistance;

    float FogMaxOpacity;
    float3 FogInscatteringColor;

    float3 CameraPosition;
    float FogHeight;

    float2 ViewportTopLeft;
    float2 ViewportSize;
    float2 SceneRTSize;
    float2 Padding2;

    // Inverse View-Projection Matrix
    row_major float4x4 InvViewProj;
};

// Scene Textures
Texture2D SceneColorTexture : register(t0);
Texture2D SceneDepthTexture : register(t1);
SamplerState SceneSampler : register(s0);

struct VS_INPUT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct PS_OUTPUT
{
    float4 Color : SV_TARGET;
    float Depth : SV_DEPTH;  // Depth를 백버퍼 DSV로 출력
};

// Fullscreen Quad
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.Position = float4(input.Position, 1.0f);
    output.TexCoord = input.TexCoord;
    return output;
}

// Full Post-Process Fog
PS_OUTPUT mainPS(PS_INPUT input)
{
    PS_OUTPUT Output;

    // Convert viewport TexCoord to Scene RT UV
    // input.TexCoord는 현재 viewport 내에서 (0,0)~(1,1)
    // Scene RT UV로 변환: (ViewportTopLeft + TexCoord * ViewportSize) / SceneRTSize
    float2 sceneUV = (ViewportTopLeft + input.TexCoord * ViewportSize) / SceneRTSize;

    // Sample Scene Color and Depth (Scene RT UV 사용)
    float4 SceneColor = SceneColorTexture.Sample(SceneSampler, sceneUV);
    float Depth = SceneDepthTexture.Sample(SceneSampler, sceneUV).r;

    // Reconstruct World Position from Depth
    // NDC 좌표 계산 (Scene RT 전체 기준 UV 사용)
    float2 screenPos = sceneUV * 2.0f - 1.0f;  // sceneUV는 전체 Scene RT 기준 (0,0)~(1,1)
    screenPos.y = -screenPos.y;  // Flip Y for DirectX

    // Clip Space Position
    float4 clipPos = float4(screenPos, Depth, 1.0f);

    // World Space Position (Inverse View-Projection)
    float4 worldPos = mul(clipPos, InvViewProj);
    worldPos /= worldPos.w;  // Perspective divide

    // Calculate Distance from Camera
    float3 worldPosition = worldPos.xyz;
    float DistanceFromCamera = length(worldPosition - CameraPosition);

    // Apply Start Distance
    float fogDistance = max(0.0f, DistanceFromCamera - StartDistance);

    // Exponential Height Fog - Line Integral (UE5 방식)
    // Z-up LH 좌표계: Z축이 높이 (Up/Down)
    // 카메라에서 픽셀까지의 ray를 따라 안개 밀도를 적분

    float CameraHeight = CameraPosition.z;
    float PixelHeight = worldPosition.z;

    // Line Integral 계산
    // ∫[Camera → Pixel] exp(-(h - FogHeight) * FogHeightFalloff) dh

    float Falloff = FogHeightFalloff * (PixelHeight - CameraHeight);
    float LineIntegral;

    if (abs(Falloff) > 0.01f)
    {
        // Ray가 경사져 있음 - Exponential 적분 사용
        // ∫ exp(-k * h) dh = -exp(-k * h) / k
        float ExpStart = exp(-FogHeightFalloff * (CameraHeight - FogHeight));
        float ExpEnd = exp(-FogHeightFalloff * (PixelHeight - FogHeight));
        LineIntegral = (ExpStart - ExpEnd) / Falloff;
    }
    else
    {
        // Ray가 거의 수평 - 평균 높이 사용 (Taylor series approximation)
        float AvgHeight = (CameraHeight + PixelHeight) * 0.5f;
        LineIntegral = exp(-FogHeightFalloff * (AvgHeight - FogHeight));
    }

    // Line Integral에 거리와 밀도 적용
    float FogAmount = FogDensity * LineIntegral * fogDistance;
    float FogFactor = 1.0f - exp(-FogAmount);

    // Apply Fog Cutoff Distance
    if (DistanceFromCamera > FogCutoffDistance)
    {
        FogFactor = 1.0f;  // 최대 거리 이상은 완전히 안개
    }

    // Clamp to Max Opacity
    FogFactor = saturate(FogFactor);
    FogFactor = min(FogFactor, FogMaxOpacity);

    // Lerp between Scene Color and Fog Color
    float3 FinalColor = lerp(SceneColor.rgb, FogInscatteringColor, FogFactor);

    // Output Final Color and Depth
    Output.Color = float4(FinalColor, 1.0f);
    Output.Depth = Depth;  // Scene Depth를 백버퍼 DSV로 복사

    return Output;
}
