#include "pch.h"
#include "Physics/Public/AABB.h"
#include "Physics/Public/OBB.h"

float FAABB::GetCenterDistanceSquared(const FVector& Point) const
{
    FVector Center = GetCenter();
    FVector Diff = Center - Point;

    return (Diff.X * Diff.X) + (Diff.Y * Diff.Y) + (Diff.Z * Diff.Z);
}

bool FAABB::IsContains(const FAABB& Other) const
{
    return (Other.Min.X >= Min.X && Other.Max.X <= Max.X) &&
        (Other.Min.Y >= Min.Y && Other.Max.Y <= Max.Y) &&
        (Other.Min.Z >= Min.Z && Other.Max.Z <= Max.Z);
}

bool FAABB::IsIntersected(const FAABB& Other) const
{
	return (Min.X <= Other.Max.X && Max.X >= Other.Min.X) &&
		(Min.Y <= Other.Max.Y && Max.Y >= Other.Min.Y) &&
		(Min.Z <= Other.Max.Z && Max.Z >= Other.Min.Z);
}

float FAABB::GetSurfaceArea() const
{
    FVector Extent = Max - Min;
    return 2.f * (Extent.X * Extent.Y + Extent.Y * Extent.Z + Extent.Z * Extent.X);
}

bool FAABB::RaycastHit() const
{
	return false;
}

bool CheckIntersectionRayBox(const FRay& Ray, const FAABB& Box)
{
	// AABB intersectin test by "Slab Method"
    float TMin = -FLT_MAX;
    float TMax = FLT_MAX;

    for (int Axis = 0; Axis < 3; ++Axis)
    {
        float Origin = (Axis == 0) ? Ray.Origin.X : (Axis == 1) ? Ray.Origin.Y : Ray.Origin.Z;
        float Dir = (Axis == 0) ? Ray.Direction.X : (Axis == 1) ? Ray.Direction.Y : Ray.Direction.Z;
        float BMin = (Axis == 0) ? Box.Min.X : (Axis == 1) ? Box.Min.Y : Box.Min.Z;
        float BMax = (Axis == 0) ? Box.Max.X : (Axis == 1) ? Box.Max.Y : Box.Max.Z;

		// if the ray is parallel to the slab
        if (fabs(Dir) < MATH_EPSILON)
        {
            if (Origin < BMin || Origin > BMax)
            {
				return false; // ray is parallel and outside the slab
            }
            continue;
        }

        float T1 = (BMin - Origin) / Dir;
        float T2 = (BMax - Origin) / Dir;
        if (T1 > T2) std::swap(T1, T2);

        TMin = std::max(TMin, T1);
        TMax = std::min(TMax, T2);
        if (TMax < TMin) return false;
    }

	if (TMax < 0.0f) return false; // box is behind the ray

    return true;
}

FAABB Union(const FAABB& Box1, const FAABB& Box2)
{
    FVector NewMin(
        std::min(Box1.Min.X, Box2.Min.X),
        std::min(Box1.Min.Y, Box2.Min.Y),
        std::min(Box1.Min.Z, Box2.Min.Z)
    );
    FVector NewMax(
        std::max(Box1.Max.X, Box2.Max.X),
        std::max(Box1.Max.Y, Box2.Max.Y),
        std::max(Box1.Max.Z, Box2.Max.Z)
    );
    return FAABB(NewMin, NewMax);
}


float FAABB::GetDistanceSquaredToPoint(const FVector& Point) const
{
    float SquaredDistance = 0.0f;

    if (Point.X < Min.X) {
        SquaredDistance += (Min.X - Point.X) * (Min.X - Point.X);
    }
    else if (Point.X > Max.X) {
        SquaredDistance += (Point.X - Max.X) * (Point.X - Max.X);
    }

    if (Point.Y < Min.Y) {
        SquaredDistance += (Min.Y - Point.Y) * (Min.Y - Point.Y);
    }
    else if (Point.Y > Max.Y) {
        SquaredDistance += (Point.Y - Max.Y) * (Point.Y - Max.Y);
    }

    if (Point.Z < Min.Z) {
        SquaredDistance += (Min.Z - Point.Z) * (Min.Z - Point.Z);
    }
    else if (Point.Z > Max.Z) {
        SquaredDistance += (Point.Z - Max.Z) * (Point.Z - Max.Z);
    }

    return SquaredDistance;
}

bool CheckIntersectionOBBAABB(const FOBB& OBB, const FAABB& AABB)
{
	// Separating Axis Theorem (SAT)을 사용한 OBB-AABB 교차 검사
	// 15개의 분리축을 테스트:
	// - 3개의 AABB 축 (World X, Y, Z)
	// - 3개의 OBB 축 (OBB의 로컬 X, Y, Z)
	// - 9개의 교차곱 축 (각 AABB 축 × 각 OBB 축)

	// AABB의 중심과 반경
	FVector AABBCenter = AABB.GetCenter();
	FVector AABBExtents = (AABB.Max - AABB.Min) * 0.5f;

	// OBB의 3개 축 추출 (회전 행렬의 각 열) - 스케일 포함
	FVector OBBAxis[3];
	OBBAxis[0] = FVector(OBB.ScaleRotation.Data[0][0], OBB.ScaleRotation.Data[1][0], OBB.ScaleRotation.Data[2][0]);
	OBBAxis[1] = FVector(OBB.ScaleRotation.Data[0][1], OBB.ScaleRotation.Data[1][1], OBB.ScaleRotation.Data[2][1]);
	OBBAxis[2] = FVector(OBB.ScaleRotation.Data[0][2], OBB.ScaleRotation.Data[1][2], OBB.ScaleRotation.Data[2][2]);

	// OBB Extents에 스케일 적용 (축의 길이 = 스케일)
	FVector ScaledExtents;
	ScaledExtents.X = OBB.Extents.X * OBBAxis[0].Length();
	ScaledExtents.Y = OBB.Extents.Y * OBBAxis[1].Length();
	ScaledExtents.Z = OBB.Extents.Z * OBBAxis[2].Length();

	// 축을 정규화 (방향만 남김)
	OBBAxis[0].Normalize();
	OBBAxis[1].Normalize();
	OBBAxis[2].Normalize();

	// 중심 간 거리 벡터
	FVector T = OBB.Center - AABBCenter;

	// 절대값 행렬: abs(OBBAxis · AABBAxis)
	float AbsR[3][3];
	for (int32 i = 0; i < 3; ++i)
	{
		for (int32 j = 0; j < 3; ++j)
		{
			float Dot = 0.0f;
			if (j == 0) Dot = OBBAxis[i].X;
			else if (j == 1) Dot = OBBAxis[i].Y;
			else Dot = OBBAxis[i].Z;
			AbsR[i][j] = fabs(Dot) + MATH_EPSILON;
		}
	}

	// 1. AABB의 3개 축 테스트 (World X, Y, Z)
	{
		// X축
		float Ra = AABBExtents.X;
		float Rb = ScaledExtents.X * AbsR[0][0] + ScaledExtents.Y * AbsR[1][0] + ScaledExtents.Z * AbsR[2][0];
		if (fabs(T.X) > Ra + Rb) return false;

		// Y축
		Ra = AABBExtents.Y;
		Rb = ScaledExtents.X * AbsR[0][1] + ScaledExtents.Y * AbsR[1][1] + ScaledExtents.Z * AbsR[2][1];
		if (fabs(T.Y) > Ra + Rb) return false;

		// Z축
		Ra = AABBExtents.Z;
		Rb = ScaledExtents.X * AbsR[0][2] + ScaledExtents.Y * AbsR[1][2] + ScaledExtents.Z * AbsR[2][2];
		if (fabs(T.Z) > Ra + Rb) return false;
	}

	// 2. OBB의 3개 축 테스트
	for (int32 i = 0; i < 3; ++i)
	{
		float Projection = T.X * OBBAxis[i].X + T.Y * OBBAxis[i].Y + T.Z * OBBAxis[i].Z;
		float Ra = AABBExtents.X * AbsR[i][0] + AABBExtents.Y * AbsR[i][1] + AABBExtents.Z * AbsR[i][2];
		float Rb = (i == 0) ? ScaledExtents.X : (i == 1) ? ScaledExtents.Y : ScaledExtents.Z;
		if (fabs(Projection) > Ra + Rb) return false;
	}

	// 3. 교차곱 축 9개 테스트
	// 축: AABB X × OBB X
	{
		float Projection = T.Z * OBBAxis[0].Y - T.Y * OBBAxis[0].Z;
		float Ra = AABBExtents.Y * AbsR[0][2] + AABBExtents.Z * AbsR[0][1];
		float Rb = ScaledExtents.Y * AbsR[2][0] + ScaledExtents.Z * AbsR[1][0];
		if (fabs(Projection) > Ra + Rb) return false;
	}

	// 축: AABB X × OBB Y
	{
		float Projection = T.Z * OBBAxis[1].Y - T.Y * OBBAxis[1].Z;
		float Ra = AABBExtents.Y * AbsR[1][2] + AABBExtents.Z * AbsR[1][1];
		float Rb = ScaledExtents.X * AbsR[2][0] + ScaledExtents.Z * AbsR[0][0];
		if (fabs(Projection) > Ra + Rb) return false;
	}

	// 축: AABB X × OBB Z
	{
		float Projection = T.Z * OBBAxis[2].Y - T.Y * OBBAxis[2].Z;
		float Ra = AABBExtents.Y * AbsR[2][2] + AABBExtents.Z * AbsR[2][1];
		float Rb = ScaledExtents.X * AbsR[1][0] + ScaledExtents.Y * AbsR[0][0];
		if (fabs(Projection) > Ra + Rb) return false;
	}

	// 축: AABB Y × OBB X
	{
		float Projection = T.X * OBBAxis[0].Z - T.Z * OBBAxis[0].X;
		float Ra = AABBExtents.X * AbsR[0][2] + AABBExtents.Z * AbsR[0][0];
		float Rb = ScaledExtents.Y * AbsR[2][1] + ScaledExtents.Z * AbsR[1][1];
		if (fabs(Projection) > Ra + Rb) return false;
	}

	// 축: AABB Y × OBB Y
	{
		float Projection = T.X * OBBAxis[1].Z - T.Z * OBBAxis[1].X;
		float Ra = AABBExtents.X * AbsR[1][2] + AABBExtents.Z * AbsR[1][0];
		float Rb = ScaledExtents.X * AbsR[2][1] + ScaledExtents.Z * AbsR[0][1];
		if (fabs(Projection) > Ra + Rb) return false;
	}

	// 축: AABB Y × OBB Z
	{
		float Projection = T.X * OBBAxis[2].Z - T.Z * OBBAxis[2].X;
		float Ra = AABBExtents.X * AbsR[2][2] + AABBExtents.Z * AbsR[2][0];
		float Rb = ScaledExtents.X * AbsR[1][1] + ScaledExtents.Y * AbsR[0][1];
		if (fabs(Projection) > Ra + Rb) return false;
	}

	// 축: AABB Z × OBB X
	{
		float Projection = T.Y * OBBAxis[0].X - T.X * OBBAxis[0].Y;
		float Ra = AABBExtents.X * AbsR[0][1] + AABBExtents.Y * AbsR[0][0];
		float Rb = ScaledExtents.Y * AbsR[2][2] + ScaledExtents.Z * AbsR[1][2];
		if (fabs(Projection) > Ra + Rb) return false;
	}

	// 축: AABB Z × OBB Y
	{
		float Projection = T.Y * OBBAxis[1].X - T.X * OBBAxis[1].Y;
		float Ra = AABBExtents.X * AbsR[1][1] + AABBExtents.Y * AbsR[1][0];
		float Rb = ScaledExtents.X * AbsR[2][2] + ScaledExtents.Z * AbsR[0][2];
		if (fabs(Projection) > Ra + Rb) return false;
	}

	// 축: AABB Z × OBB Z
	{
		float Projection = T.Y * OBBAxis[2].X - T.X * OBBAxis[2].Y;
		float Ra = AABBExtents.X * AbsR[2][1] + AABBExtents.Y * AbsR[2][0];
		float Rb = ScaledExtents.X * AbsR[1][2] + ScaledExtents.Y * AbsR[0][2];
		if (fabs(Projection) > Ra + Rb) return false;
	}

	// 모든 분리축 테스트를 통과했으므로 교차함
	return true;
}