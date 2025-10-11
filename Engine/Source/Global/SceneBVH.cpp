#include "pch.h"
#include "Global/SceneBVH.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Physics/Public/OBB.h"

const FSceneNode& FSceneBVH::GetNode(uint32 Index) const
{
	assert(Index < Nodes.size());
	return Nodes[Index];
}

FSceneNode& FSceneBVH::GetNode(uint32 Index)
{
	assert(Index < Nodes.size());
	return Nodes[Index];
}

void FSceneBVH::Clear()
{
	Nodes.clear();
	RootIndex = -1;
	Cost = 0.0f;
	ComponentToNodeMap.clear();
}

int32 FSceneBVH::InsertLeaf(UPrimitiveComponent* InComponent)
{
	if (!InComponent)
	{
		std::cerr << "FSceneBVH::InsertLeaf: Component is null. Cannot insert leaf node." << std::endl;
		return -1;
	}

	// 1. Component의 AABB 가져오기
	FVector WorldMin, WorldMax;
	InComponent->GetWorldAABB(WorldMin, WorldMax);
	FAABB LeafAABB(WorldMin, WorldMax);

	// 2. 새 Leaf node 생성
	FSceneNode NewNode;
	NewNode.ObjectIndex = static_cast<int32>(Nodes.size());
	NewNode.ParentIndex = -1; // Parent는 InsertInternalNode에서 설정
	NewNode.Child1 = -1;
	NewNode.Child2 = -1;
	NewNode.bIsLeaf = true;
	NewNode.Box = LeafAABB;
	NewNode.Component = InComponent;

	// 빈 트리인 경우 새 노드를 루트로 설정하고 종료
	if (NewNode.ObjectIndex == 0)
	{
		Nodes.push_back(NewNode);
		RootIndex = 0;
		Cost = GetCost(RootIndex);
		ComponentToNodeMap[InComponent] = 0;
		return 0;
	}

	// 3. 새 Leaf node를 삽입할 최적의 sibling node 찾기
	int32 SiblingIndex = FindBestSibling(NewNode.Box);

	// 4. 새 leaf node와 sibling의 새로운 부모 node 생성
	Nodes.push_back(NewNode);
	InsertInternalNode(NewNode.ObjectIndex, SiblingIndex);

	// 5. 새 node가 추가되었으므로 부모 거슬러올라가며 AABB 리피팅
	RefitAncestors(Nodes[SiblingIndex].ParentIndex);

	// 6. 해시맵에 등록
	ComponentToNodeMap[InComponent] = NewNode.ObjectIndex;

	return NewNode.ObjectIndex;
}

float FSceneBVH::GetCost(int32 SubTreeRootIndex, bool bInternalOnly) const
{
	// 인덱스 벗어난 경우 0 반환
	if (SubTreeRootIndex >= Nodes.size() || SubTreeRootIndex < 0)
	{
		return 0.0f;
	}

	const FSceneNode& SubTreeRoot = Nodes[SubTreeRootIndex];
	if (SubTreeRoot.bIsLeaf)
	{
		// InternalOnly면 leaf node의 cost는 0으로 계산
		if (bInternalOnly)
		{
			return 0.0f;
		}

		return SubTreeRoot.Box.GetSurfaceArea();
	}

	return SubTreeRoot.Box.GetSurfaceArea() + GetCost(SubTreeRoot.Child1, bInternalOnly) + GetCost(SubTreeRoot.Child2, bInternalOnly);
}

int32 FSceneBVH::FindBestSibling(const FAABB& NewLeafAABB)
{
	int32 BestSiblingIndex = -1;
	float MinCostIncrease = FLT_MAX;

	// Branch and Bound 최적화를 위한 스택 기반 순회
	TArray<int32> CandidateStack;
	CandidateStack.push_back(RootIndex);

	while (!CandidateStack.empty())
	{
		int32 CurrentIndex = CandidateStack.back();
		CandidateStack.pop_back();

		if (CurrentIndex < 0 || CurrentIndex >= static_cast<int32>(Nodes.size()))
		{
			continue;
		}

		const FSceneNode& CurrentNode = Nodes[CurrentIndex];
		FAABB CombinedAABB = Union(NewLeafAABB, CurrentNode.Box);

		// 하한 비용 계산
		float DirectIncrease = CombinedAABB.GetSurfaceArea() - CurrentNode.Box.GetSurfaceArea();
		float InheritedIncrease = 0.0f;
		int32 AncestorIndex = CurrentNode.ParentIndex;
		int32 CurrentChildIndex = CurrentIndex;
		FAABB CurrentAABB = CombinedAABB;
		while (AncestorIndex != -1)
		{
			const FSceneNode& AncestorNode = Nodes[AncestorIndex];
			int32 SiblingOfCurrentChildIndex = (AncestorNode.Child1 == CurrentChildIndex)
				? AncestorNode.Child2
				: AncestorNode.Child1;
			const FSceneNode& SiblingOfCurrentChild = Nodes[SiblingOfCurrentChildIndex];

			FAABB AncestorNewAABB = Union(SiblingOfCurrentChild.Box, CurrentAABB);
			InheritedIncrease += AncestorNewAABB.GetSurfaceArea() - AncestorNode.Box.GetSurfaceArea();
			AncestorIndex = AncestorNode.ParentIndex;
			CurrentChildIndex = AncestorNode.ObjectIndex;
			CurrentAABB = AncestorNewAABB;
		}
		float LowerBoundIncrease = DirectIncrease + InheritedIncrease;

		// 하한 비용이 현재 최적 증가 비용 이상이면 탐색 중단
		if (LowerBoundIncrease >= MinCostIncrease)
		{
			continue;
		}

		if (CurrentNode.bIsLeaf)
		{
			// 리프 노드인 경우: 전체 비용 증가 계산
			float TotalCostIncrease = CalculateCostIncrease(CurrentIndex, NewLeafAABB);

			if (TotalCostIncrease < MinCostIncrease)
			{
				MinCostIncrease = TotalCostIncrease;
				BestSiblingIndex = CurrentIndex;
			}
		}
		else
		{
			// 내부 노드인 경우: 자식들을 스택에 추가
			if (LowerBoundIncrease < MinCostIncrease)
			{
				if (CurrentNode.Child1 >= 0 && CurrentNode.Child1 < static_cast<int32>(Nodes.size()))
				{
					CandidateStack.push_back(CurrentNode.Child1);
				}
				if (CurrentNode.Child2 >= 0 && CurrentNode.Child2 < static_cast<int32>(Nodes.size()))
				{
					CandidateStack.push_back(CurrentNode.Child2);
				}
			}
		}
	}

	return BestSiblingIndex;
}

float FSceneBVH::CalculateCostIncrease(int32 CandidateIndex, const FAABB& NewLeafAABB) const
{
	if (CandidateIndex < 0 || CandidateIndex >= static_cast<int32>(Nodes.size()))
	{
		return FLT_MAX;
	}

	const FSceneNode& CandidateNode = Nodes[CandidateIndex];
	const FAABB& SiblingAABB = CandidateNode.Box;

	// 새 리프와 sibling을 묶는 새 internal node의 AABB 계산
	FAABB CombinedAABB = Union(NewLeafAABB, SiblingAABB);

	// Cost 증가 계산
	float CostIncrease = CombinedAABB.GetSurfaceArea();

	// Sibling이 가지고 있던 부모 노드가 있다면 부모부터 루트까지 올라가며 추가 cost 계산
	int32 AncestorIndex = CandidateNode.ParentIndex;
	int32 CurrentChildIndex = CandidateIndex;
	FAABB CurrentAABB = CombinedAABB;
	while (AncestorIndex != -1 && AncestorIndex < static_cast<int32>(Nodes.size()))
	{
		const FSceneNode& AncestorNode = Nodes[AncestorIndex];

		// 형제 노드의 AABB와 합쳐서 새로운 조상 AABB 계산
		int32 SiblingOfCurrentChildIndex = (AncestorNode.Child1 == CurrentChildIndex)
			? AncestorNode.Child2
			: AncestorNode.Child1;

		if (SiblingOfCurrentChildIndex >= 0 && SiblingOfCurrentChildIndex < static_cast<int32>(Nodes.size()))
		{
			const FAABB& SiblingOfCurrentChild = Nodes[SiblingOfCurrentChildIndex].Box;
			FAABB NewAncestorAABB = Union(CurrentAABB, SiblingOfCurrentChild);
			CostIncrease += NewAncestorAABB.GetSurfaceArea() - AncestorNode.Box.GetSurfaceArea();
			CurrentAABB = NewAncestorAABB;
		}

		CurrentChildIndex = AncestorIndex;
		AncestorIndex = AncestorNode.ParentIndex;
	}

	return CostIncrease;
}

void FSceneBVH::InsertInternalNode(int32 NewLeafIndex, int32 SiblingIndex)
{
	// 새 부모(Internal) 노드 구성
	FSceneNode NewParent;
	int32 OldParentIndex = Nodes[SiblingIndex].ParentIndex;
	NewParent.ParentIndex = OldParentIndex;
	NewParent.Child1 = SiblingIndex;
	NewParent.Child2 = NewLeafIndex;
	NewParent.bIsLeaf = false;
	NewParent.Component = nullptr; // Internal 노드는 Component 없음

	// 부모 AABB는 두 자식의 합집합
	FAABB NewLeafAABB = Nodes[NewLeafIndex].Box;
	FAABB SiblingAABB = Nodes[SiblingIndex].Box;
	NewParent.Box = Union(NewLeafAABB, SiblingAABB);

	// 새 부모 노드를 push하고 인덱스 확정
	int32 NewParentIndex = static_cast<int32>(Nodes.size());
	NewParent.ObjectIndex = NewParentIndex;
	Nodes.push_back(NewParent);

	// 자식들의 부모 갱신
	Nodes[SiblingIndex].ParentIndex = NewParentIndex;
	Nodes[NewLeafIndex].ParentIndex = NewParentIndex;

	// 이전 부모가 있었다면 그 부모의 자식 포인터를 새 부모로 교체
	if (OldParentIndex != -1)
	{
		FSceneNode& OldParent = Nodes[OldParentIndex];
		if (OldParent.Child1 == SiblingIndex)
		{
			OldParent.Child1 = NewParentIndex;
		}
		else if (OldParent.Child2 == SiblingIndex)
		{
			OldParent.Child2 = NewParentIndex;
		}
		else
		{
			throw std::runtime_error("OldParent does not reference SiblingIndex as a child.");
		}
	}
	// sibling이 루트였던 경우, 새 부모가 루트가 됨
	else
	{
		RootIndex = NewParentIndex;
	}
}

void FSceneBVH::RefitAncestors(int32 RefitStartIndex)
{
	// 시작점의 부모부터 루트까지 올라가며 InternalBox를 리핏
	if (RefitStartIndex < 0 || RefitStartIndex >= static_cast<int32>(Nodes.size()))
	{
		return;
	}

	int32 CurrentIndex = Nodes[RefitStartIndex].ParentIndex; // 부모부터 시작
	while (CurrentIndex != -1)
	{
		FSceneNode& Current = Nodes[CurrentIndex];
		// 두 자식의 AABB 합집합으로 현재 InternalBox 갱신
		FAABB Child1AABB = Nodes[Current.Child1].Box;
		FAABB Child2AABB = Nodes[Current.Child2].Box;
		Current.Box = Union(Child1AABB, Child2AABB);

		CurrentIndex = Current.ParentIndex;
	}

	// 전체 비용 갱신
	Cost = GetCost(RootIndex);
}

bool FSceneBVH::CheckValidity() const
{
	// 1. 루트의 인덱스가 유효한지 확인
	if ((RootIndex < 0 && !Nodes.empty()) || RootIndex >= static_cast<int32>(Nodes.size()))
	{
		return false;
	}

	// 2. 비어있는 트리의 경우 값이 정상적인지 확인
	if (Nodes.empty())
	{
		if (RootIndex != -1 || Cost != 0.0f)
		{
			return false;
		}
	}

	// 3. 각 노드의 부모-자식 관계가 일관적인지 확인
	for (int32 i = 0; i < static_cast<int32>(Nodes.size()); ++i)
	{
		const FSceneNode& Node = Nodes[i];
		if (Node.bIsLeaf) // Leaf node 인 경우 자식이 없어야 함
		{
			if (Node.Child1 != -1 || Node.Child2 != -1)
			{
				return false;
			}
			if (Node.Component == nullptr)
			{
				return false; // 리프 노드는 반드시 유효한 Component를 가져야 함
			}
		}
		else // Internal Node 인 경우 자식이 있어야 함
		{
			if (Node.Child1 == -1 || Node.Child2 == -1)
			{
				return false;
			}
			if (Node.Component != nullptr)
			{
				return false; // 내부 노드는 Component를 가져선 안됨
			}
			// 자식 노드들이 올바른 부모 인덱스를 가리키는지 확인
			if (Node.Child1 < 0 || Node.Child1 >= static_cast<int32>(Nodes.size()) ||
				Node.Child2 < 0 || Node.Child2 >= static_cast<int32>(Nodes.size()))
			{
				return false;
			}
			const FSceneNode& Child1 = Nodes[Node.Child1];
			const FSceneNode& Child2 = Nodes[Node.Child2];
			if (Child1.ParentIndex != i || Child2.ParentIndex != i)
			{
				return false;
			}
		}

		// 부모 인덱스가 유효한지 확인 (루트는 제외)
		if (Node.ParentIndex != -1)
		{
			if (Node.ParentIndex < 0 || Node.ParentIndex >= static_cast<int32>(Nodes.size()))
			{
				return false;
			}
		}
		else if (i != RootIndex) // 루트가 아닌데 부모가 없는 경우
		{
			return false;
		}
	}
	return true;
}

void FSceneBVH::Build(const TArray<UPrimitiveComponent*>& InComponents)
{
	Clear();

	// 모든 Component에 대해 Leaf 노드 삽입
	for (UPrimitiveComponent* Component : InComponents)
	{
		if (Component)
		{
			InsertLeaf(Component);
		}
	}

	// 전체 비용 계산
	Cost = GetCost(RootIndex);

	// 유효성 검사
	if (!CheckValidity())
	{
		std::cerr << "FSceneBVH::Build: BVH structure is invalid after build." << std::endl;
	}
}

bool FSceneBVH::QueryOverlappingOBBs(const TArray<FOBB>& OBBList, TArray<int32>& OutOBBIndices) const
{
	OutOBBIndices.clear();

	// 빈 트리이거나 루트가 유효하지 않은 경우
	if (RootIndex < 0 || RootIndex >= static_cast<int32>(Nodes.size()))
	{
		return false;
	}

	// 각 OBB에 대해 Scene BVH와 교차 검사
	for (int32 OBBIndex = 0; OBBIndex < OBBList.size(); ++OBBIndex)
	{
		const FOBB& OBB = OBBList[OBBIndex];

		// 스택을 사용한 반복적 순회
		TArray<int32> NodeStack;
		NodeStack.push_back(RootIndex);
		bool bFoundIntersection = false;

		while (!NodeStack.empty())
		{
			int32 CurrentNodeIndex = NodeStack.back();
			NodeStack.pop_back();

			const FSceneNode& CurrentNode = Nodes[CurrentNodeIndex];

			// OBB와 현재 노드의 AABB 교차 검사
			if (!CheckIntersectionOBBAABB(OBB, CurrentNode.Box))
			{
				continue; // AABB와 교차하지 않으면 이 노드의 자식들도 건너뜀
			}

			// 교차하는 노드를 발견했으므로 이 OBB는 씬과 겹침
			bFoundIntersection = true;
			break; // 하나라도 찾으면 더 이상 탐색 불필요
		}

		if (bFoundIntersection)
		{
			OutOBBIndices.push_back(OBBIndex);
		}
	}

	return !OutOBBIndices.empty();
}

bool FSceneBVH::QueryOverlappingComponents(const FOBB& OBB, TArray<UPrimitiveComponent*>& OutComponents) const
{
	OutComponents.clear();

	// 빈 트리이거나 루트가 유효하지 않은 경우
	if (RootIndex < 0 || RootIndex >= static_cast<int32>(Nodes.size()))
	{
		return false;
	}

	// 스택을 사용한 반복적 순회
	TArray<int32> NodeStack;
	NodeStack.push_back(RootIndex);

	while (!NodeStack.empty())
	{
		int32 CurrentNodeIndex = NodeStack.back();
		NodeStack.pop_back();

		const FSceneNode& CurrentNode = Nodes[CurrentNodeIndex];

		// OBB와 현재 노드의 AABB 교차 검사
		if (!CheckIntersectionOBBAABB(OBB, CurrentNode.Box))
		{
			continue; // AABB와 교차하지 않으면 이 노드의 자식들도 건너뜀
		}

		if (CurrentNode.bIsLeaf)
		{
			// 리프 노드인 경우 Component 추가
			if (CurrentNode.Component)
			{
				OutComponents.push_back(CurrentNode.Component);
			}
		}
		else
		{
			// 내부 노드인 경우 자식들을 스택에 추가
			if (CurrentNode.Child1 >= 0 && CurrentNode.Child1 < static_cast<int32>(Nodes.size()))
			{
				NodeStack.push_back(CurrentNode.Child1);
			}
			if (CurrentNode.Child2 >= 0 && CurrentNode.Child2 < static_cast<int32>(Nodes.size()))
			{
				NodeStack.push_back(CurrentNode.Child2);
			}
		}
	}

	return !OutComponents.empty();
}

void FSceneBVH::GetDebugDrawInfo(TArray<FAABB>& OutBoxes, TArray<FVector4>& OutColors, int32 MaxDepth) const
{
	OutBoxes.clear();
	OutColors.clear();

	// 빈 트리이거나 루트가 유효하지 않은 경우
	if (RootIndex < 0 || RootIndex >= static_cast<int32>(Nodes.size()))
	{
		return;
	}

	// 계층별 색상 (깊이에 따라 다른 색상)
	TArray<FVector4> DepthColors = {
		FVector4(1.0f, 0.0f, 0.0f, 1.0f), // 빨강 - 루트
		FVector4(1.0f, 0.5f, 0.0f, 1.0f), // 주황
		FVector4(1.0f, 1.0f, 0.0f, 1.0f), // 노랑
		FVector4(0.0f, 1.0f, 0.0f, 1.0f), // 초록
		FVector4(0.0f, 1.0f, 1.0f, 1.0f), // 청록
		FVector4(0.0f, 0.0f, 1.0f, 1.0f), // 파랑
		FVector4(0.5f, 0.0f, 1.0f, 1.0f), // 보라
		FVector4(1.0f, 0.0f, 1.0f, 1.0f), // 자홍
	};

	// BFS로 순회하며 각 노드의 깊이 계산
	struct FNodeDepth
	{
		int32 NodeIndex;
		int32 Depth;
	};

	TArray<FNodeDepth> Queue;
	Queue.push_back({ RootIndex, 0 });

	while (!Queue.empty())
	{
		FNodeDepth Current = Queue[0];
		Queue.erase(Queue.begin());

		// MaxDepth 제한 확인
		if (MaxDepth >= 0 && Current.Depth > MaxDepth)
		{
			continue;
		}

		const FSceneNode& Node = Nodes[Current.NodeIndex];

		// AABB 추가
		OutBoxes.push_back(Node.Box);

		// 깊이에 따른 색상 선택 (순환)
		int32 ColorIndex = Current.Depth % DepthColors.size();
		OutColors.push_back(DepthColors[ColorIndex]);

		// 자식 노드 추가
		if (!Node.bIsLeaf)
		{
			if (Node.Child1 >= 0 && Node.Child1 < static_cast<int32>(Nodes.size()))
			{
				Queue.push_back({ Node.Child1, Current.Depth + 1 });
			}
			if (Node.Child2 >= 0 && Node.Child2 < static_cast<int32>(Nodes.size()))
			{
				Queue.push_back({ Node.Child2, Current.Depth + 1 });
			}
		}
	}
}

int32 FSceneBVH::FindLeafNode(UPrimitiveComponent* InComponent) const
{
	if (!InComponent)
	{
		return -1;
	}

	// 해시맵에서 O(1)로 검색
	auto It = ComponentToNodeMap.find(InComponent);
	if (It != ComponentToNodeMap.end())
	{
		return It->second;
	}

	return -1;
}

void FSceneBVH::RemoveLeaf(int32 LeafIndex)
{
	if (LeafIndex < 0 || LeafIndex >= static_cast<int32>(Nodes.size()))
	{
		return;
	}

	FSceneNode& Leaf = Nodes[LeafIndex];
	if (!Leaf.bIsLeaf)
	{
		return; // 리프가 아니면 제거 불가
	}

	// 해시맵에서 제거
	if (Leaf.Component)
	{
		ComponentToNodeMap.erase(Leaf.Component);
	}

	// 트리에 노드가 하나만 있는 경우 (루트 = 리프)
	if (LeafIndex == RootIndex)
	{
		Clear();
		return;
	}

	int32 ParentIndex = Leaf.ParentIndex;
	if (ParentIndex < 0 || ParentIndex >= static_cast<int32>(Nodes.size()))
	{
		return;
	}

	FSceneNode& Parent = Nodes[ParentIndex];
	int32 SiblingIndex = (Parent.Child1 == LeafIndex) ? Parent.Child2 : Parent.Child1;
	int32 GrandParentIndex = Parent.ParentIndex;

	// 형제 노드를 할아버지 노드에 연결
	if (GrandParentIndex != -1)
	{
		FSceneNode& GrandParent = Nodes[GrandParentIndex];
		if (GrandParent.Child1 == ParentIndex)
		{
			GrandParent.Child1 = SiblingIndex;
		}
		else
		{
			GrandParent.Child2 = SiblingIndex;
		}
		Nodes[SiblingIndex].ParentIndex = GrandParentIndex;

		// 할아버지부터 루트까지 AABB 리피팅
		RefitAncestors(SiblingIndex);
	}
	else
	{
		// 부모가 루트였던 경우, 형제가 새 루트가 됨
		RootIndex = SiblingIndex;
		Nodes[SiblingIndex].ParentIndex = -1;
	}

	// 제거된 노드를 무효화 (실제로는 배열에서 제거하지 않고 Component를 null로 설정)
	Leaf.Component = nullptr;
	Leaf.bIsLeaf = false;
	Parent.Component = nullptr;
	Parent.bIsLeaf = false;
	Parent.Child1 = -1;
	Parent.Child2 = -1;

	Cost = GetCost(RootIndex);
}

bool FSceneBVH::UpdateComponent(UPrimitiveComponent* InComponent)
{
	if (!InComponent)
	{
		return false;
	}

	// 1. 기존 노드 찾기
	int32 LeafIndex = FindLeafNode(InComponent);
	if (LeafIndex == -1)
	{
		// 노드가 없으면 새로 삽입
		InsertLeaf(InComponent);
		return true;
	}

	// 2. 기존 노드 제거
	RemoveLeaf(LeafIndex);

	// 3. 새 위치에 다시 삽입
	InsertLeaf(InComponent);

	return true;
}

bool FSceneBVH::RemoveComponent(UPrimitiveComponent* InComponent)
{
	if (!InComponent)
	{
		return false;
	}

	int32 LeafIndex = FindLeafNode(InComponent);
	if (LeafIndex == -1)
	{
		return false;
	}

	RemoveLeaf(LeafIndex);
	return true;
}
