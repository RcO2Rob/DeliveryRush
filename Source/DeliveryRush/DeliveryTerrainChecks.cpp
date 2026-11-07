#if WITH_DEV_AUTOMATION_TESTS
#include "DeliveryActors.h"
#include "DeliveryTerrain.h"
#include "ProceduralMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "CollisionQueryParams.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeliveryGroundingCheck,"DeliveryRush.Terrain.VehicleGrounding",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeliveryGroundingCheck::RunTest(const FString& Parameters)
{
    UWorld* World=GWorld;
    if(!TestNotNull(TEXT("A loaded terrain world is required"),World)) return false;
    ADeliveryTerrain* Terrain=nullptr;
    TActorIterator<ADeliveryTerrain> It(World);
    if(It) Terrain=*It;
    if(!TestNotNull(TEXT("The hills map contains native terrain"),Terrain)) return false;
    auto* Baked=Terrain->FindComponentByClass<UStaticMeshComponent>();
    TestTrue(TEXT("The game terrain has a baked static visual mesh"),Baked&&Baked->GetStaticMesh()!=nullptr);
    auto* Surface=Terrain->FindComponentByClass<UProceduralMeshComponent>();
    FHitResult Hit;
    const FVector Start(13000,3500,5000),End(13000,3500,-1000);
    const bool HitGround=Surface&&Surface->LineTraceComponent(Hit,Start,End,FCollisionQueryParams(SCENE_QUERY_STAT(DeliveryGrounding),true));
    TestTrue(TEXT("The real native terrain collider can be traced"),HitGround);
    if(HitGround) TestTrue(TEXT("Terrain collision height matches the shared field"),
        FMath::Abs(Hit.ImpactPoint.Z-Delivery::GroundHeight(130,35)*100)<.2);
    auto* Bike=World->SpawnActor<ADeliveryBike>();auto* Car=World->SpawnActor<ADeliveryCar>();
    if(!TestNotNull(TEXT("Native bicycle spawns"),Bike)||!TestNotNull(TEXT("Native car spawns"),Car)) return false;
    Delivery::Simulation State;State.bike={130,35};State.heading=PI/2;
    Bike->Present(State,0);
    Delivery::Car C;C.phase=Delivery::CarPhase::Pursuing;C.position={135,40};C.heading=PI/2;C.identity=1;
    Car->Present(C);
    TestTrue(TEXT("Bicycle follows real slope elevation"),FMath::Abs(Bike->GetActorLocation().Z-
        Delivery::SurfacePose(130,35,PI/2,.65,.25).height*100)<.1);
    TestTrue(TEXT("Bicycle visibly pitches uphill"),Bike->GetActorForwardVector().Z>.1);
    TestTrue(TEXT("Pursuing car follows slope elevation"),FMath::Abs(Car->GetActorLocation().Z-
        Delivery::SurfacePose(135,40,PI/2,1.32,.91).height*100)<.1);
    TestTrue(TEXT("Pursuing car visibly pitches uphill"),Car->GetActorForwardVector().Z>.1);
    TArray<USceneComponent*> Components;Bike->GetComponents(Components);
    int Wheels=0;
    for(auto* Component:Components) if(Component->GetFName()==TEXT("FrontWheel")||Component->GetFName()==TEXT("RearWheel"))
    {
        const FVector P=Component->GetComponentLocation();++Wheels;
        TestTrue(TEXT("Bicycle wheel stays near the terrain"),FMath::Abs(P.Z-35-Delivery::GroundHeight(P.X/100,P.Y/100)*100)<8);
    }
    TestEqual(TEXT("Both bicycle wheels were checked"),Wheels,2);
    USceneComponent* LowerLeg=nullptr;
    for(auto* Component:Components)
        if(Component->GetFName()==TEXT("Leg1")) LowerLeg=Component;
    if(TestNotNull(TEXT("The forward leg has a lower segment"),LowerLeg))
    {
        const double Before=LowerLeg->GetRelativeLocation().Z;
        State.phase=Delivery::Phase::Riding;
        State.speed=8;
        Bike->Present(State,.1f);
        TestTrue(TEXT("The forward leg pushes the pedal down as the bicycle advances"),
            LowerLeg->GetRelativeLocation().Z<Before);
    }
    World->DestroyActor(Bike);World->DestroyActor(Car);
    return !HasAnyErrors();
}
#endif
