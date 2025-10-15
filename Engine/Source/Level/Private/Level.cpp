#include "pch.h"
#include "Level/Public/Level.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Component/Public/DecalComponent.h"
#include "Editor/Public/Editor.h"
#include "Actor/Public/Actor.h"
#include "Core/Public/Object.h"
#include "Manager/Config/Public/ConfigManager.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Editor/Public/Viewport.h"
#include "Utility/Public/JsonSerializer.h"
#include "Utility/Public/ActorTypeMapper.h"
#include "Global/Octree.h"
#include "Global/SceneBVH.h"
#include <json.hpp>

#include "Component/Public/PointLightComponent.h"
#include "Component/Public/UUIDTextComponent.h"
#include "Render/UI/Widget/Public/DecalComponentWidget.h"

IMPLEMENT_CLASS(ULevel, UObject)

ULevel::ULevel()
{
	StaticOctree = new FOctree(FVector(0, 0, -5), 75, 0);
}

ULevel::ULevel(const FName& InName)
	: UObject(InName)
{
	StaticOctree = new FOctree(FVector(0, 0, -5), 75, 0);
}

ULevel::~ULevel()
{
	// LevelActors 배열에 남아있는 모든 액터의 메모리를 해제합니다.
	for (const auto& Actor : Actors)
	{
		DestroyActor(Actor);
	}
	Actors.clear();

	// 모든 액터 객체가 삭제되었으므로, 포인터를 담고 있던 컨테이너들을 비웁니다.
	SafeDelete(StaticOctree);
	SafeDelete(SceneBVH);
	DynamicPrimitives.clear();
}

void ULevel::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// 불러오기
	if (bInIsLoading)
	{
		// NOTE: 레벨 로드 시 NextUUID를 변경하면 UUID 충돌이 발생하므로 관련 기능 구현을 보류합니다.
		uint32 NextUUID = 0;
		FJsonSerializer::ReadUint32(InOutHandle, "NextUUID", NextUUID);

		JSON PerspectiveCameraData;
		if (FJsonSerializer::ReadObject(InOutHandle, "PerspectiveCamera", PerspectiveCameraData))
		{
			UConfigManager::GetInstance().SetCameraSettingsFromJson(PerspectiveCameraData);
			URenderer::GetInstance().GetViewportClient()->ApplyAllCameraDataToViewportClients();
		}

		JSON ActorsJson;
		if (FJsonSerializer::ReadObject(InOutHandle, "Actors", ActorsJson))
		{
			for (auto& Pair : ActorsJson.ObjectRange())
			{
				const FString& IdString = Pair.first;
				JSON& ActorDataJson = Pair.second;

				FString TypeString;
				FJsonSerializer::ReadString(ActorDataJson, "Type", TypeString);

				UClass* NewClass = FActorTypeMapper::TypeToActor(TypeString);
				AActor* NewActor = SpawnActorToLevel(NewClass, IdString, &ActorDataJson);
			}
		}

		// 모든 액터 로드 완료 후 BVH 리빌드 플래그 설정
		// 실제 빌드는 다음 프레임 TickLevel()에서 수행 (World Transform 갱신 후)
		bBVHNeedsRebuild = true;
	}

	// 저장
	else
	{
		// NOTE: 레벨 로드 시 NextUUID를 변경하면 UUID 충돌이 발생하므로 관련 기능 구현을 보류합니다.
		InOutHandle["NextUUID"] = 0;

		// GetCameraSetting 호출 전에 뷰포트 클라이언트의 최신 데이터를 ConfigManager로 동기화합니다.
		URenderer::GetInstance().GetViewportClient()->UpdateCameraSettingsToConfig();
		InOutHandle["PerspectiveCamera"] = UConfigManager::GetInstance().GetCameraSettingsAsJson();

		JSON ActorsJson = json::Object();
		for (AActor* Actor : Actors)
		{
			JSON ActorJson;
			ActorJson["Type"] = FActorTypeMapper::ActorToType(Actor->GetClass());
			Actor->Serialize(bInIsLoading, ActorJson);

			ActorsJson[std::to_string(Actor->GetUUID())] = ActorJson;
		}
		InOutHandle["Actors"] = ActorsJson;
	}
}

void ULevel::Init()
{
	// TEST CODE

	// SceneBVH 생성 (비어있는 상태로)
	if (!SceneBVH)
	{
		SceneBVH = new FSceneBVH();
	}

	// BVH 빌드는 TickLevel()에서 수행 (Transform 갱신 후)
	bBVHNeedsRebuild = true;

	// 디버그 시각화는 필요 시 활성화
	// ToggleSceneBVHVisualization(true, -1);
}

AActor* ULevel::SpawnActorToLevel(UClass* InActorClass, const FName& InName, JSON* ActorJsonData)
{
	if (!InActorClass)
	{
		return nullptr;
	}

	AActor* NewActor = Cast<AActor>(NewObject(InActorClass));
	if (NewActor)
	{
		if (!InName.IsNone())
		{
			NewActor->SetName(InName);
		}
		Actors.push_back(NewActor);
		if (ActorJsonData != nullptr)
		{
			NewActor->Serialize(true, *ActorJsonData);
		}
		else
		{
			NewActor->InitializeComponents();
		}
		NewActor->BeginPlay();
		AddPrimitiveComponent(NewActor);

		// 에디터에서 단일 액터 스폰 시 BVH 리빌드 플래그 설정
		// (ActorJsonData가 nullptr이면 에디터에서 스폰한 것임)
		if (ActorJsonData == nullptr)
		{
			bBVHNeedsRebuild = true;
		}

		// TODO: DecalActor 특수처리한거임, 나중에는 삭제요망
		RegisterDecalComponent(Cast<UDecalComponent>(NewActor->GetRootComponent()));
		
		return NewActor;
	}

	return nullptr;
}

void ULevel::RegisterPrimitiveComponent(UPrimitiveComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	// StaticOctree에 먼저 삽입 시도
	if (StaticOctree->Insert(InComponent) == false)
	{
		// 실패하면 DynamicPrimitives 목록에 추가
		// 중복 추가를 방지하기 위해 이미 있는지 확인
		if (std::find(DynamicPrimitives.begin(), DynamicPrimitives.end(), InComponent) == DynamicPrimitives.end())
		{
			DynamicPrimitives.push_back(InComponent);
		}
	}

	// SceneBVH가 존재하면 BVH에 추가 (RefitComponent는 내부에서 InsertLeaf 호출)
	if (SceneBVH && !Cast<UUUIDTextComponent>(InComponent))
	{
		SceneBVH->RefitComponent(InComponent);
	}

	UE_LOG("Level: '%s' 컴포넌트를 씬에 등록했습니다.", InComponent->GetName().ToString().data());
}

void ULevel::UnregisterPrimitiveComponent(UPrimitiveComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}
	// StaticOctree에서 제거 시도
	if (StaticOctree->Remove(InComponent) == false)
	{
		// 실패하면 DynamicPrimitives 목록에서 찾아서 제거
		if (auto It = std::find(DynamicPrimitives.begin(), DynamicPrimitives.end(), InComponent); It != DynamicPrimitives.end())
		{
			*It = std::move(DynamicPrimitives.back());
			DynamicPrimitives.pop_back();
		}
	}

	// SceneBVH가 존재하면 BVH에서도 제거
	if (SceneBVH && !Cast<UUUIDTextComponent>(InComponent))
	{
		SceneBVH->RemoveComponent(InComponent);
	}
}

void ULevel::AddPrimitiveComponent(AActor* Actor)
{
	if (!Actor) return;

	for (auto& Component : Actor->GetOwnedComponents())
	{
		UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
		if (!PrimitiveComponent) { continue; }

		if (StaticOctree->Insert(PrimitiveComponent) == false)
		{
			DynamicPrimitives.push_back(PrimitiveComponent);
		}
	}

	// BVH 재구축은 호출자가 명시적으로 수행
	// (World Transform이 제대로 설정된 후에 호출되어야 함)
}

// Level에서 Actor 제거하는 함수
bool ULevel::DestroyActor(AActor* InActor)
{
	if (!InActor) return false;

	// 컴포넌트들을 옥트리와 BVH에서 제거
	for (auto& Component : InActor->GetOwnedComponents())
	{
		UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
		UnregisterPrimitiveComponent(PrimitiveComponent);
		UDecalComponent* DecalComponent = Cast<UDecalComponent>(Component);
		UnregisterDecalComponent(DecalComponent);
	}

	// LevelActors 리스트에서 제거
	if (auto It = std::find(Actors.begin(), Actors.end(), InActor); It != Actors.end())
	{
		*It = std::move(Actors.back());
		Actors.pop_back();
	}

	// 에디터 선택 해제(액터/컴포넌트 모두)
	if (UEditor* Editor = GEditor->GetEditorModule()) {
		if (Editor->GetSelectedActor() == InActor) {
			Editor->SelectActor(nullptr);
		}
		if (UActorComponent* SelComp = Editor->GetSelectedComponent()) {
			if (SelComp->GetOwner() == InActor) {
				Editor->SelectComponent(nullptr);
			}
		}
	}

	SafeDelete(InActor);
	UE_LOG("Level: Actor Destroyed Successfully");
	return true;
}

void ULevel::UpdatePrimitiveInOctree(UPrimitiveComponent* Primitive)
{
	if (!Primitive) { return; }

	// 1. StaticOctree에서 제거 먼저 시도
	if (StaticOctree->Remove(Primitive))
	{
		// 2. DynamicPrimitives로 등록
		DynamicPrimitives.push_back(Primitive);
	}
	else
	{
		// 3. Static에 없으면 그냥 Dynamic에 넣어줌 (중복 방지 체크 필요)
		if (std::find(DynamicPrimitives.begin(), DynamicPrimitives.end(), Primitive) == DynamicPrimitives.end())
		{
			DynamicPrimitives.push_back(Primitive);
		}
	}
}

UObject* ULevel::Duplicate()
{
	ULevel* Level = Cast<ULevel>(Super::Duplicate());
	Level->ShowFlags = ShowFlags;
	return Level;
}

void ULevel::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
	ULevel* DuplicatedLevel = Cast<ULevel>(DuplicatedObject);

	for (AActor* Actor : Actors)
	{
		AActor* DuplicatedActor = Cast<AActor>(Actor->Duplicate());
		DuplicatedLevel->Actors.push_back(DuplicatedActor);
		DuplicatedLevel->AddPrimitiveComponent(DuplicatedActor);

		// DecalComponent도 등록 (PrimitiveComponent와 별도 관리)
		for (auto& Component : DuplicatedActor->GetOwnedComponents())
		{
			if (UDecalComponent* DecalComp = Cast<UDecalComponent>(Component))
			{
				DuplicatedLevel->RegisterDecalComponent(DecalComp);
			}
		}
	}

	// SceneBVH 복제
	// PIE 월드에서도 BVH를 사용할 수 있도록 복제
	if (SceneBVH)
	{
		// 기존 BVH가 있으면 삭제
		if (DuplicatedLevel->SceneBVH)
		{
			SafeDelete(DuplicatedLevel->SceneBVH);
		}

		// 새로운 BVH 생성하고 복제된 컴포넌트들로 빌드
		DuplicatedLevel->SceneBVH = new FSceneBVH();

		// 복제된 레벨의 모든 PrimitiveComponent 수집
		TArray<UPrimitiveComponent*> DuplicatedPrimitives;
		for (AActor* Actor : DuplicatedLevel->Actors)
		{
			if (!Actor) continue;

			for (auto& Component : Actor->GetOwnedComponents())
			{
				UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
				if (!PrimitiveComponent) continue;
				if (Cast<UUUIDTextComponent>(PrimitiveComponent)) continue;

				DuplicatedPrimitives.push_back(PrimitiveComponent);
			}
		}

		// 복제된 컴포넌트들로 BVH 빌드
		DuplicatedLevel->SceneBVH->Build(DuplicatedPrimitives);

		UE_LOG("Level: Duplicated Scene BVH built with %d nodes (Total primitives: %zu)",
		       DuplicatedLevel->SceneBVH->GetNodeCount(), DuplicatedPrimitives.size());
	}
}

// ========================================
// Decal Management Implementation
// ========================================

void ULevel::RegisterDecalComponent(UDecalComponent* InDecal)
{
	if (!InDecal)
	{
		UE_LOG("Level: RegisterDecalComponent called with nullptr");
		return;
	}

	// 모든 Decal 목록에 추가
	AllDecals.push_back(InDecal);

	// 가시 Decal만 별도 관리
	if (InDecal->IsVisible())
	{
		VisibleDecals.push_back(InDecal);
	}

	// BVH 재구축 플래그
	DirtyDecals.insert(InDecal);
	bDecalsDirty = true;

	UE_LOG("Level: Decal '%s' registered (Visible: %d)",
		InDecal->GetName().ToString().data(),
		InDecal->IsVisible());
}

void ULevel::UnregisterDecalComponent(UDecalComponent* InDecal)
{
	if (!InDecal) return;

	// AllDecals에서 제거
	if (auto It = std::find(AllDecals.begin(), AllDecals.end(), InDecal);
		It != AllDecals.end())
	{
		*It = std::move(AllDecals.back());
		AllDecals.pop_back();
	}

	// VisibleDecals에서 제거
	if (auto It = std::find(VisibleDecals.begin(), VisibleDecals.end(), InDecal);
		It != VisibleDecals.end())
	{
		*It = std::move(VisibleDecals.back());
		VisibleDecals.pop_back();
	}

	// 더티 플래그 제거
	DirtyDecals.erase(InDecal);
	if (DirtyDecals.empty())
	{
		bDecalsDirty = false;
	}

	UE_LOG("Level: Decal '%s' unregistered", InDecal->GetName().ToString().data());
}

void ULevel::UpdateDecalDirtyFlag(UDecalComponent* InDecal)
{
	if (!InDecal) return;

	DirtyDecals.insert(InDecal);
	bDecalsDirty = true;

	// 디버깅 필요 시 활성화
	// UE_LOG("Level: Decal '%s' marked dirty", InDecal->GetName().ToString().data());
}

void ULevel::OnDecalVisibilityChanged(UDecalComponent* InDecal, bool bVisible)
{
	if (!InDecal) return;

	if (bVisible)
	{
		// 가시 목록에 추가
		if (std::find(VisibleDecals.begin(), VisibleDecals.end(), InDecal) == VisibleDecals.end())
		{
			VisibleDecals.push_back(InDecal);
		}
	}
	else
	{
		// 가시 목록에서 제거
		if (auto It = std::find(VisibleDecals.begin(), VisibleDecals.end(), InDecal);
			It != VisibleDecals.end())
		{
			*It = std::move(VisibleDecals.back());
			VisibleDecals.pop_back();
		}
	}

	// BVH 재구축 플래그
	UpdateDecalDirtyFlag(InDecal);

	UE_LOG("Level: Decal '%s' visibility changed to %d",
		InDecal->GetName().ToString().data(), bVisible);
}

void ULevel::MarkDecalIndexClean()
{
	DirtyDecals.clear();
	bDecalsDirty = false;
}

// ========================================
// Fireball Management Implementation
// ========================================

void ULevel::RegisterPointLight(UPointLightComponent* InFireball)
{
	if (!InFireball)
	{
		UE_LOG("Level: RegisterPointLight called with nullptr");
		return;
	}

	// PointLight 목록에 추가 (중복 방지)
	if (std::find(AllPointLights.begin(), AllPointLights.end(), InFireball) == AllPointLights.end())
	{
		AllPointLights.push_back(InFireball);
	}

	UE_LOG("Level: PointLight '%s' registered (Intensity: %.1f, Radius: %.1f)",
		InFireball->GetName().ToString().data(),
		InFireball->GetIntensity(),
		InFireball->GetRadius());
}

void ULevel::UnregisterPointLight(UPointLightComponent* InFireball)
{
	if (!InFireball)
	{
		return;
	}

	// AllPointLights에서 제거
	if (auto It = std::find(AllPointLights.begin(), AllPointLights.end(), InFireball);
		It != AllPointLights.end())
	{
		*It = std::move(AllPointLights.back());
		AllPointLights.pop_back();
	}

	UE_LOG("Level: PointLight '%s' unregistered", InFireball->GetName().ToString().data());
}

// ========================================
// Scene BVH Implementation
// ========================================

void ULevel::BuildSceneBVH()
{
	// 기존 BVH 제거
	if (SceneBVH)
	{
		SafeDelete(SceneBVH);
	}

	// 새 BVH 생성 및 구축
	SceneBVH = new FSceneBVH();

	// StaticOctree와 DynamicPrimitives 모두 포함
	TArray<UPrimitiveComponent*> AllPrimitives;
	for (auto& Actor : Actors)
	{
		if (!Actor) continue;

		for (auto& Component : Actor->GetOwnedComponents())
		{
			UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
			if (!PrimitiveComponent)
			{
				continue;
			}
			if (Cast<UUUIDTextComponent>(PrimitiveComponent))
			{
				continue;
			}
			AllPrimitives.push_back(PrimitiveComponent);
		}
	}

	SceneBVH->Build(AllPrimitives);

	UE_LOG("Level: Scene BVH built with %d nodes (Total primitives: %zu)",
	       SceneBVH->GetNodeCount(), AllPrimitives.size());
}

void ULevel::ToggleSceneBVHVisualization(bool bShow, int32 MaxDepth)
{
	bShowSceneBVH = bShow;
	BVHDebugMaxDepth = MaxDepth;

	// BVH가 없으면 생성
	if (bShow && !SceneBVH)
	{
		BuildSceneBVH();
	}

	UE_LOG("Level: Scene BVH visualization %s (MaxDepth: %d)",
		bShow ? "enabled" : "disabled", MaxDepth);
}

void ULevel::RenderSceneBVHDebug()
{
	// 시각화가 꺼져있거나 BVH가 없으면 캐시 클리어
	if (!bShowSceneBVH || !SceneBVH)
	{
		CachedDebugBoxes.clear();
		CachedDebugColors.clear();
		return;
	}

	// 디버그 정보 가져와서 캐싱
	SceneBVH->GetDebugDrawInfo(CachedDebugBoxes, CachedDebugColors, BVHDebugMaxDepth);

	// 실제 렌더링은 여기서 하지 않고, 렌더러나 Editor에서
	// GetCachedDebugBoxes()를 통해 데이터를 가져가서 그리도록 함

	// TODO: 실제 라인 렌더링은 BatchLines나 별도의 DebugDrawer를 통해 수행
	// 예시:
	// for (int32 i = 0; i < CachedDebugBoxes.size(); ++i)
	// {
	//     DrawDebugBox(CachedDebugBoxes[i], CachedDebugColors[i]);
	// }
}

void ULevel::UpdateSceneBVHComponent(USceneComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	if (!SceneBVH)
	{
		BuildSceneBVH();
	}

	// 현재 컴포넌트가 PrimitiveComponent면 BVH 업데이트
	if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(InComponent))
	{
		if (Cast<UUUIDTextComponent>(Primitive))
		{
			return;
		}
		// Transform 변경만 → Refit 사용 (O(depth), 빠름)
		// 노드 구조는 그대로, AABB만 갱신
		SceneBVH->RefitComponent(Primitive);
	}

	// 자식 컴포넌트들도 재귀적으로 업데이트
	for (USceneComponent* Child : InComponent->GetChildren())
	{
		if (Child)
		{
			UpdateSceneBVHComponent(Child);
		}
	}

	// 시각화가 켜져있으면 디버그 정보 갱신 (루트 컴포넌트일 때만)
	if (bShowSceneBVH && InComponent->GetParentAttachment() == nullptr)
	{
		SceneBVH->GetDebugDrawInfo(CachedDebugBoxes, CachedDebugColors, BVHDebugMaxDepth);
	}
}

bool ULevel::QueryOverlappingComponentsWithBVH(const FOBB& OBB, TArray<UPrimitiveComponent*>& OutComponents) const
{
	if (!SceneBVH)
	{
		OutComponents.clear();
		return false;
	}

	return SceneBVH->QueryOverlappingComponents(OBB, OutComponents);
}

void ULevel::TickLevel()
{
	// BVH 리빌드가 필요한 경우
	if (bBVHNeedsRebuild)
	{
		// 이 시점에는 모든 World Transform이 갱신됨
		BuildSceneBVH();
		bBVHNeedsRebuild = false;

		UE_LOG("Level: BVH rebuilt during TickLevel()");
	}
}