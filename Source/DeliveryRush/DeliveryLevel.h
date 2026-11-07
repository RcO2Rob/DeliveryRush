#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelLayout.h"
#include "DeliveryLevel.generated.h"

class AStaticMeshActor;
class UBoxComponent;
class USplineComponent;
class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;

// Move/duplicate these actors to shape the height field in the editor.
UCLASS(meta=(DisplayName="Delivery Hill"))
class DELIVERYRUSH_API ADeliveryHill : public AActor
{
    GENERATED_BODY()
public:
    ADeliveryHill();
    virtual void OnConstruction(const FTransform& Transform) override;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Hill",meta=(ClampMin="100",Units="cm")) double RadiusX=6500;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Hill",meta=(ClampMin="100",Units="cm")) double RadiusY=6500;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Hill",meta=(Units="cm")) double Height=900;
    UFUNCTION(BlueprintCallable,CallInEditor,Category="Hill") void RefreshTerrain();
#if WITH_EDITOR
    virtual void PostEditMove(bool bFinished) override;
    virtual void PostEditUndo() override;
    virtual void Destroyed() override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
private:
    UPROPERTY() UBoxComponent* Outline;
};

UCLASS(meta=(DisplayName="Delivery Road"))
class DELIVERYRUSH_API ADeliveryRoad : public AActor
{
    GENERATED_BODY()
public:
    ADeliveryRoad();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void PostRegisterAllComponents() override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Road") USplineComponent* Spline;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Road",meta=(ClampMin="100",Units="cm")) double Width=1800;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Road") bool bDirtPath=false;
    UFUNCTION(BlueprintCallable,CallInEditor,Category="Road") void RebuildRoad();
private:
    // Retained to load older maps, whose road surface used overlapping instances.
    UPROPERTY() UInstancedStaticMeshComponent* Pieces;
    UPROPERTY(Transient) UProceduralMeshComponent* Ribbon;
};

UCLASS(meta=(DisplayName="Delivery Level Settings"))
class DELIVERYRUSH_API ADeliveryLevelSettings : public AActor
{
    GENERATED_BODY()
public:
    ADeliveryLevelSettings();
    UFUNCTION(BlueprintCallable,Category="Delivery|Setup")
    void MergeDecoration(AStaticMeshActor* Prop,AStaticMeshActor* Decoration);
    UPROPERTY(EditAnywhere,Category="Delivery") TObjectPtr<AActor> StartActor;
    UPROPERTY(EditAnywhere,Category="Delivery") TObjectPtr<AActor> GoalActor;
    UPROPERTY(EditAnywhere,Category="Delivery") TArray<TObjectPtr<AActor>> BridgeCheckpoints;
    UPROPERTY(EditAnywhere,Category="Delivery",meta=(ClampMin="10",Units="s")) double TimeLimit=100;
    UPROPERTY(EditAnywhere,Category="Delivery",meta=(ClampMin="1000",Units="cm")) double WorldHalfExtent=20800;
};

namespace Delivery
{
std::shared_ptr<const LevelLayout> ReadLevelLayout(UWorld* World);
}
