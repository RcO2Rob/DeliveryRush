#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeliveryTerrain.generated.h"
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;

UCLASS()
class DELIVERYRUSH_API ADeliveryTerrain : public AActor
{
    GENERATED_BODY()
public:
    ADeliveryTerrain();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
#if WITH_EDITOR
    virtual void PreSave(FObjectPreSaveContext SaveContext) override;
#endif
    UPROPERTY(EditAnywhere,Category="Delivery|Terrain") bool bUseLevelHills=false;
    UFUNCTION(BlueprintPure,Category="Delivery|Terrain") double SampleHeight(double X,double Y) const;
    UFUNCTION(BlueprintPure,Category="Delivery|Terrain")
    static double GetGroundHeight(double X,double Y);
    UFUNCTION(BlueprintCallable,CallInEditor,Category="Delivery|Terrain")
    void BuildSurface();
    UFUNCTION(BlueprintCallable,CallInEditor,Category="Delivery|Terrain")
    UStaticMesh* BakeSurface();
private:
    void BindMaterials();
    UPROPERTY() TArray<double> HeightSamples;
    UPROPERTY(VisibleAnywhere) UProceduralMeshComponent* Surface;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* BakedSurface;
};
