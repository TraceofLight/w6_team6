#pragma once
#include "Core/Public/Object.h"
#include "Editor/Public/Camera.h"
#include "Global/Enum.h"

namespace json { class JSON; }
using JSON = json::JSON;

class UWorld;
class AActor;
class UPrimitiveComponent;
class UDecalComponent;
class FOctree;
class FSceneBVH;

UCLASS()
class ULevel :
	public UObject
{
	GENERATED_BODY()
	DECLARE_CLASS(ULevel, UObject)
public:
	ULevel();
	ULevel(const FName& InName);
	~ULevel() override;

	virtual void Init();

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	const TArray<AActor*>& GetActors() const { return Actors; }

	void AddPrimitiveComponent(AActor* Actor);

	void RegisterPrimitiveComponent(UPrimitiveComponent* InComponent);
	void UnregisterPrimitiveComponent(UPrimitiveComponent* InComponent);
	bool DestroyActor(AActor* InActor);

	uint64 GetShowFlags() const { return ShowFlags; }
	void SetShowFlags(uint64 InShowFlags) { ShowFlags = InShowFlags; }

	void UpdatePrimitiveInOctree(UPrimitiveComponent* InComponent);

	FOctree* GetStaticOctree() { return StaticOctree; }
	TArray<UPrimitiveComponent*>& GetDynamicPrimitives() { return DynamicPrimitives; }

	// ========================================
	// Decal Management API
	// ========================================

	/**
	 * Decal 컴포넌트 등록
	 * Actor::RegisterComponent()에서 자동 호출됨
	 */
	void RegisterDecalComponent(UDecalComponent* InDecal);

	/**
	 * Decal 컴포넌트 등록 해제
	 * Actor::RemoveComponent()에서 자동 호출됨
	 */
	void UnregisterDecalComponent(UDecalComponent* InDecal);

	/**
	 * Decal 트랜스폼 변경 알림
	 * DecalComponent::MarkAsDirty()에서 호출
	 */
	void UpdateDecalDirtyFlag(UDecalComponent* InDecal);

	/**
	 * Decal 가시성 변경 알림
	 * DecalComponent::SetVisibility()에서 호출
	 */
	void OnDecalVisibilityChanged(UDecalComponent* InDecal, bool bVisible);

	/**
	 * Renderer가 사용할 가시 Decal 목록
	 * @return 렌더링 가능한 Decal 배열 (const 참조)
	 */
	const TArray<UDecalComponent*>& GetVisibleDecals() const { return VisibleDecals; }

	/**
	 * BVH 재구축 필요 여부
	 */
	bool NeedsDecalIndexRebuild() const { return bDecalsDirty; }

	/**
	 * BVH 재구축 완료 후 호출
	 */
	void MarkDecalIndexClean();

	/**
	 * 변경된 Decal 집합
	 * 증분 BVH 업데이트에 사용
	 */
	const TSet<UDecalComponent*>& GetDirtyDecals() const { return DirtyDecals; }

	// ========================================
	// Scene BVH Management API
	// ========================================

	/**
	 * Scene BVH 구축 (모든 Dynamic Primitives로부터)
	 */
	void BuildSceneBVH();

	/**
	 * Scene BVH 시각화 토글
	 * @param bShow: 표시 여부
	 * @param MaxDepth: 표시할 최대 깊이 (-1이면 전체)
	 */
	void ToggleSceneBVHVisualization(bool bShow, int32 MaxDepth = -1);

	/**
	 * Scene BVH 디버그 렌더링
	 */
	void RenderSceneBVHDebug();

	/**
	 * Scene BVH 가져오기
	 */
	FSceneBVH* GetSceneBVH() { return SceneBVH; }

	/**
	 * 캐시된 디버그 박스 데이터 가져오기
	 */
	const TArray<FAABB>& GetCachedDebugBoxes() const { return CachedDebugBoxes; }
	const TArray<FVector4>& GetCachedDebugColors() const { return CachedDebugColors; }

	/**
	 * Scene BVH 활성화 여부 확인
	 */
	bool IsSceneBVHEnabled() const { return bShowSceneBVH; }

	/**
	 * Scene BVH에서 특정 Component와 그 자식들 재귀적으로 업데이트
	 * @param InComponent: 업데이트할 Component (자식 포함)
	 */
	void UpdateSceneBVHComponent(USceneComponent* InComponent);

	/**
	 * Scene BVH를 사용하여 OBB와 겹치는 Component들 찾기
	 * @param OBB: 검사할 OBB
	 * @param OutComponents: 겹치는 Component들 (output)
	 * @return: 겹치는 Component가 있으면 true
	 */
	bool QueryOverlappingComponentsWithBVH(const struct FOBB& OBB, TArray<UPrimitiveComponent*>& OutComponents) const;

	friend class UWorld;
public:
	virtual UObject* Duplicate() override;

protected:
	virtual void DuplicateSubObjects(UObject* DuplicatedObject) override;

private:
	AActor* SpawnActorToLevel(UClass* InActorClass, const FName& InName = FName::GetNone(), JSON* ActorJsonData = nullptr);

	TArray<AActor*> Actors;	// 레벨이 보유하고 있는 모든 Actor를 배열로 저장합니다.
	FOctree* StaticOctree = nullptr;
	TArray<UPrimitiveComponent*> DynamicPrimitives;

	// 지연 삭제를 위한 리스트
	TArray<AActor*> ActorsToDelete;

	uint64 ShowFlags = static_cast<uint64>(EEngineShowFlags::SF_Primitives) |
		static_cast<uint64>(EEngineShowFlags::SF_Billboard) |
		static_cast<uint64>(EEngineShowFlags::SF_Bounds) |
		static_cast<uint64>(EEngineShowFlags::SF_StaticMesh) |
		static_cast<uint64>(EEngineShowFlags::SF_Text) |
		static_cast<uint64>(EEngineShowFlags::SF_Decal);
	// ========================================
	// Decal Cache
	// ========================================
	TArray<UDecalComponent*> AllDecals;           // 모든 Decal
	TArray<UDecalComponent*> VisibleDecals;       // 가시 Decal만
	TSet<UDecalComponent*> DirtyDecals;           // 변경된 Decal
	bool bDecalsDirty = false;                     // BVH 재구축 플래그

	// ========================================
	// Scene BVH
	// ========================================
	FSceneBVH* SceneBVH = nullptr;
	bool bShowSceneBVH = false;
	int32 BVHDebugMaxDepth = -1;

	// 디버그 렌더링용 데이터
	TArray<FAABB> CachedDebugBoxes;
	TArray<FVector4> CachedDebugColors;
};
