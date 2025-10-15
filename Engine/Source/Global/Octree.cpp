#include "pch.h"
#include "Global/Octree.h"
#include "Component/Public/PrimitiveComponent.h"

#include "Level/Public/Level.h"

namespace
{
	FAABB GetPrimitiveBoundingBox(UPrimitiveComponent* InPrimitive)
	{
		FVector Min, Max;
		InPrimitive->GetWorldAABB(Min, Max);

		return FAABB(Min, Max);
	}
}

FOctree::FOctree()
	: BoundingBox(), Depth(0)
{
	Children.resize(8);
}

FOctree::FOctree(const FAABB& InBoundingBox, int InDepth)
	: BoundingBox(InBoundingBox), Depth(InDepth)
{
	Children.resize(8);
}

FOctree::FOctree(const FVector& InPosition, float InSize, int InDepth)
	: Depth(InDepth)
{
	const float HalfSize = InSize * 0.5f;
	BoundingBox.Min = InPosition - FVector(HalfSize, HalfSize, HalfSize);
	BoundingBox.Max = InPosition + FVector(HalfSize, HalfSize, HalfSize);
	Children.resize(8);
}

FOctree::~FOctree()
{
	Primitives.clear();
	for (int Index = 0; Index < 8; ++Index) { SafeDelete(Children[Index]); }
}

bool FOctree::Insert(UPrimitiveComponent* InPrimitive)
{
	// nullptr 체크
	if (!InPrimitive) { return false; }

	// 0. 영역 내에 객체가 없으면 종료
	if (BoundingBox.IsIntersected(GetPrimitiveBoundingBox(InPrimitive)) == false) { return false; }

	if (IsLeaf())
	{
		// 리프 노드이며, 여유 공간이 있거나 최대 깊이에 도달했다면
		if (Primitives.size() < MAX_PRIMITIVES || Depth == MAX_DEPTH)
		{
			Primitives.push_back(InPrimitive); // 해당 객체를 추가한다
			return true;
		}
		else // 여유 공간이 없고, 최대 깊이에 도달하지 않았다면
		{
			// 분할 및 재귀적 추가를 한다
			Subdivide(InPrimitive);
			return true;
		}
	}
	else
	{
		for (int Index = 0; Index < 8; ++Index)
		{
			// 자식 노드를 보유하고 있고, 영역 내에 해당 객체가 존재한다면
			if (Children[Index] && Children[Index]->BoundingBox.IsContains(GetPrimitiveBoundingBox(InPrimitive)))
			{
				return Children[Index]->Insert(InPrimitive); // 자식 노드에게 넘겨준다
			}
		}

		Primitives.push_back(InPrimitive);
		return true;
	}

	return false;
}

bool FOctree::Remove(UPrimitiveComponent* InPrimitive)
{
	if (!InPrimitive) return false;

	// 빠른 경로: 경계 교차 기반 제거
	if (BoundingBox.IsIntersected(GetPrimitiveBoundingBox(InPrimitive)))
	{
		if (IsLeaf())
		{
			if (auto It = std::find(Primitives.begin(), Primitives.end(), InPrimitive);
				It != Primitives.end())
			{
				*It = std::move(Primitives.back());
				Primitives.pop_back();
				return true;
			}
			return false;
		}
		else
		{
			for (int i = 0; i < 8; ++i)
			{
				if (Children[i] && Children[i]->Remove(InPrimitive))
				{
					TryMerge();
					return true;
				}
			}
		}
	}

	// 폴백: 경계 무시, 포인터 일치로 전체 트리 탐색
	return RemoveByIdentityInternal(this, InPrimitive);
}

void FOctree::Clear()
{
	Primitives.clear();
	for (int Index = 0; Index < 8; ++Index) { SafeDelete(Children[Index]); }
}

void FOctree::GetAllPrimitives(TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	// 1. 현재 노드가 가진 프리미티브를 결과 배열에 추가합니다.
	OutPrimitives.insert(OutPrimitives.end(), Primitives.begin(), Primitives.end());

	// 2. 리프 노드가 아니라면, 모든 자식 노드에 대해 재귀적으로 함수를 호출합니다.
	if (!IsLeaf())
	{
		for (int Index = 0; Index < 8; ++Index)
		{
			if (Children[Index]) { Children[Index]->GetAllPrimitives(OutPrimitives); }
		}
	}
}

TArray<UPrimitiveComponent*> FOctree::FindNearestPrimitives(const FVector& FindPos, uint32 MaxPrimitiveCount)
{
	TArray<UPrimitiveComponent*> Candidates = GWorld->GetLevel()->GetDynamicPrimitives();
	Candidates.reserve(MaxPrimitiveCount);
	FNodeQueue NodeQueue;

	float RootDistance = this->GetBoundingBox().GetCenterDistanceSquared(FindPos);
	NodeQueue.push({ RootDistance, this });

	while (!NodeQueue.empty() && Candidates.size() < MaxPrimitiveCount)
	{
		FOctree* CurrentNode = NodeQueue.top().second;
		NodeQueue.pop();

		if (CurrentNode->IsLeafNode())
		{
			for (UPrimitiveComponent* Primitive : CurrentNode->GetPrimitives())
			{
				Candidates.push_back(Primitive);
			}
		}
		else
		{
			for (int i = 0; i < 8; ++i)
			{
				FOctree* Child = CurrentNode->Children[i];
				if (Child)
				{
					float ChildDistance = Child->GetBoundingBox().GetCenterDistanceSquared(FindPos);
					NodeQueue.push({ ChildDistance, Child });
				}
			}
		}
	}

	return Candidates;
}

void FOctree::Subdivide(UPrimitiveComponent* InPrimitive)
{
	const FVector& Min = BoundingBox.Min;
	const FVector& Max = BoundingBox.Max;
	const FVector Center = (Min + Max) * 0.5f;

	Children[0] = new FOctree(FAABB(FVector(Min.X, Center.Y, Min.Z), FVector(Center.X, Max.Y, Center.Z)), Depth + 1); // Top-Back-Left
	Children[1] = new FOctree(FAABB(FVector(Center.X, Center.Y, Min.Z), FVector(Max.X, Max.Y, Center.Z)), Depth + 1); // Top-Back-Right
	Children[2] = new FOctree(FAABB(FVector(Min.X, Center.Y, Center.Z), FVector(Center.X, Max.Y, Max.Z)), Depth + 1); // Top-Front-Left
	Children[3] = new FOctree(FAABB(FVector(Center.X, Center.Y, Center.Z), FVector(Max.X, Max.Y, Max.Z)), Depth + 1); // Top-Front-Right
	Children[4] = new FOctree(FAABB(FVector(Min.X, Min.Y, Min.Z), FVector(Center.X, Center.Y, Center.Z)), Depth + 1); // Bottom-Back-Left
	Children[5] = new FOctree(FAABB(FVector(Center.X, Min.Y, Min.Z), FVector(Max.X, Center.Y, Center.Z)), Depth + 1); // Bottom-Back-Right
	Children[6] = new FOctree(FAABB(FVector(Min.X, Min.Y, Center.Z), FVector(Center.X, Center.Y, Max.Z)), Depth + 1); // Bottom-Front-Left
	Children[7] = new FOctree(FAABB(FVector(Center.X, Min.Y, Center.Z), FVector(Max.X, Center.Y, Max.Z)), Depth + 1); // Bottom-Front-Right

	TArray<UPrimitiveComponent*> primitivesToMove = Primitives;
	primitivesToMove.push_back(InPrimitive);
	Primitives.clear();

	for (UPrimitiveComponent* prim : primitivesToMove)
	{
		Insert(prim);
	}
}

void FOctree::TryMerge()
{
	// Case 1. 자식 노드가 존재하지 않으므로 종료
	if (IsLeaf()) { return; }

	// 모든 자식 노드가 리프 노드인지 확인
	for (int Index = 0; Index < 8; ++Index)
	{
		if (!Children[Index]->IsLeaf())
		{
			return; // 하나라도 리프가 아니면 합치지 않음
		}
	}

	// 모든 자식 노드에 있는 프리미티브의 총 개수를 계산
	uint32 TotalPrimitives = static_cast<uint32>(Primitives.size());
	for (int Index = 0; Index < 8; ++Index)
	{
		TotalPrimitives += static_cast<uint32>(Children[Index]->Primitives.size());
	}

	// 프리미티브 총 개수가 최대치보다 작으면 합치기 수행
	if (TotalPrimitives <= MAX_PRIMITIVES)
	{
		for (int Index = 0; Index < 8; ++Index)
		{
			Primitives.insert(Primitives.end(), Children[Index]->Primitives.begin(), Children[Index]->Primitives.end());
		}

		// 모든 자식 노드를 메모리에서 해제
		for (int Index = 0; Index < 8; ++Index) { SafeDelete(Children[Index]); }
	}
}

void FOctree::DeepCopy(FOctree* OutOctree) const
{
	if (!OutOctree) { return; }

	// 1) 필드 복사
	OutOctree->BoundingBox = BoundingBox;
	OutOctree->Depth = Depth;

	// 2) 기존 대상의 프리미티브/자식 정리 후 초기화
	//    - 프리미티브는 대입으로 교체
	OutOctree->Primitives = Primitives; // shallow copy of pointers

	//    - 기존 자식 노드 메모리 해제
	for (FOctree* Child : OutOctree->Children)
	{
		SafeDelete(Child);
	}
	OutOctree->Children.clear();
	OutOctree->Children.resize(8, nullptr);

	// 3) 자식 재귀 복사
	if (!IsLeaf())
	{
		for (int Index = 0; Index < 8; ++Index)
		{
			if (Children[Index] != nullptr)
			{
				// 자식 노드 생성 후 재귀 복사
				OutOctree->Children[Index] = new FOctree(Children[Index]->BoundingBox, Children[Index]->Depth);
				Children[Index]->DeepCopy(OutOctree->Children[Index]);
			}
		}
	}
}

bool FOctree::RemoveByIdentityInternal(FOctree* Node, UPrimitiveComponent* Prim)
{
	if (!Node || !Prim) return false;

	// 현재 노드에서 직접 찾아 제거
	if (auto it = std::find(Node->Primitives.begin(), Node->Primitives.end(), Prim);
		it != Node->Primitives.end())
	{
		*it = std::move(Node->Primitives.back());
		Node->Primitives.pop_back();
		return true;
	}

	// 리프가 아니면 자식 재귀 탐색
	if (!Node->IsLeaf())
	{
		for (FOctree* Child : Node->Children)
		{
			if (Child && RemoveByIdentityInternal(Child, Prim))
			{
				Node->TryMerge(); // 필요 시 병합
				return true;
			}
		}
	}
	return false;
}