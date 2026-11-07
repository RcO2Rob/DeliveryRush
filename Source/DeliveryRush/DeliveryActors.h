#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "RideSimulation.h"
#include "DeliveryActors.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UCameraComponent;
class USpringArmComponent;
class APlayerController;

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
    void SetWalk(const Delivery::WalkRoute& Walk,float Speed,float StartProgress,int32 Outfit);
    void Present(double Elapsed,const Delivery::LevelLayout& Level);
private:
    UPROPERTY() UStaticMeshComponent* Shirt;
    UPROPERTY() UStaticMeshComponent* Arms[2];
    UPROPERTY() UStaticMeshComponent* Legs[2];
    Delivery::WalkRoute Route;
    double WalkSpeed=1.4, InitialDistance=0;
};

UCLASS()
class DELIVERYRUSH_API ADeliveryBike : public APawn
{
    GENERATED_BODY()
public:
    ADeliveryBike();
    void Present(const Delivery::Simulation& State, float DeltaSeconds);
    void Look(float X, float Y, bool Held, float Dt);
private:
    UPROPERTY() USceneComponent* Body;
    UPROPERTY() USceneComponent* FrontWheel;
    UPROPERTY() USceneComponent* RearWheel;
    UPROPERTY() UStaticMeshComponent* Legs[4];
    UPROPERTY() USpringArmComponent* CameraArm;
    UPROPERTY() UCameraComponent* Camera;
    float WheelAngle = 0, OrbitYaw = 0, OrbitPitch = -18;
};

UCLASS()
class DELIVERYRUSH_API ADeliveryCar : public AActor
{
    GENERATED_BODY()
public:
    ADeliveryCar();
    void Present(const Delivery::Car& State, const Delivery::LevelLayout* Level=nullptr);
private:
    UPROPERTY() USceneComponent* Body;
    UPROPERTY() UStaticMeshComponent* Chassis;
    UPROPERTY() UStaticMeshComponent* Roof;
    int PaintedIdentity = 0;
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
    bool PedestrianRefreshPending=false;
    void UpdateGoalMarker();
    void UpdatePickups();
    void SetUpPedestrians();
    void UpdateMouseInput(APlayerController& Controller) const;
    void PlayCue(const TCHAR* Name, float Volume = .5f);
};

UCLASS()
class DELIVERYRUSH_API ADeliveryHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
private:
    float Scale = 1, OffsetX = 0, OffsetY = 0;
    void Rect(float X, float Y, float W, float H, FLinearColor Color);
    void Text(const FString& Value, float X, float Y, float Size, FLinearColor Color);
    void Center(const FString& Value, float Y, float Size, FLinearColor Color);
};
