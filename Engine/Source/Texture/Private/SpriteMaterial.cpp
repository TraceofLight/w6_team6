#include "pch.h"
#include "Texture/Public/SpriteMaterial.h"

#include <algorithm>

IMPLEMENT_CLASS(USpriteMaterial, UMaterial)

namespace
{
	constexpr float KMinimumFPS = 0.1f;
}

USpriteMaterial::USpriteMaterial()
	: UMaterial()
{
	if (bAutoPlay)
	{
		bIsPlaying = true;
	}
}

USpriteMaterial::USpriteMaterial(const FName& InName)
	: UMaterial(InName)
{
	if (bAutoPlay)
	{
		bIsPlaying = true;
	}
}

void USpriteMaterial::SetSubUVParams(int32 InRows, int32 InCols, float InFPS)
{
	SubUVRows = std::max(1, InRows);
	SubUVCols = std::max(1, InCols);
	TotalFrames = std::max(1, SubUVRows * SubUVCols);
	SetFrameRate(InFPS);
	ClampCurrentFrame();
}

void USpriteMaterial::SetCurrentFrame(int32 InFrame)
{
	CurrentFrame = std::clamp(InFrame, 0, std::max(0, TotalFrames - 1));
	FrameTimeAccumulator = 0.0f;
}

void USpriteMaterial::SetLooping(bool bInLoop)
{
	bLooping = bInLoop;
}

void USpriteMaterial::SetAutoPlay(bool bInAutoPlay)
{
	bAutoPlay = bInAutoPlay;
	if (bAutoPlay && !bIsPlaying)
	{
		Play();
	}
}

void USpriteMaterial::SetFrameRate(float InFPS)
{
	FrameRate = std::max(KMinimumFPS, InFPS);
}

void USpriteMaterial::Play()
{
	bIsPlaying = true;
}

void USpriteMaterial::Pause()
{
	bIsPlaying = false;
}

void USpriteMaterial::Stop()
{
	bIsPlaying = false;
	CurrentFrame = 0;
	FrameTimeAccumulator = 0.0f;
}

void USpriteMaterial::Reset()
{
	CurrentFrame = 0;
	FrameTimeAccumulator = 0.0f;
}

void USpriteMaterial::Update(float DeltaTime)
{
	if (!bAutoPlay || !bIsPlaying || TotalFrames <= 1)
	{
		return;
	}

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	FrameTimeAccumulator += DeltaTime;
	const float FrameDuration = 1.0f / FrameRate;

	while (FrameTimeAccumulator >= FrameDuration)
	{
		FrameTimeAccumulator -= FrameDuration;
		++CurrentFrame;

		if (CurrentFrame >= TotalFrames)
		{
			if (bLooping)
			{
				CurrentFrame = 0;
			}
			else
			{
				CurrentFrame = TotalFrames - 1;
				bIsPlaying = false;
				break;
			}
		}
	}
}

FVector4 USpriteMaterial::GetSubUVParams() const
{
	return FVector4(
		static_cast<float>(SubUVRows),
		static_cast<float>(SubUVCols),
		static_cast<float>(CurrentFrame),
		static_cast<float>(TotalFrames)
	);
}

bool USpriteMaterial::RequiresTick() const
{
	return TotalFrames > 1;
}

void USpriteMaterial::ClampCurrentFrame()
{
	CurrentFrame = std::clamp(CurrentFrame, 0, std::max(0, TotalFrames - 1));
}
