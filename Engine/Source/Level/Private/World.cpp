#include "pch.h"
#include "Level/Public/World.h"
#include "Level/Public/Level.h"
#include "Utility/Public/JsonSerializer.h"
#include "Manager/Config/Public/ConfigManager.h"
#include "Manager/Path/Public/PathManager.h"
#include "Component/Public/ActorComponent.h"
#include "Editor/Public/EditorEngine.h"
#include "Editor/Public/Editor.h"
IMPLEMENT_CLASS(UWorld, UObject)

UWorld::UWorld()
	: WorldType(EWorldType::Editor)
	, bBegunPlay(false)
{
}

UWorld::UWorld(EWorldType InWorldType)
	: WorldType(InWorldType)
	, bBegunPlay(false)
{
}

UWorld::~UWorld()
{
	EndPlay();
	if (Level)
	{
		ULevel* CurrentLevel = Level;
		SafeDelete(CurrentLevel); // 내부 Clean up은 Level의 소멸자에서 수행
		Level = nullptr;
	}
}

void UWorld::BeginPlay()
{
	if (bBegunPlay)
	{
		return;
	}

	if (!Level)
	{
		UE_LOG_ERROR("World: BeginPlay 호출 전에 로드된 Level이 없습니다.");
		return;
	}

	Level->Init();
	bBegunPlay = true;
}

bool UWorld::EndPlay()
{
	if (!Level || !bBegunPlay)
	{
		bBegunPlay = false;
		return false;
	}

	FlushPendingDestroy();
	// Level EndPlay
	bBegunPlay = false;
	return true;
}

void UWorld::Tick(float DeltaTimes)
{
	if (!Level || !bBegunPlay)
	{
		return;
	}

	// 스폰 / 삭제 처리
	FlushPendingDestroy();

	// Level Tick (BVH 리빌드 등)
	Level->TickLevel();

	if (WorldType == EWorldType::Editor )
	{
		for (AActor* Actor : Level->GetActors())
		{
			if(Actor->CanTickInEditor() && Actor->CanTick())
			{
				Actor->Tick(DeltaTimes);
			}
		}
	}

	if (WorldType == EWorldType::Game || WorldType == EWorldType::PIE)
	{
		for (AActor* Actor : Level->GetActors())
		{
			if(Actor->CanTick())
			{
				Actor->Tick(DeltaTimes);
			}
		}
	}
}

ULevel* UWorld::GetLevel() const
{
	return Level;
}

/**
* @brief 지정된 경로에서 Level을 로드하고 현재 Level로 전환합니다.
* @param InLevelFilePath 로드할 Level 파일 경로
* @return 로드 성공 여부
* @note FilePath는 최종 확정된 경로여야 합니다. EditorEngine을 통해 호출됩니다.
*/
bool UWorld::LoadLevel(path InLevelFilePath)
{
	JSON LevelJson;
	ULevel* NewLevel = nullptr;

	try
	{
		FString LevelNameString = InLevelFilePath.stem().string();
		NewLevel = new ULevel(FName(LevelNameString));

		if (!FJsonSerializer::LoadJsonFromFile(LevelJson, InLevelFilePath.string()))
		{
			UE_LOG_ERROR("World: Level JSON 로드에 실패했습니다: %s", InLevelFilePath.string().c_str());
			SafeDelete(NewLevel);
			return false;
		}

		NewLevel->SetOuter(this);
		SwitchToLevel(NewLevel);
		NewLevel->Serialize(true, LevelJson);

		// 에디터에서도 틱/삭제/페이드가 다시 돌도록 보장
		BeginPlay();
		UConfigManager::GetInstance().SetLastUsedLevelPath(InLevelFilePath.string());
	}
	catch (const exception& Exception)
	{
		UE_LOG_ERROR("World: Level 로드 중 예외 발생: %s", Exception.what());
		SafeDelete(NewLevel);
		CreateNewLevel();
		return false;
	}


	return true;
}

/**
* @brief 현재 Level을 지정된 경로에 저장합니다.
* @param InLevelFilePath 저장할 파일 경로
* @return 저장 성공 여부
* @note FilePath는 최종 확정된 경로여야 합니다. EditorEngine을 통해 호출됩니다.
*/
bool UWorld::SaveCurrentLevel(path InLevelFilePath) const
{
	if (!Level)
	{
		UE_LOG_ERROR("World: 저장할 Level이 없습니다.");
		return false;
	}

	if(WorldType != EWorldType::Editor && WorldType != EWorldType::EditorPreview)
	{
		UE_LOG_ERROR("World: 게임 또는 PIE 모드에서는 Level 저장이 허용되지 않습니다.");
		return false;
	}

	try
	{
		JSON LevelJson;
		Level->Serialize(false, LevelJson);

		if (!FJsonSerializer::SaveJsonToFile(LevelJson, InLevelFilePath.string()))
		{
			UE_LOG_ERROR("World: Level 저장에 실패했습니다: %s", InLevelFilePath.string().c_str());
			return false;
		}

	}
	catch (const exception& Exception)
	{
		UE_LOG_ERROR("World: Level 저장 중 예외 발생: %s", Exception.what());
		return false;
	}

	return true;
}

AActor* UWorld::SpawnActor(UClass* InActorClass, const FName& InName, JSON* ActorJsonData)
{
	if (!Level)
	{
		UE_LOG_ERROR("World: Actor를 Spawn할 수 있는 Level이 없습니다.");
		return nullptr;
	}

	return Level->SpawnActorToLevel(InActorClass, InName, ActorJsonData);
}

/**
* @brief 지정된 Actor를 월드에서 삭제합니다. 실제 삭제는 안전한 시점에 이루어집니다.
* @param InActor 삭제할 Actor
* @return 삭제 요청이 성공적으로 접수되었는지 여부
*/
bool UWorld::DestroyActor(AActor* InActor)
{
	if (!Level)
	{
		UE_LOG_ERROR("World: Level이 없어 Actor 삭제를 수행할 수 없습니다.");
		return false;
	}

	if (!InActor)
	{
		UE_LOG_ERROR("World: DestroyActor에 null 포인터가 전달되었습니다.");
		return false;
	}

	if (std::find(PendingDestroyActors.begin(), PendingDestroyActors.end(), InActor) != PendingDestroyActors.end())
	{
		UE_LOG_ERROR("World: 이미 삭제 대기 중인 액터에 대한 중복 삭제 요청입니다.");
		return false; // 이미 삭제 대기 중인 액터
	}
	// 대기열에 넣기 전에 옥트리/동적 목록에서 즉시 제거해 같은 프레임 컬러 안전
	if (Level && InActor) {
		for (UActorComponent* Comp : InActor->GetOwnedComponents()) {
			if (auto* Prim = Cast<UPrimitiveComponent>(Comp)) {
				Level->UnregisterPrimitiveComponent(Prim);
			}
		}
	}
	// 선택도 해제
	if (GEditor) {
		if (GEditor->GetEditorModule()->GetSelectedActor() == InActor)
			GEditor->GetEditorModule()->SelectActor(nullptr);
		if (auto* SelComp = GEditor->GetEditorModule()->GetSelectedComponent())
			if (SelComp->GetOwner() == InActor) GEditor->GetEditorModule()->SelectComponent(nullptr);
	}

	PendingDestroyActors.push_back(InActor);
	return true;
}

bool UWorld::DestroyComponent(UActorComponent* InComponent)
{
	if (!InComponent) { UE_LOG_ERROR("World: DestroyComponent null"); return false; }

	// 컴포넌트 소유 액터가 이미 삭제 대기라면, 액터 삭제가 컴포넌트 해제까지 처리하므로 스킵
	if (AActor* Owner = InComponent->GetOwner())
	{
		if (std::find(PendingDestroyActors.begin(), PendingDestroyActors.end(), Owner) != PendingDestroyActors.end())
		{
			return true;
		}
	}

	if (std::find(PendingDestroyComponents.begin(), PendingDestroyComponents.end(), InComponent) !=
		PendingDestroyComponents.end())
	{
		return false; // 중복 예약 방지
	}

	// 바로 옥트리/동적에서 제거
	if (auto* Prim = Cast<UPrimitiveComponent>(InComponent)) {
		if (GetLevel()) GetLevel()->UnregisterPrimitiveComponent(Prim);
	}
	PendingDestroyComponents.push_back(InComponent);
	return true;
}

EWorldType UWorld::GetWorldType() const
{
	return WorldType;
}

void UWorld::SetWorldType(EWorldType InWorldType)
{
	WorldType = InWorldType;
}

/**
 * @brief 삭제 대기 중인 Actor들을 실제로 삭제합니다.
 * @note 이 함수는 Tick 루프 내에서 안전한 시점에 호출되어야 합니다.
 */
void UWorld::FlushPendingDestroy()
{
	if (!Level) { return; }

	// 1) 컴포넌트 삭제 먼저 처리
	if (!PendingDestroyComponents.empty())
	{
		TArray<UActorComponent*> ComponentsToProcess = PendingDestroyComponents;
		PendingDestroyComponents.clear();

		for (UActorComponent* Comp : ComponentsToProcess)
		{
			if (!Comp) { continue; }
			// 안전망: 아직 Owner가 들고 있다면 제거
			if (AActor* Owner = Comp->GetOwner())
			{
				auto& Owned = Owner->GetOwnedComponents();
				if (auto it = std::find(Owned.begin(), Owned.end(), Comp); it != Owned.end())
				{
					*it = std::move(Owned.back());
					Owned.pop_back();
				}
			}
			SafeDelete(Comp);
		}
	}

	// 2) 기존 Actor 삭제 처리 (기존 코드 유지)
	if (PendingDestroyActors.empty())
	{
		return;
	}

	TArray<AActor*> ActorsToProcess = PendingDestroyActors;
	PendingDestroyActors.clear();
	UE_LOG("World: %zu개의 Actor를 삭제합니다.", ActorsToProcess.size());
	for (AActor* ActorToDestroy : ActorsToProcess)
	{
		if (!Level->DestroyActor(ActorToDestroy))
		{
			UE_LOG_ERROR("World: Actor 삭제에 실패했습니다: %s", ActorToDestroy->GetName().ToString().c_str());
		}
	}
}

/**
 * @brief 현재 Level을 새 Level로 전환합니다. 기존 Level은 소멸됩니다.
 * @param InNewLevel 새로 전환할 Level
 * @note 이전 Level의 안전한 종료 및 메모리 해제를 여기에서 책입집니다.
 */
void UWorld::SwitchToLevel(ULevel* InNewLevel)
{
	// 1) 이전 월드 실행 중이면 대기 삭제 먼저 처리
	if (bBegunPlay) { FlushPendingDestroy(); }

	EndPlay(); // 내부에서 한 번 더 Flush 호출(중복 무해)

	if (Level) {
		ULevel* OldLevel = Level;
		SafeDelete(OldLevel);
		Level = nullptr;
	}

	// 2) 에디터 선택 상태 초기화(구 레벨 포인터 잔존 방지)
	if (GEditor) {
		GEditor->GetEditorModule()->SelectComponent(nullptr);
		GEditor->GetEditorModule()->SelectActor(nullptr);
	}

	// 3) 새 레벨 설정 + 삭제 대기 큐 초기화
	Level = InNewLevel;
	PendingDestroyActors.clear();
	PendingDestroyComponents.clear();
	bBegunPlay = false;
}

UObject* UWorld::Duplicate()
{
	UWorld* World = Cast<UWorld>(Super::Duplicate());
	return World;
}

void UWorld::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
	UWorld* World = Cast<UWorld>(DuplicatedObject);
	World->Level = Cast<ULevel>(Level->Duplicate());
}

void UWorld::CreateNewLevel(const FName& InLevelName)
{
	ULevel* NewLevel = NewObject<ULevel>();
	NewLevel->SetName(InLevelName);
	NewLevel->SetOuter(this);
	SwitchToLevel(NewLevel);
}
