#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "RideSimulation.h"
#include "DeliveryActors.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UPoseableMeshComponent;
class USkeletalMeshComponent;
class UAnimSequence;
class UCameraComponent;
class USpringArmComponent;
class APlayerController;
class UAudioComponent;
class USoundAttenuation;

UCLASS()
class DELIVERYRUSH_API ADeliveryGoalMarker : public AActor
{
    GENERATED_BODY()
public:
    ADeliveryGoalMarker();
};

UCLASS()
class DELIVERYRUSH_API ADeliveryTimePickup : public AActor
{
    GENERATED_BODY()
public:
    ADeliveryTimePickup();
};

UCLASS()
class DELIVERYRUSH_API ADeliveryFreezePickup : public AActor
{
    GENERATED_BODY()
public:
    ADeliveryFreezePickup();
};

UCLASS()
class DELIVERYRUSH_API ADeliveryPedestrian : public AActor
{
    GENERATED_BODY()
public:
    ADeliveryPedestrian();
    void SetOutfit(int32 Outfit);
    void Present(const Delivery::PedestrianWalk& Walk,double Elapsed,const Delivery::LevelLayout& Level);
private:
    UPROPERTY() USkeletalMeshComponent* Mesh;
    UPROPERTY() UAnimSequence* WalkAnimation;
};

UCLASS()
class DELIVERYRUSH_API ADeliveryBike : public APawn
{
    GENERATED_BODY()
public:
    ADeliveryBike();
    void Present(const Delivery::Simulation& State, float DeltaSeconds, bool bPedaling = false);
    void Look(float X, float Y, bool Held, float Dt);
private:
    UPROPERTY() USceneComponent* Body;
    UPROPERTY() USceneComponent* Steering;
    UPROPERTY() USceneComponent* FrontWheel;
    UPROPERTY() USceneComponent* RearWheel;
    UPROPERTY() UStaticMeshComponent* CrankArms[2];
    UPROPERTY() UStaticMeshComponent* Pedals[2];
    UPROPERTY() UPoseableMeshComponent* Courier;
    UPROPERTY() UStaticMeshComponent* Helmet;
    UPROPERTY() USpringArmComponent* CameraArm;
    UPROPERTY() UCameraComponent* Camera;
    void PresentCourier(const FVector (&PedalPositions)[2]);
    TArray<FTransform> SeatedPose;
    FIntVector Legs[2];
    FIntVector Arms[2];
    int32 HipsBone = INDEX_NONE, NeckBone = INDEX_NONE;
    float WheelAngle = 0, PedalAngle = 0, OrbitYaw = 0, OrbitPitch = -18;
};

UCLASS()
class DELIVERYRUSH_API ADeliveryCar : public AActor
{
    GENERATED_BODY()
public:
    ADeliveryCar();
    void Present(const Delivery::Car& State, const Delivery::LevelLayout* Level=nullptr);
    void UpdateAudio(const Delivery::Car& State, bool Audible, float DeltaSeconds);
private:
    UPROPERTY() USceneComponent* Body;
    UPROPERTY() UStaticMeshComponent* Chassis;
    UPROPERTY() UStaticMeshComponent* Wheels[4];
    UPROPERTY() UAudioComponent* EngineAudio;
    UPROPERTY() UAudioComponent* BrakeAudio;
    int PaintedIdentity = INDEX_NONE;
    Delivery::Vec2 PreviousPosition;
    double PreviousHeading = 0, WheelAngle = 0, FrontSteer = 0;
    bool bHasPreviousPose = false;
    float PreviousAudioSpeed = 0.f;
    float BrakeCooldown = 0.f;
};

UCLASS()
class DELIVERYRUSH_API ADeliveryGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ADeliveryGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    Delivery::Simulation Ride;
    UFUNCTION(BlueprintPure, Category="Delivery")
    FString GetRideStatus() const;
private:
    UPROPERTY() ADeliveryBike* Rider;
    UPROPERTY() TArray<ADeliveryCar*> Cars;
    UPROPERTY() AActor* ClinicGoal;
    UPROPERTY() ADeliveryGoalMarker* PracticeGoal;
    UPROPERTY() TArray<ADeliveryTimePickup*> TimePickupActors;
    UPROPERTY() TArray<ADeliveryFreezePickup*> FreezePickupActors;
    UPROPERTY() TArray<ADeliveryPedestrian*> Pedestrians;
    UPROPERTY() UAudioComponent* MusicComponent = nullptr;
    UPROPERTY() USoundAttenuation* TrafficImpactAttenuation = nullptr;
    bool PedestrianRefreshPending=false;
    void UpdateGoalMarker();
    void UpdatePickups();
    void SetUpPedestrians();
    void UpdateMouseInput(APlayerController& Controller) const;
    void UpdateMusic();
    void PlayTrafficImpact();
    void PlayCue(const TCHAR* Name, float Volume = .5f);
};

UCLASS()
class DELIVERYRUSH_API ADeliveryHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
private:
    float Scale = 1, HudWidth = 1280, HudHeight = 720;
    void Rect(float X, float Y, float W, float H, FLinearColor Color);
    void Text(const FString& Value, float X, float Y, float Size, FLinearColor Color,
        bool Centered = false, bool Italic = false);
    void Center(const FString& Value, float Y, float Size, FLinearColor Color, bool Italic = false);
};
