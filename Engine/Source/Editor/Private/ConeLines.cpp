#include "pch.h"
#include "Editor/Public/ConeLines.h"
#include <cmath>

UConeLines::UConeLines()
	: Vertices(TArray<FVector>())
{
	// NumVertices = 1 (apex) + NumSegments (바닥면 원)
	NumVertices = 1 + NumSegments;
	Vertices.reserve(NumVertices);
	Vertices.resize(NumVertices);

	// 초기에는 비활성화 상태 (모든 정점을 원점에)
	Disable();
}

UConeLines::~UConeLines() = default;

void UConeLines::UpdateVertices(const FVector& Apex, const FVector& Direction, const FVector& UpVector,
                                float Angle, float Depth, float RadiusX, float RadiusY)
{
	if (Vertices.size() < NumVertices)
	{
		Vertices.resize(NumVertices);
	}

	bIsEnabled = true;

	// 첫 번째 정점: Cone의 꼭지점 (Apex) - 광원 위치
	Vertices[0] = Apex;

	// Angle에 따른 실제 투사 반지름 계산
	// Shader와 동일하게 계산: radius = depth * tan(Angle / 2)
	const float PI = 3.14159265358979323846f;
	const float HalfAngleRad = (Angle * PI / 180.0f) * 0.5f;

	// 정규화된 깊이 1.0에서의 반지름 (Shader와 동일)
	const float NormalizedRadius = tanf(HalfAngleRad);

	// 실제 월드 반지름 = 정규화 반지름 * Depth
	const float CalculatedRadius = Depth * NormalizedRadius;

	// DecalBox를 넘지 않도록 제한 (RadiusX, RadiusY는 최대 허용 반지름)
	float ActualRadiusX = (CalculatedRadius < RadiusX) ? CalculatedRadius : RadiusX;
	float ActualRadiusY = (CalculatedRadius < RadiusY) ? CalculatedRadius : RadiusY;

	// Direction 벡터로 로컬 좌표계 구성
	FVector Forward = Direction;
	Forward.Normalize();

	FVector Up = UpVector;
	Up.Normalize();

	// Right 벡터 계산 (Forward x Up)
	FVector Right = Forward.Cross(Up);
	Right.Normalize();

	// Up 벡터 재계산 (Right x Forward)
	Up = Right.Cross(Forward);
	Up.Normalize();

	// 바닥면의 중심 (Apex에서 Direction 방향으로 Depth만큼)
	FVector BottomCenter = Apex + Forward * Depth;

	// 바닥면의 원/타원 위의 점들 계산
	const float AngleStep = (2.0f * PI) / static_cast<float>(NumSegments);

	for (uint32 i = 0; i < NumSegments; ++i)
	{
		float CurrentAngle = static_cast<float>(i) * AngleStep;

		// 로컬 좌표계에서 타원 위의 점 계산
		float LocalX = ActualRadiusX * cosf(CurrentAngle);
		float LocalY = ActualRadiusY * sinf(CurrentAngle);

		// 로컬 좌표를 월드 좌표로 변환
		FVector WorldPos = BottomCenter + Right * LocalX + Up * LocalY;

		Vertices[1 + i] = WorldPos;
	}
}

void UConeLines::MergeVerticesAt(TArray<FVector>& DestVertices, size_t InsertStartIndex)
{
	// 인덱스 범위 보정
	InsertStartIndex = std::min(InsertStartIndex, DestVertices.size());

	// 미리 메모리 확보
	DestVertices.reserve(DestVertices.size() + std::distance(Vertices.begin(), Vertices.end()));

	// 덮어쓸 수 있는 개수 계산
	size_t OverwriteCount = std::min(
		Vertices.size(),
		DestVertices.size() - InsertStartIndex
	);

	// 기존 요소 덮어쓰기
	std::copy(
		Vertices.begin(),
		Vertices.begin() + OverwriteCount,
		DestVertices.begin() + InsertStartIndex
	);
}

void UConeLines::Disable()
{
	bIsEnabled = false;

	// 모든 정점을 원점으로 (렌더링 시 보이지 않도록)
	for (uint32 i = 0; i < NumVertices; ++i)
	{
		Vertices[i] = FVector(0.0f, 0.0f, 0.0f);
	}
}
