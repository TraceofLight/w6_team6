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

	// SceneBVH 테스트: 자동으로 BVH 구축 및 시각화
	if (!DynamicPrimitives.empty())
	{
		BuildSceneBVH();
		ToggleSceneBVHVisualization(true, -1); // 모든 레벨 표시
	}
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

	// SceneBVH 테스트: Dynamic Primitive가 추가되면 BVH 재구축
	if (!DynamicPrimitives.empty() && bShowSceneBVH)
	{
		BuildSceneBVH();
	}
}

// Level에서 Actor 제거하는 함수
bool ULevel::DestroyActor(AActor* InActor)
{
	if (!InActor) return false;

	// 컴포넌트들을 옥트리에서 제거
	for (auto& Component : InActor->GetOwnedComponents())
	{
		UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
		if (!PrimitiveComponent) { continue; }

		if (StaticOctree)
		{
			if (!StaticOctree->Remove(PrimitiveComponent))
			{
				if (auto It = std::find(DynamicPrimitives.begin(), DynamicPrimitives.end(), PrimitiveComponent); It != DynamicPrimitives.end())
				{
					*It = std::move(DynamicPrimitives.back());
					DynamicPrimitives.pop_back();
				}
			}
		}
	}

	// LevelActors 리스트에서 제거
	if (auto It = std::find(Actors.begin(), Actors.end(), InActor); It != Actors.end())
	{
		*It = std::move(Actors.back());
		Actors.pop_back();
	}

	// Remove Actor Selection
	UEditor* Editor = GEditor->GetEditorModule();
	if (Editor->GetSelectedActor() == InActor)
	{
		Editor->SelectActor(nullptr);
	}

	// Remove
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

	UE_LOG("Level: Decal '%s' marked dirty", InDecal->GetName().ToString().data());
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
	SceneBVH->Build(DynamicPrimitives);

	UE_LOG("Level: Scene BVH built with %d nodes", SceneBVH->GetNodeCount());
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
	if (!InComponent || !SceneBVH)
	{
		return;
	}

	// 현재 컴포넌트가 PrimitiveComponent면 BVH 업데이트
	if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(InComponent))
	{
		SceneBVH->UpdateComponent(Primitive);
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