#include "pch.h"
#include "Utility/Public/ActorTypeMapper.h"

#include "Global/Types.h"
#include "Component/Mesh/Public/CubeComponent.h"
#include "Component/Mesh/Public/SphereComponent.h"
#include "Component/Mesh/Public/TriangleComponent.h"
#include "Component/Mesh/Public/SquareComponent.h"
#include "Component/Public/LineComponent.h"
#include "Component/Public/SceneComponent.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Actor/Public/CubeActor.h"
#include "Actor/Public/MovingCubeActor.h"
#include "Actor/Public/SphereActor.h"
#include "Actor/Public/SquareActor.h"
#include "Actor/Public/StaticMeshActor.h"
#include "Actor/Public/TriangleActor.h"
#include "Actor/Public/BillBoardActor.h"
#include "Actor/Public/TextActor.h"
#include "Actor/Public/DecalActor.h"
#include "Actor/Public/SemiLightActor.h"
#include "Core/Public/Class.h"

// 하드 코딩으로 구현
FString FActorTypeMapper::ActorToType(UClass* InClass)
{
	FName TypeName = InClass->GetName();

	if (TypeName == ACubeActor::StaticClass()->GetName())
	{
		return "Cube";
	}
	else if (TypeName == AMovingCubeActor::StaticClass()->GetName())
	{
		return "MovingCube";
	}
	else if (TypeName == ASphereActor::StaticClass()->GetName())
	{
		return "Sphere";
	}
	else if (TypeName == ASquareActor::StaticClass()->GetName())
	{
		return "Square";
	}
	else if (TypeName == ATriangleActor::StaticClass()->GetName())
	{
		return "Triangle";
	}
	else if (TypeName == AStaticMeshActor::StaticClass()->GetName())
	{
		return "StaticMeshComp";
	}
	else if (TypeName == ABillBoardActor::StaticClass()->GetName()) 
	{ 
		return "Billboard"; 
	}
	else if (TypeName == ATextActor::StaticClass()->GetName()) 
	{
		return "Text";
	}
	else if (TypeName == ADecalActor::StaticClass()->GetName())
	{ 
		return "Decal";
	}
	else if (TypeName == ASemiLightActor::StaticClass()->GetName()) 
	{
		return "SemiLight"; 
	}
	// Fallback: 클래스 이름 그대로 저장해도 로드에서 복구 가능
	return TypeName.ToString();
}

// 하드 코딩으로 구현
UClass* FActorTypeMapper::TypeToActor(const FString& InTypeString)
{
	UClass* NewActorClass = nullptr;

	// 타입에 따라 액터 생성
	if (InTypeString == "Cube")
	{
		NewActorClass = ACubeActor::StaticClass();
	}
	else if (InTypeString == "MovingCube")
	{
		NewActorClass = AMovingCubeActor::StaticClass();
	}
	else if (InTypeString == "Sphere")
	{
		NewActorClass = ASphereActor::StaticClass();
	}
	else if (InTypeString == "Triangle")
	{
		NewActorClass = ATriangleActor::StaticClass();
	}
	else if (InTypeString == "Square")
	{
		NewActorClass = ASquareActor::StaticClass();
	}
	else if (InTypeString == "StaticMeshComp")
	{
		NewActorClass = AStaticMeshActor::StaticClass();
	}
	else if (InTypeString == "Billboard") 
	{ 
		NewActorClass = ABillBoardActor::StaticClass();
	}
	else if (InTypeString == "Text") 
	{
		NewActorClass = ATextActor::StaticClass(); 
	}
	else if (InTypeString == "Decal") 
	{
		NewActorClass = ADecalActor::StaticClass(); 
	}
	else if (InTypeString == "SemiLight") 
	{
		NewActorClass = ASemiLightActor::StaticClass();
	}
	// Fallback: 저장 때 클래스 이름을 그대로 넣은 경우 복구 시도
	if (!NewActorClass)
	{
		NewActorClass = UClass::FindClass(InTypeString);
	}
	return NewActorClass;
}
