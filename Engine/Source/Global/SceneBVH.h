#pragma once
#include "pch.h"
#include "Physics/Public/AABB.h"

class UPrimitiveComponent;

/**
* Scene-level BVH의 노드
* 삼각형 대신 PrimitiveComponent(메시, 액터 등)를 리프로 가짐
*/
struct FSceneNode
{
	int32 ObjectIndex;
	int32 ParentIndex;
	int32 Child1;
	int32 Child2;
	bool bIsLeaf;
	FAABB Box;
	UPrimitiveComponent* Component; // 리프 노드인 경우 해당 컴포넌트를 가리킴
};

/**
* Scene-level BVH (Bounding Volume Hierarchy)
* 여러 StaticMesh/PrimitiveComponent들을 계층적으로 관리
* OBB와의 교차 검사를 통해 어떤 오브젝트들과 겹치는지 효율적으로 판단
*/
class FSceneBVH
{
public:
	FSceneBVH() = default;

	/**
	* @brief: PrimitiveComponent 리스트로부터 Scene BVH 구축
	* @param InComponents: BVH에 포함할 Component 배열
	*/
	void Build(const TArray<UPrimitiveComponent*>& InComponents);

	/**
	* @brief: BVH 초기화
	*/
	void Clear();

	int32 GetRootIndex() const { return RootIndex; }
	int32 GetNodeCount() const { return static_cast<int32>(Nodes.size()); }
	const FSceneNode& GetNode(uint32 Index) const;
	FSceneNode& GetNode(uint32 Index);

	/**
	* @brief: 서브트리의 cost(노드가 가진 AABB의 표면적 합)을 계산.
	* @param SubTreeRootIndex: cost 계산 시작 노드 인덱스
	* @param bInternalOnly: true로 설정하면 leaf의 코스트는 포함 안시킴
	* @return cost 값
	*/
	float GetCost(int32 SubTreeRootIndex, bool bInternalOnly = false) const;

	/**
	* @brief: 트리의 유효성 검사.
	*/
	bool CheckValidity() const;

	/**
	* @brief: SceneBVH를 시각화하기 위한 디버그 정보 수집
	* @param OutBoxes: 각 노드의 AABB 정보 (output)
	* @param OutColors: 각 노드의 색상 (계층별 구분, output)
	* @param MaxDepth: 표시할 최대 깊이 (-1이면 전체)
	*/
	void GetDebugDrawInfo(TArray<FAABB>& OutBoxes, TArray<FVector4>& OutColors, int32 MaxDepth = -1) const;

	/**
	* @brief: 여러 OBB들과 Scene BVH를 비교하여 겹치는 OBB들의 인덱스 리스트를 반환
	* @param OBBList: 교차 검사를 수행할 OBB 배열
	* @param OutOBBIndices: Scene(메시들)과 겹치는 OBB들의 인덱스 리스트 (output)
	* @return: 겹치는 OBB가 있으면 true, 없으면 false
	*/
	bool QueryOverlappingOBBs(const TArray<struct FOBB>& OBBList, TArray<int32>& OutOBBIndices) const;

	/**
	* @brief: 하나의 OBB와 Scene BVH를 비교하여 겹치는 Component들의 리스트를 반환
	* @param OBB: 교차 검사를 수행할 OBB
	* @param OutComponents: OBB와 겹치는 Component들의 리스트 (output)
	* @return: 겹치는 Component가 있으면 true, 없으면 false
	*/
	bool QueryOverlappingComponents(const struct FOBB& OBB, TArray<UPrimitiveComponent*>& OutComponents) const;

	/**
	* @brief: 특정 Component의 Transform이 변경되었을 때 BVH에서 해당 노드 업데이트
	* @param InComponent: 업데이트할 Component
	* @return: 업데이트 성공 여부
	* @note: 제거/재삽입 방식 (느림, 구조 변경 시 사용)
	*/
	bool UpdateComponent(UPrimitiveComponent* InComponent);

	/**
	* @brief: 특정 Component의 Transform만 변경되었을 때 AABB만 갱신 (빠름)
	* @param InComponent: 업데이트할 Component
	* @return: 업데이트 성공 여부
	* @note: 트리 구조는 그대로, AABB만 리핏 (O(depth))
	*/
	bool RefitComponent(UPrimitiveComponent* InComponent);

	/**
	* @brief: 특정 Component를 BVH에서 제거
	* @param InComponent: 제거할 Component
	* @return: 제거 성공 여부
	*/
	bool RemoveComponent(UPrimitiveComponent* InComponent);

	/**
	* @brief: 새 리프 노드를 특정 노드의 형제로 추가했을 때 전체 업데이트 트리의 비용 증가량 계산
	* @param CandidateIndex: 후보 형제 노드 인덱스
	* @param NewLeafAABB: 새로운 리프 노드의 AABB
	* @return: 비용 증가량
	*/
	float CalculateCostIncrease(int32 CandidateIndex, const FAABB& NewLeafAABB) const;

private:
	/**
	* @brief 새로운 leaf node를 삽입.
	* @note cost가 가장 낮아지는 최적의 sibling node를 찾아서 삽입되도록 함.
	* @return 삽입된 leaf node의 인덱스, 실패 시 -1 반환
	*/
	int32 InsertLeaf(UPrimitiveComponent* InComponent);

	/**
	* @brief 특정 leaf node를 BVH에서 제거.
	* @param LeafIndex: 제거할 leaf node의 인덱스
	*/
	void RemoveLeaf(int32 LeafIndex);

	/**
	* @brief Component로부터 해당하는 leaf node 인덱스 찾기
	* @param InComponent: 찾을 Component
	* @return leaf node 인덱스, 못 찾으면 -1 반환
	*/
	int32 FindLeafNode(UPrimitiveComponent* InComponent) const;

	// --- 새 leaf node 삽입 과정 보조 메소드들 ---

	//@brief 새로운 leaf node가 추가되었을 때 cost가 가장 조금 증가하는 sibling node 탐색.
	int32 FindBestSibling(const FAABB& NewLeafAABB);
	//@brief 새로운 leaf node와 기존 sibling node를 묶는 internal node를 생성하고 트리에 삽입.
	void InsertInternalNode(int32 LeafIndex, int32 SiblingIndex);
	//@brief 주어진 노드의 '부모'부터 루트까지 올라가며 AABB Refit 수행.
	void RefitAncestors(int32 RefitStartIndex);

	TArray<FSceneNode> Nodes;
	int32 RootIndex = -1;
	float Cost = 0.0f;

	// Component -> Node Index 매핑 (O(1) 검색을 위함)
	TMap<UPrimitiveComponent*, int32> ComponentToNodeMap;
};
