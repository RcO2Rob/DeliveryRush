#include "DeliveryActors.h"
#include "DeliveryLevel.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Sound/SoundBase.h"
#include <cmath>

namespace
{
const FLinearColor Ink(.025f, .045f, .075f, .94f);
const FLinearColor White(.94f, .96f, 1.f);
const FLinearColor Muted(.58f, .68f, .75f);
const FLinearColor Teal(.19f, .94f, .73f);
const FLinearColor Amber(1.f, .68f, .18f);
const FLinearColor Red(1.f, .24f, .23f);
const FLinearColor Ice(.42f, .87f, 1.f);

const TCHAR* StageTitle(Delivery::CampaignStage Stage)
{
    switch(Stage)
    {
        case Delivery::CampaignStage::Tutorial: return TEXT("1 / 3   RIDING LESSON");
        case Delivery::CampaignStage::FirstChase: return TEXT("2 / 3   ONCOMING TRAFFIC");
        case Delivery::CampaignStage::CityRush: return TEXT("3 / 3   CITY RUSH");
    }
    return TEXT("DELIVERY RUSH");
}

UStaticMeshComponent* Part(AActor* Owner, USceneComponent* Parent, const FString& Name,
    const TCHAR* Shape, FVector Location, FVector Size, const TCHAR* Material,
    FRotator Rotation = FRotator::ZeroRotator)
{
    auto* Mesh = Owner->CreateDefaultSubobject<UStaticMeshComponent>(*Name);
    Mesh->SetupAttachment(Parent);
    const FString MeshPath = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *MeshPath));
    const FString MatPath = FString::Printf(TEXT("/Game/Art/M_%s.M_%s"), Material, Material);
    if (auto* Mat = LoadObject<UMaterialInterface>(nullptr, *MatPath)) Mesh->SetMaterial(0, Mat);
    Mesh->SetRelativeLocation(Location);
    Mesh->SetRelativeRotation(Rotation);
    Mesh->SetRelativeScale3D(Size / 100.f);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCastShadow(true);
    return Mesh;
}
void Between(UStaticMeshComponent* Mesh, FVector A, FVector B, float Diameter)
{
    Mesh->SetRelativeLocation((A + B) * .5f);
    Mesh->SetRelativeRotation(FRotationMatrix::MakeFromZ(B - A).Rotator());
    Mesh->SetRelativeScale3D(FVector(Diameter, Diameter, (B - A).Size()) / 100.f);
}
UStaticMeshComponent* Tube(AActor* Owner, USceneComponent* Parent, const FString& Name,
    FVector A, FVector B, float Diameter, const TCHAR* Material)
{
    auto* Mesh = Part(Owner, Parent, Name, TEXT("Cylinder"), FVector::ZeroVector,
        FVector(1), Material);
    Between(Mesh, A, B, Diameter);
    return Mesh;
}
void PlaceOnSurface(AActor* Actor,Delivery::Vec2 Position,double Heading,double HalfLength,double HalfWidth,const Delivery::LevelLayout* Level=nullptr)
{
    const auto Pose=Level?Delivery::PoseOnLevel(*Level,Position.x,Position.y,Heading,HalfLength,HalfWidth):Delivery::SurfacePose(Position.x,Position.y,Heading,HalfLength,HalfWidth);
    const double C=std::cos(Heading),S=std::sin(Heading);
    const FVector Forward(C,S,std::tan(Pose.pitch)),Right(-S,C,std::tan(Pose.bank));
    const FVector Up=FVector::CrossProduct(Forward,Right).GetSafeNormal();
    Actor->SetActorLocation(FVector(Position.x*100,Position.y*100,Pose.height*100));
    Actor->SetActorRotation(FRotationMatrix::MakeFromXZ(Forward.GetSafeNormal(),Up).Rotator());
}
template<class PickupActor,std::size_t Count>
void PresentPickups(UWorld& World,const Delivery::Simulation& Ride,
    const std::array<Delivery::Pickup,Count>& Pickups,TArray<PickupActor*>& Actors,double Spin)
{
    for(int32 Index=0;Index<Actors.Num();++Index)
    {
        const auto& Pickup=Pickups[Index];
        auto*& Actor=Actors[Index];
        if(!Pickup.available)
        {
            if(Actor&&!Actor->IsHidden()) Actor->SetActorHiddenInGame(true);
            continue;
        }
        const double X=Pickup.position.x*100,Y=Pickup.position.y*100;
        if(!Actor||Actor->GetActorLocation().X!=X||Actor->GetActorLocation().Y!=Y)
        {
            const FVector Location(X,Y,Ride.layout->height(Pickup.position.x,Pickup.position.y)*100);
            if(Actor) Actor->SetActorLocation(Location);
            else Actor=World.SpawnActor<PickupActor>(Location,FRotator::ZeroRotator);
        }
        if(Actor)
        {
            if(Actor->IsHidden()) Actor->SetActorHiddenInGame(false);
            Actor->SetActorRotation(FRotator(0,Ride.elapsed*Spin+Index*45,0));
        }
    }
}
}

ADeliveryGoalMarker::ADeliveryGoalMarker()
{
    auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("GoalRoot"));
    SetRootComponent(Root);
    Part(this,Root,TEXT("Pad"),TEXT("Cylinder"),FVector(0,0,18),FVector(1100,1100,12),TEXT("GlowTeal"))->SetCastShadow(false);
    Part(this,Root,TEXT("Beacon"),TEXT("Cylinder"),FVector(0,0,650),FVector(38,38,1300),TEXT("GlowTeal"))->SetCastShadow(false);
    Part(this,Root,TEXT("Lamp"),TEXT("Sphere"),FVector(0,0,1300),FVector(135),TEXT("GlowTeal"))->SetCastShadow(false);
}

ADeliveryTimePickup::ADeliveryTimePickup()
{
    auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("ClockRoot"));
    SetRootComponent(Root);
    const FRotator FaceRotation(90,0,0);
    Part(this,Root,TEXT("GoldRim"),TEXT("Cylinder"),FVector(0,0,145),FVector(150,150,24),TEXT("GlowAmber"),FaceRotation)->SetCastShadow(false);
    for(int32 Side:{-1,1})
    {
        const FString Face=Side<0?TEXT("Front"):TEXT("Back");
        Part(this,Root,Face+TEXT("Dial"),TEXT("Cylinder"),FVector(Side*13,0,145),FVector(121,121,5),TEXT("Ink"),FaceRotation)->SetCastShadow(false);
        Part(this,Root,Face+TEXT("MinuteHand"),TEXT("Cube"),FVector(Side*17,0,163),FVector(5,8,42),TEXT("GlowAmber"))->SetCastShadow(false);
        Part(this,Root,Face+TEXT("HourHand"),TEXT("Cube"),FVector(Side*17,Side*14,145),FVector(5,30,8),TEXT("GlowAmber"))->SetCastShadow(false);
    }
}

ADeliveryFreezePickup::ADeliveryFreezePickup()
{
    auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("SnowflakeRoot"));
    SetRootComponent(Root);
    const FVector Center(0,0,145);
    Part(this,Root,TEXT("IceCore"),TEXT("Sphere"),Center,FVector(42),TEXT("GlowTeal"))->SetCastShadow(false);
    for(int32 Arm=0;Arm<6;++Arm)
    {
        const double Angle=Arm*PI/3;
        const FVector Ray(0,FMath::Cos(Angle),FMath::Sin(Angle)),Side(0,-Ray.Z,Ray.Y);
        Tube(this,Root,FString::Printf(TEXT("Crystal%d"),Arm),Center,Center+Ray*91,13,TEXT("GlowTeal"))->SetCastShadow(false);
        for(int32 Fork:{-1,1})
            Tube(this,Root,FString::Printf(TEXT("Fork%d_%d"),Arm,Fork+1),Center+Ray*67,
                Center+Ray*49+Side*(Fork*25),10,TEXT("Ivory"))->SetCastShadow(false);
    }
}

ADeliveryPedestrian::ADeliveryPedestrian()
{
    auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("PedestrianRoot"));
    SetRootComponent(Root);
    SetActorEnableCollision(false);
    Shirt=Part(this,Root,TEXT("Shirt"),TEXT("Cube"),FVector(0,0,115),FVector(27,39,48),TEXT("Coral"));
    Part(this,Root,TEXT("Head"),TEXT("Sphere"),FVector(1,0,155),FVector(25,25,29),TEXT("Skin"));
    Part(this,Root,TEXT("Hair"),TEXT("Sphere"),FVector(-3,0,165),FVector(23,25,15),TEXT("Ink"));
    for(int32 Index=0;Index<2;++Index)
    {
        const double Side=Index==0?-1.:1.;
        Arms[Index]=Tube(this,Root,FString::Printf(TEXT("WalkArm%d"),Index),FVector(0,Side*22,133),FVector(0,Side*24,87),10,TEXT("Skin"));
        Legs[Index]=Tube(this,Root,FString::Printf(TEXT("WalkLeg%d"),Index),FVector(0,Side*11,93),FVector(0,Side*12,7),13,TEXT("Navy"));
    }
}
void ADeliveryPedestrian::SetWalk(const Delivery::WalkRoute& Walk,float Speed,float StartProgress,int32 Outfit)
{
    Route=Walk;
    WalkSpeed=Speed;
    const auto Delta=Route.end-Route.start;
    InitialDistance=std::hypot(Delta.x,Delta.y)*StartProgress;
    const TCHAR* Colors[]={TEXT("Coral"),TEXT("Teal"),TEXT("Amber"),TEXT("Sage"),TEXT("Ivory")};
    const FString MaterialPath=FString::Printf(TEXT("/Game/Art/M_%s.M_%s"),Colors[Outfit],Colors[Outfit]);
    if(auto* Material=LoadObject<UMaterialInterface>(nullptr,*MaterialPath)) Shirt->SetMaterial(0,Material);
}
void ADeliveryPedestrian::Present(double Elapsed,const Delivery::LevelLayout& Level)
{
    const auto Delta=Route.end-Route.start;
    const double Length=std::hypot(Delta.x,Delta.y);
    const double Distance=InitialDistance+Elapsed*WalkSpeed;
    const double Cycle=std::fmod(Distance,2*Length);
    const bool Returning=Cycle>=Length;
    const auto Position=Route.start+Delta*((Returning?2*Length-Cycle:Cycle)/Length);
    const double Yaw=FMath::RadiansToDegrees(std::atan2(Delta.y,Delta.x))+(Returning?180:0);
    SetActorLocationAndRotation(FVector(Position.x*100,Position.y*100,Level.height(Position.x,Position.y)*100),FRotator(0,Yaw,0));
    const double Stride=std::sin(Distance*5.5);
    for(int32 Index=0;Index<2;++Index)
    {
        const double Side=Index==0?-1.:1., Swing=Side*Stride;
        Between(Legs[Index],FVector(0,Side*11,93),FVector(Swing*29,Side*12,7+FMath::Max(0.,Swing)*14),13);
        Between(Arms[Index],FVector(0,Side*22,133),FVector(-Swing*18,Side*24,87+FMath::Abs(Swing)*4),10);
    }
}

ADeliveryBike::ADeliveryBike()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Body = CreateDefaultSubobject<USceneComponent>(TEXT("BicycleAndCourier"));
    Body->SetupAttachment(RootComponent);
    FrontWheel = CreateDefaultSubobject<USceneComponent>(TEXT("FrontWheel"));
    RearWheel = CreateDefaultSubobject<USceneComponent>(TEXT("RearWheel"));
    FrontWheel->SetupAttachment(Body); RearWheel->SetupAttachment(Body);
    FrontWheel->SetRelativeLocation(FVector(65, 0, 36));
    RearWheel->SetRelativeLocation(FVector(-65, 0, 36));
    for (int32 Index = 0; Index < 2; ++Index)
    {
        auto* Wheel = Index == 0 ? FrontWheel : RearWheel;
        const FString Prefix = Index == 0 ? TEXT("Front") : TEXT("Rear");
        Part(this, Wheel, Prefix + TEXT("Tire"), TEXT("Cylinder"), FVector::ZeroVector,
            FVector(70, 70, 7), TEXT("Ink"), FRotator(0, 0, 90));
        Part(this, Wheel, Prefix + TEXT("Rim"), TEXT("Cylinder"), FVector::ZeroVector,
            FVector(55, 55, 7.5), TEXT("Silver"), FRotator(0, 0, 90));
        Part(this, Wheel, Prefix + TEXT("Hub"), TEXT("Cylinder"), FVector::ZeroVector,
            FVector(13, 13, 9), TEXT("Ink"), FRotator(0, 0, 90));
        Tube(this, Wheel, Prefix + TEXT("SpokeA"), FVector(-26, -4, 0), FVector(26, -4, 0), 3, TEXT("Ink"));
        Tube(this, Wheel, Prefix + TEXT("SpokeB"), FVector(0, -4, -26), FVector(0, -4, 26), 3, TEXT("Ink"));
    }
    const FVector Rear(-65, 0, 36), Front(65, 0, 36), Crank(0, 0, 36), Seat(-28, 0, 91), Stem(44, 0, 94);
    Tube(this, Body, TEXT("FrameRearUpper"), Rear, Seat, 5, TEXT("Coral"));
    Tube(this, Body, TEXT("FrameRearLower"), Rear, Crank, 5, TEXT("Coral"));
    Tube(this, Body, TEXT("FrameSeat"), Crank, Seat, 5, TEXT("Coral"));
    Tube(this, Body, TEXT("FrameTop"), Seat, Stem, 5, TEXT("Coral"));
    Tube(this, Body, TEXT("FrameDiagonal"), Crank, Stem, 5, TEXT("Coral"));
    Tube(this, Body, TEXT("Fork"), Stem, Front, 5, TEXT("Silver"));
    Tube(this, Body, TEXT("HandlebarStem"), Stem, FVector(42, 0, 114), 4, TEXT("Silver"));
    Tube(this, Body, TEXT("Handlebar"), FVector(42, -30, 114), FVector(42, 30, 114), 5, TEXT("Ink"));
    Part(this, Body, TEXT("Saddle"), TEXT("Cube"), FVector(-29, 0, 94), FVector(30, 20, 7), TEXT("Ink"));
    Part(this, Body, TEXT("Parcel"), TEXT("Cube"), FVector(-63, 0, 105), FVector(38, 40, 38), TEXT("Amber"));
    Part(this, Body, TEXT("ParcelStrap"), TEXT("Cube"), FVector(-63, 0, 125), FVector(39, 9, 2), TEXT("Ivory"));
    Part(this, Body, TEXT("Jacket"), TEXT("Cube"), FVector(-1, 0, 132), FVector(34, 35, 48), TEXT("Teal"), FRotator(20, 0, 0));
    Part(this, Body, TEXT("Head"), TEXT("Sphere"), FVector(15, 0, 168), FVector(25, 25, 28), TEXT("Skin"));
    Part(this, Body, TEXT("Helmet"), TEXT("Sphere"), FVector(14, 0, 178), FVector(30, 29, 17), TEXT("Ivory"));
    Tube(this, Body, TEXT("LeftArm"), FVector(5, -18, 143), FVector(42, -28, 115), 10, TEXT("Teal"));
    Tube(this, Body, TEXT("RightArm"), FVector(5, 18, 143), FVector(42, 28, 115), 10, TEXT("Teal"));
    for (int32 Index = 0; Index < 4; ++Index)
        Legs[Index] = Tube(this, Body, FString::Printf(TEXT("Leg%d"), Index), FVector(0, 0, 90), FVector(0, 0, 50), 10, TEXT("Navy"));
    CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraArm->SetupAttachment(RootComponent);
    CameraArm->SetUsingAbsoluteRotation(true);
    CameraArm->TargetArmLength = 660;
    CameraArm->TargetOffset = FVector(0, 0, 135);
    CameraArm->SetRelativeRotation(FRotator(-16, 0, 0));
    CameraArm->bDoCollisionTest = false;
    CameraArm->bEnableCameraRotationLag = true;
    CameraArm->CameraRotationLagSpeed = 7;
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(CameraArm);
    Camera->FieldOfView = 82;
}
void ADeliveryBike::Look(float X, float Y, bool Held, float Dt)
{
    if(Held) { OrbitYaw=FMath::Clamp(OrbitYaw+X*1.4f,-160.f,160.f); OrbitPitch=FMath::Clamp(OrbitPitch+Y,-35.f,-8.f); }
    else { OrbitYaw=FMath::FInterpTo(OrbitYaw,0.f,Dt,3.f); OrbitPitch=FMath::FInterpTo(OrbitPitch,-18.f,Dt,2.f); }
}
void ADeliveryBike::Present(const Delivery::Simulation& State, float DeltaSeconds)
{
    PlaceOnSurface(this,State.bike,State.heading,.65,.25,State.layout.get());
    CameraArm->SetWorldRotation(FRotator(OrbitPitch,FMath::RadiansToDegrees(float(State.heading))+OrbitYaw,0));
    const float Lean=float(State.steering);
    Body->SetRelativeRotation(FRotator(0,0,-Lean*9+FMath::RadiansToDegrees(float(State.roll))));
    WheelAngle+=float(State.phase==Delivery::Phase::Riding?State.speed:0)*DeltaSeconds*165;
    FrontWheel->SetRelativeRotation(FRotator(WheelAngle,Lean*19,0));
    RearWheel->SetRelativeRotation(FRotator(WheelAngle,0,0));
    // The forward pedal moves down on the power stroke as the bike advances.
    const float Angle=FMath::DegreesToRadians(-WheelAngle*.45f);
    for(int32 Side=0;Side<2;++Side)
    {
        const float Sign=Side==0?-1.f:1.f, Pedal=Angle+Side*PI;
        FVector Hip(-22,Sign*14,109);
        FVector Knee(4+FMath::Cos(Pedal)*13,Sign*17,77+FMath::Sin(Pedal)*12);
        FVector Foot(FMath::Cos(Pedal)*18,Sign*17,37+FMath::Sin(Pedal)*18);
        Between(Legs[Side*2],Hip,Knee,12); Between(Legs[Side*2+1],Knee,Foot,10);
    }
    const float Fov=State.phase==Delivery::Phase::Riding?80+float(FMath::Abs(State.speed))*.35f:82;
    Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView,Fov,DeltaSeconds,3));
    Camera->SetRelativeLocation(FVector(0,State.hitFlash>0?FMath::Sin(float(State.elapsed)*80)*float(State.hitFlash)*8:0,0));
}

ADeliveryCar::ADeliveryCar()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Body=CreateDefaultSubobject<USceneComponent>(TEXT("CarBody"));
    Body->SetupAttachment(RootComponent);
    Body->SetRelativeLocation(FVector(0,0,70));
    Chassis=Part(this, Body, TEXT("Chassis"), TEXT("Cube"), FVector(0, 0, -14), FVector(410, 180, 65), TEXT("Coral"));
    Part(this, Body, TEXT("Cabin"), TEXT("Cube"), FVector(-12, 0, 40), FVector(200, 160, 65), TEXT("Navy"));
    Roof=Part(this, Body, TEXT("Roof"), TEXT("Cube"), FVector(-12, 0, 76), FVector(205, 164, 10), TEXT("Coral"));
    for (int32 Side : {-1, 1})
    {
        for (int32 End : {-1, 1})
            Part(this, Body, FString::Printf(TEXT("Wheel%d_%d"), Side, End), TEXT("Cylinder"),
                FVector(End * 132, Side * 91, -37), FVector(66, 66, 18), TEXT("Ink"), FRotator(0, 0, 90));
        Part(this, Body, FString::Printf(TEXT("Headlamp%d"), Side), TEXT("Cube"),
            FVector(207, Side * 57, -6), FVector(4, 38, 16), TEXT("GlowAmber"));
        Part(this, Body, FString::Printf(TEXT("TailLamp%d"), Side), TEXT("Cube"),
            FVector(-207, Side * 62, -6), FVector(4, 30, 12), TEXT("GlowRed"));
    }
}
void ADeliveryCar::Present(const Delivery::Car& State,const Delivery::LevelLayout* Level)
{
    const bool Visible=State.phase!=Delivery::CarPhase::Waiting;
    SetActorHiddenInGame(!Visible);
    if(!Visible) return;
    PlaceOnSurface(this,State.position,State.heading,1.32,.91,Level);
    const float Roll=float(State.roll);
    Body->SetRelativeRotation(FRotator(0,0,FMath::RadiansToDegrees(Roll)));
    Body->SetRelativeLocation(FVector(0,0,FMath::Abs(FMath::Sin(Roll))*100+FMath::Abs(FMath::Cos(Roll))*70));
    if(PaintedIdentity!=State.identity)
    {
        const TCHAR* Colors[]={TEXT("Coral"),TEXT("Navy"),TEXT("Sand"),TEXT("Sage"),TEXT("Teal"),TEXT("Ivory"),TEXT("Brick"),TEXT("Silver")};
        const TCHAR* Color=Colors[FMath::Max(0,State.identity-1)%8];
        const FString Path=FString::Printf(TEXT("/Game/Art/M_%s.M_%s"),Color,Color);
        if(auto* Mat=LoadObject<UMaterialInterface>(nullptr,*Path))
        { Chassis->SetMaterial(0,Mat); Roof->SetMaterial(0,Mat); }
        PaintedIdentity=State.identity;
    }
}

ADeliveryGameMode::ADeliveryGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = ADeliveryBike::StaticClass();
    HUDClass = ADeliveryHUD::StaticClass();
}
void ADeliveryGameMode::BeginPlay()
{
    Super::BeginPlay();
    Ride.BeginCampaign(Delivery::ReadLevelLayout(GetWorld()));
    for(TActorIterator<ADeliveryLevelSettings> It(GetWorld());It;++It)
    {
        ClinicGoal=It->GoalActor;
        break;
    }
    UpdateGoalMarker();
    auto* PC = GetWorld()->GetFirstPlayerController();
    Rider = PC ? Cast<ADeliveryBike>(PC->GetPawn()) : nullptr;
    if (!Rider)
    {
        Rider = GetWorld()->SpawnActor<ADeliveryBike>();
        if (PC) PC->Possess(Rider);
    }
    if (PC)
    {
        PC->SetViewTarget(Rider);
        UpdateMouseInput(*PC);
    }
    Cars.SetNumZeroed(int32(Delivery::Simulation::CarCount));
    TimePickupActors.SetNumZeroed(int32(Delivery::Simulation::TimePickupCount));
    FreezePickupActors.SetNumZeroed(int32(Delivery::Simulation::FreezePickupCount));
    if (Rider) Rider->Present(Ride, 0);
    UE_LOG(LogTemp, Display, TEXT("DELIVERY_RUSH_READY: press Enter to ride."));
}
void ADeliveryGameMode::UpdateMouseInput(APlayerController& Controller) const
{
    Controller.bShowMouseCursor=true;
    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetHideCursorDuringCapture(true);
    Controller.SetInputMode(Mode);
    if(auto* Viewport=GetWorld()->GetGameViewport())
        Viewport->SetMouseCaptureMode(Ride.phase==Delivery::Phase::Riding
            ? EMouseCaptureMode::CaptureDuringRightMouseDown : EMouseCaptureMode::NoCapture);
}
void ADeliveryGameMode::UpdateGoalMarker()
{
    const bool AtClinic=Ride.IsClinicDelivery();
    const auto Goal=Ride.layout->goal;
    UE_LOG(LogTemp, Display, TEXT("DELIVERY_RUSH_STAGE: stage=%d order=%d goal=(%.1f,%.1f) trafficSpeed=%.1f cap=%d"),
        int32(Ride.Stage()),FMath::Min(Ride.deliveriesCompleted+1,Delivery::Simulation::DeliveryCount),Goal.x,Goal.y,
        Ride.Traffic().speed,Ride.Traffic().maxActiveCars);
    if(ClinicGoal) ClinicGoal->SetActorHiddenInGame(!AtClinic);
    if(!AtClinic&&!PracticeGoal) PracticeGoal=GetWorld()->SpawnActor<ADeliveryGoalMarker>();
    if(!PracticeGoal) return;
    PracticeGoal->SetActorHiddenInGame(AtClinic);
    if(!AtClinic)
    {
        const double Ground=Ride.layout->height(Goal.x,Goal.y);
        PracticeGoal->SetActorLocation(FVector(Goal.x*100,Goal.y*100,Ground*100));
    }
}
void ADeliveryGameMode::UpdatePickups()
{
    PresentPickups(*GetWorld(),Ride,Ride.timePickups,TimePickupActors,85);
    PresentPickups(*GetWorld(),Ride,Ride.freezePickups,FreezePickupActors,68);
}
void ADeliveryGameMode::SetUpPedestrians()
{
    if(Ride.TrafficFrozen()) { PedestrianRefreshPending=true; return; }
    PedestrianRefreshPending=false;
    for(auto* Pedestrian:Pedestrians) Pedestrian->Destroy();
    Pedestrians.Reset();
    if(Ride.Stage()!=Delivery::CampaignStage::FirstChase) return;

    const auto& Level=*Ride.layout;
    const auto Junction=Level.start+Delivery::Simulation::Forward(Level.startHeading)*90;
    FRandomStream Random(FMath::Rand());
    for(const auto& Walk:Delivery::RoadsideWalks(Level,Junction,Random.GetUnsignedInt()))
    {
        if(auto* Pedestrian=GetWorld()->SpawnActor<ADeliveryPedestrian>())
        {
            Pedestrian->SetWalk(Walk,Random.FRandRange(1.15f,1.7f),Random.FRandRange(0.f,2.f),Random.RandRange(0,4));
            Pedestrians.Add(Pedestrian);
        }
    }
    UE_LOG(LogTemp,Display,TEXT("DELIVERY_RUSH_PEDESTRIANS: count=%d"),Pedestrians.Num());
}
void ADeliveryGameMode::PlayCue(const TCHAR* Name, float Volume)
{
    const FString Path = FString::Printf(TEXT("/Game/Audio/S_%s.S_%s"), Name, Name);
    if (auto* Sound = LoadObject<USoundBase>(nullptr, *Path)) UGameplayStatics::PlaySound2D(this, Sound, Volume);
}
FString ADeliveryGameMode::GetRideStatus() const
{
    FString Status=FString::Printf(TEXT("{\"phase\":%d,\"stage\":%d,\"deliveries_completed\":%d,\"money\":%d,\"x\":%.3f,\"y\":%.3f,\"heading\":%.3f,\"speed\":%.3f,\"time\":%.3f,\"cars\":%d,\"wrecks\":%d,\"falls\":%d,\"grazes\":%d,\"waves\":%d,\"pursuers\":["),
        int32(Ride.phase),int32(Ride.Stage()),Ride.deliveriesCompleted,Ride.money,Ride.bike.x,Ride.bike.y,Ride.heading,Ride.speed,Ride.time,Ride.ActiveCars(),Ride.WreckedCars(),Ride.falls,Ride.grazes,Ride.wavesStarted);
    for(std::size_t Index=0;Index<Ride.cars.size();++Index)
    {
        const auto& Car=Ride.cars[Index];
        if(Car.wave==0) continue;
        if(!Status.EndsWith(TEXT("["))) Status+=TEXT(",");
        Status+=FString::Printf(TEXT("{\"id\":%d,\"wave\":%d,\"phase\":%d,\"mode\":%d,\"x\":%.3f,\"y\":%.3f,\"speed\":%.3f,\"passes\":%d,\"rear_attacks\":%d,\"next_in\":%.3f}"),
            Car.identity,Car.wave,int32(Car.phase),int32(Car.attackMode),Car.position.x,Car.position.y,Car.speed,Car.passes,Car.rearAttacks,Car.reattackIn);
    }
    return Status+TEXT("]}");
}
void ADeliveryGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* PC=GetWorld()->GetFirstPlayerController();
    if(!PC||!Rider) return;
    const bool WasRiding=Ride.phase==Delivery::Phase::Riding;
    if(PC->WasInputKeyJustPressed(EKeys::R)) Ride.Restart();
    if(PC->WasInputKeyJustPressed(EKeys::Enter))
    {
        if(Ride.phase==Delivery::Phase::Ready) Ride.Start();
        else if(Ride.phase==Delivery::Phase::Failed) Ride.Restart();
        else if(Ride.phase==Delivery::Phase::Delivered)
        {
            Ride.AdvanceStage();
            UpdateGoalMarker();
            SetUpPedestrians();
        }
        else if(Ride.phase==Delivery::Phase::Paused) Ride.TogglePause();
    }
    if(PC->WasInputKeyJustPressed(EKeys::Escape))
    {
        if(Ride.phase==Delivery::Phase::Riding) Ride.TogglePause();
    }
    else if(PC->WasInputKeyJustPressed(EKeys::P)) Ride.TogglePause();
    Delivery::Input Input;
    Input.throttle=(PC->IsInputKeyDown(EKeys::W)||PC->IsInputKeyDown(EKeys::Up)?1:0)
        -(PC->IsInputKeyDown(EKeys::S)||PC->IsInputKeyDown(EKeys::Down)?1:0);
    Input.steer=(PC->IsInputKeyDown(EKeys::D)||PC->IsInputKeyDown(EKeys::Right)?1:0)
        -(PC->IsInputKeyDown(EKeys::A)||PC->IsInputKeyDown(EKeys::Left)?1:0);
    Input.brake=PC->IsInputKeyDown(EKeys::SpaceBar);
    float MouseX=0,MouseY=0; PC->GetInputMouseDelta(MouseX,MouseY);
    Rider->Look(MouseX,MouseY,Ride.phase==Delivery::Phase::Riding&&PC->IsInputKeyDown(EKeys::RightMouseButton),DeltaSeconds);
    const int PreviousGrazes=Ride.grazes,PreviousFalls=Ride.falls;
    const int PreviousWrecks=Ride.WreckedCars();
    const int PreviousWaves=Ride.wavesStarted;
    const int PreviousPickups=Ride.pickupsCollected;
    const int PreviousFreezes=Ride.freezesCollected;
    const bool WasFrozen=Ride.TrafficFrozen();
    const int PreviousDeliveries=Ride.deliveriesCompleted;
    std::array<int,Delivery::Simulation::CarCount> PreviousAttacks{};
    std::array<Delivery::CarPhase,Delivery::Simulation::CarCount> PreviousCarPhases{};
    for(std::size_t Index=0;Index<Ride.cars.size();++Index)
    {
        PreviousAttacks[Index]=Ride.cars[Index].rearAttacks;
        PreviousCarPhases[Index]=Ride.cars[Index].phase;
    }
    const Delivery::Phase PreviousPhase=Ride.phase;
    if(Ride.phase==Delivery::Phase::Riding&&!FApp::HasFocus()) Ride.TogglePause();
    Ride.Tick(DeltaSeconds,Input);
    if(Ride.deliveriesCompleted>PreviousDeliveries)
    {
        PlayCue(TEXT("Delivery"),.65f);
        UpdateGoalMarker();
        SetUpPedestrians();
        UE_LOG(LogTemp,Display,TEXT("DELIVERY_RUSH_ORDER: completed=%d money=%d"),Ride.deliveriesCompleted,Ride.money);
    }
    if(WasRiding!=(Ride.phase==Delivery::Phase::Riding)) UpdateMouseInput(*PC);
    Rider->Present(Ride,DeltaSeconds);
    UpdatePickups();
    if(PedestrianRefreshPending&&!Ride.TrafficFrozen()) SetUpPedestrians();
    for(auto* Pedestrian:Pedestrians) Pedestrian->Present(Ride.trafficElapsed,*Ride.layout);
    for(int32 Index=0;Index<Cars.Num();++Index)
    {
        if(!Cars[Index]&&Ride.cars[Index].phase!=Delivery::CarPhase::Waiting)
            Cars[Index]=GetWorld()->SpawnActor<ADeliveryCar>();
        if(Cars[Index]) Cars[Index]->Present(Ride.cars[Index],Ride.layout.get());
    }
    if(Ride.wavesStarted>PreviousWaves)
        UE_LOG(LogTemp,Display,TEXT("DELIVERY_RUSH_FRONT_WAVE: wave=%d active=%d elapsed=%.2f"),
            Ride.wavesStarted,Ride.ActiveCars(),Ride.elapsed);
    if(Ride.pickupsCollected>PreviousPickups)
    {
        PlayCue(TEXT("Checkpoint"),.5f);
        UE_LOG(LogTemp,Display,TEXT("DELIVERY_RUSH_TIME_PICKUP: count=%d remaining=%.2f"),Ride.pickupsCollected,Ride.time);
    }
    if(Ride.freezesCollected>PreviousFreezes)
    {
        PlayCue(TEXT("Checkpoint"),.55f);
        UE_LOG(LogTemp,Display,TEXT("DELIVERY_RUSH_FREEZE: collected=%d remaining=%.2f"),Ride.freezesCollected,Ride.freezeRemaining);
    }
    if(WasFrozen&&!Ride.TrafficFrozen()) UE_LOG(LogTemp,Display,TEXT("DELIVERY_RUSH_THAW: traffic resumed"));
    for(std::size_t Index=0;Index<Ride.cars.size();++Index)
    {
        const auto& Car=Ride.cars[Index];
        if(Car.phase==Delivery::CarPhase::Wrecked&&PreviousCarPhases[Index]!=Delivery::CarPhase::Wrecked)
            UE_LOG(LogTemp,Display,TEXT("DELIVERY_RUSH_CAR_WRECK: car=%d cause=%s position=(%.1f,%.1f)"),
                Car.identity,Car.crashCause==Delivery::CrashCause::Building?TEXT("obstacle"):
                    Car.crashCause==Delivery::CrashCause::Car?TEXT("vehicle"):TEXT("unknown"),Car.position.x,Car.position.y);
        if(Car.rearAttacks>PreviousAttacks[Index])
            UE_LOG(LogTemp,Display,TEXT("DELIVERY_RUSH_REAR_ATTACK: car=%d attack=%d elapsed=%.2f speed=%.2f"),
                Car.identity,Car.rearAttacks,Ride.elapsed,Car.speed);
    }
    if(Ride.grazes>PreviousGrazes||Ride.falls>PreviousFalls) PlayCue(TEXT("Hit"));
    if(Ride.WreckedCars()>PreviousWrecks)
    {
        PlayCue(TEXT("Hit"),.35f);
        UE_LOG(LogTemp,Display,TEXT("DELIVERY_RUSH_ENEMY_WRECK: active=%d wrecked=%d; wreck stays out"),Ride.ActiveCars(),Ride.WreckedCars());
    }
    if(Ride.falls>PreviousFalls) UE_LOG(LogTemp,Display,TEXT("DELIVERY_RUSH_FALL: full restart follows"));
    if(PreviousPhase!=Ride.phase&&Ride.phase==Delivery::Phase::Delivered)
    {
        if(!Ride.HasDeliverySequence()) PlayCue(TEXT("Delivery"),.65f);
        UE_LOG(LogTemp,Display,TEXT("DELIVERY_RUSH_DELIVERED: stage=%d"),int32(Ride.Stage()));
    }
}

void ADeliveryHUD::Rect(float X, float Y, float W, float H, FLinearColor Color)
{ DrawRect(Color, OffsetX + X * Scale, OffsetY + Y * Scale, W * Scale, H * Scale); }
void ADeliveryHUD::Text(const FString& Value, float X, float Y, float Size, FLinearColor Color)
{ DrawText(Value, Color, OffsetX + X * Scale, OffsetY + Y * Scale, GEngine->GetMediumFont(), Size / 16.f * Scale, false); }
void ADeliveryHUD::Center(const FString& Value, float Y, float Size, FLinearColor Color)
{
    float Width = 0, Height = 0;
    GetTextSize(Value, Width, Height, GEngine->GetMediumFont(), Size / 16.f);
    Text(Value, (1280 - Width) * .5f, Y, Size, Color);
}
void ADeliveryHUD::DrawHUD()
{
    Super::DrawHUD();
    if(!Canvas||!GEngine) return;
    auto* Mode=Cast<ADeliveryGameMode>(GetWorld()->GetAuthGameMode());
    if(!Mode) return;
    const auto& S=Mode->Ride;
    const bool Tutorial=S.Stage()==Delivery::CampaignStage::Tutorial;
    const bool FinalStage=S.Stage()==Delivery::CampaignStage::CityRush;
    const bool MultipleOrders=S.HasDeliverySequence();
    Scale=FMath::Min(Canvas->SizeX/1280.f,Canvas->SizeY/720.f);
    OffsetX=(Canvas->SizeX-1280*Scale)*.5f; OffsetY=(Canvas->SizeY-720*Scale)*.5f;
    Rect(24,22,1232,78,Ink);
    Text(StageTitle(S.Stage()),43,35,23,White);
    if(MultipleOrders)
        Text(FString::Printf(TEXT("ORDER %d / %d   TRAFFIC PRESSURE %d / 3"),FMath::Min(S.deliveriesCompleted+1,Delivery::Simulation::DeliveryCount),Delivery::Simulation::DeliveryCount,FMath::Min(S.deliveriesCompleted+1,Delivery::Simulation::DeliveryCount)),43,72,14,Muted);
    else Text(FinalStage?TEXT("Choose your own route to the clinic"):TEXT("Deliver to the green beacon"),43,72,14,Muted);
    const int Seconds=FMath::CeilToInt(float(S.time));
    if(Tutorial) Center(TEXT("NO TIME LIMIT"),46,20,Teal);
    else Center(FString::Printf(TEXT("%02d:%02d"),Seconds/60,Seconds%60),35,32,S.time<20?Red:Amber);
    if(!Tutorial) Center(FString::Printf(TEXT("CASH  %d"),S.money),76,14,Amber);
    Text(FString::Printf(TEXT("%s  %d m"),S.IsClinicDelivery()?TEXT("CLINIC"):TEXT("DROP"),FMath::CeilToInt(float(S.DistanceToGoal()))),1040,38,22,Teal);
    Text(FString::Printf(TEXT("ATTEMPT %d"),S.attempt),1040,73,13,Muted);
    Rect(24,658,1232,42,Ink);
    Text(TEXT("W pedal  S brake/reverse  A/D turn  SPACE brake  RMB look  ESC pause  R restart"),40,673,13,Muted);
    Text(FString::Printf(TEXT("%02d km/h"),FMath::RoundToInt(float(FMath::Abs(S.speed)*3.6))),1121,669,19,Teal);

    // A navigation map only: no enemy markers, attack strips or warning cues.
    Rect(1030,427,222,211,Ink);
    constexpr float MapX=1041,MapY=440,MapSize=198;
    auto MapPoint=[&S](double X,double Y){ const double L=S.layout->limit; return FVector2D(MapX+float((Y+L)/(2*L))*MapSize,MapY+MapSize-float((X+L)/(2*L))*MapSize); };
    const FVector2D Split=MapPoint(Delivery::ParkMinX,0);
    Rect(MapX,MapY,MapSize,Split.Y-MapY,FLinearColor(.12f,.27f,.09f));
    for(const auto& Path:S.layout->paths)
    {
        const auto A=MapPoint(Path.ax,Path.ay), B=MapPoint(Path.bx,Path.by);
        DrawLine(OffsetX+A.X*Scale,OffsetY+A.Y*Scale,OffsetX+B.X*Scale,OffsetY+B.Y*Scale,
            FLinearColor(.55f,.5f,.4f),2*Scale);
    }
    for(const auto& Obstacle:S.layout->obstacles)
    {
        FVector2D Points[4];int Index=0;
        for(const auto& Corner:{Delivery::Vec2{-1,-1},Delivery::Vec2{1,-1},Delivery::Vec2{1,1},Delivery::Vec2{-1,1}})
        {
            const double X=Corner.x*Obstacle.hx,Y=Corner.y*Obstacle.hy;
            Points[Index++]=MapPoint(Obstacle.x+FMath::Cos(Obstacle.yaw)*X-FMath::Sin(Obstacle.yaw)*Y,
                Obstacle.y+FMath::Sin(Obstacle.yaw)*X+FMath::Cos(Obstacle.yaw)*Y);
        }
        for(int I=0;I<4;++I) DrawLine(OffsetX+Points[I].X*Scale,OffsetY+Points[I].Y*Scale,
            OffsetX+Points[(I+1)%4].X*Scale,OffsetY+Points[(I+1)%4].Y*Scale,Muted*.5f,Scale);
    }
    FVector2D Goal=MapPoint(S.layout->goal.x,S.layout->goal.y);
    Rect(Goal.X-4,Goal.Y-4,8,8,Teal);
    for(const auto& Pickup:S.timePickups)
    {
        if(!Pickup.available) continue;
        const FVector2D Point=MapPoint(Pickup.position.x,Pickup.position.y);
        Rect(Point.X-2.5f,Point.Y-2.5f,5,5,Amber);
    }
    for(const auto& Pickup:S.freezePickups)
    {
        if(!Pickup.available) continue;
        const FVector2D Point=MapPoint(Pickup.position.x,Pickup.position.y);
        Rect(Point.X-4,Point.Y-1,8,2,Ice);
        Rect(Point.X-1,Point.Y-4,2,8,Ice);
    }
    FVector2D Player=MapPoint(S.bike.x,S.bike.y);
    Rect(Player.X-3,Player.Y-3,6,6,White);
    const FVector2D Tip=Player+FVector2D(float(FMath::Sin(S.heading))*10,-float(FMath::Cos(S.heading))*10);
    DrawLine(OffsetX+Player.X*Scale,OffsetY+Player.Y*Scale,OffsetX+Tip.X*Scale,OffsetY+Tip.Y*Scale,White,2*Scale);
    if(S.phase==Delivery::Phase::Riding)
    {
        if(S.TrafficFrozen())
        {
            Rect(466,111,348,42,Ink);
            Center(FString::Printf(TEXT("TRAFFIC FROZEN   %.1f s"),S.freezeRemaining),122,21,Ice);
        }
        if(Tutorial)
        {
            Rect(290,572,700,47,Ink);
            Center(TEXT("W pedal    A / D steer    SPACE brake    Aim for the green beacon"),585,17,White);
        }
        else if(S.deliveryFlash>0)
        {
            Center(FString::Printf(TEXT("ORDER %d DELIVERED   +%d CASH"),S.deliveriesCompleted,Delivery::Simulation::DeliveryReward),540,23,Amber);
            Center(TEXT("Next destination marked in green. Traffic pressure increased."),575,17,White);
        }
        else if(S.timePickupFlash>0)
            Center(FString::Printf(TEXT("+%d SECONDS"),Delivery::Simulation::TimePickupBonus),565,23,Amber);
        else if(S.restartFlash>0) Center(TEXT("NEW ATTEMPT"),565,20,White);
        if(S.hitFlash>0) DrawRect(FLinearColor(1,.5f,.2f,float(S.hitFlash)*.15f),0,0,Canvas->SizeX,Canvas->SizeY);
        return;
    }
    if(S.phase==Delivery::Phase::Fallen)
    {
        Rect(390,278,500,106,Ink); Center(TEXT("FELL OFF THE BIKE"),300,30,White);
        Center(MultipleOrders?TEXT("Retrying from the last delivery checkpoint..."):TEXT("Restarting from the beginning..."),347,18,Muted); return;
    }
    Rect(250,219,780,326,FLinearColor(.02f,.035f,.055f,.96f));
    if(S.phase==Delivery::Phase::Ready)
    {
        Center(StageTitle(S.Stage()),256,27,White);
        if(Tutorial)
        {
            Center(TEXT("Practice pedaling, steering and braking at your own pace."),310,18,Muted);
            Center(TEXT("W pedals, A / D steer while moving, SPACE brakes."),348,18,Muted);
            Center(TEXT("Reach the green beacon to make your first delivery."),386,18,Muted);
        }
        else if(FinalStage)
        {
            Center(FString::Printf(TEXT("3 deliveries, %.0f seconds each. Finish at the clinic."),S.layout->timeLimit),310,18,White);
            Center(TEXT("Faster cars and more front / rear traffic with every order."),348,18,Muted);
            Center(TEXT("Earn 100 cash per order. Retry from the last drop."),386,17,Muted);
            Center(TEXT("Alley coins add 10 seconds. Blue snowflakes freeze traffic for 5 seconds."),420,16,Ice);
        }
        else
        {
            Center(FString::Printf(TEXT("Complete 3 deliveries. Each on-time order earns %d cash."),Delivery::Simulation::DeliveryReward),305,17,White);
            Center(TEXT("2-3 cars aim at you from ahead or diagonally. Swerve to dodge."),348,18,Muted);
            Center(TEXT("Gold clocks add 10 seconds. Blue snowflakes freeze traffic for 5 seconds."),386,16,Ice);
            Center(TEXT("90 seconds each. Traffic grows with every drop. Checkpoint retries."),420,16,Muted);
        }
        Center(TEXT("PRESS ENTER TO RIDE"),461,24,Teal);
    }
    else if(S.phase==Delivery::Phase::Paused)
    {
        Center(TEXT("PAUSED"),285,38,White); Center(TEXT("ENTER / P TO CONTINUE"),440,23,Teal);
    }
    else if(S.phase==Delivery::Phase::Delivered)
    {
        Center(FinalStage?TEXT("ALL 3 STAGES COMPLETE"):(MultipleOrders?TEXT("THREE DELIVERIES COMPLETE"):TEXT("PARCEL DELIVERED")),274,35,Teal);
        Center(Tutorial?TEXT("Riding lesson complete"):FString::Printf(TEXT("%.1f seconds left / %d falls / %d brushes"),S.time,S.falls,S.grazes),351,19,White);
        if(MultipleOrders) Center(FString::Printf(TEXT("3 / 3 delivered   CASH %d"),S.money),394,22,Amber);
        Center(FinalStage?TEXT("ENTER TO START A NEW CAMPAIGN"):TEXT("ENTER FOR THE NEXT STAGE"),449,23,Teal);
    }
    else
    {
        Center(TEXT("DELIVERY TOO LATE"),280,34,Red);
        Center(MultipleOrders?TEXT("ENTER / R TO RETRY THIS ORDER"):TEXT("ENTER / R TO RETRY THIS STAGE"),439,20,Teal);
    }
}
