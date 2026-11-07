#if WITH_DEV_AUTOMATION_TESTS
#include "DeliveryActors.h"
#include "DeliveryTerrain.h"
#include "ProceduralMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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
    auto* Chassis=FindObject<UStaticMeshComponent>(Car,TEXT("Chassis"));
    auto* Front=FindObject<UStaticMeshComponent>(Car,TEXT("FrontLeftWheel"));
    auto* Rear=FindObject<UStaticMeshComponent>(Car,TEXT("RearLeftWheel"));
    if(TestTrue(TEXT("The compact has an imported chassis and separate wheels"),Chassis&&Chassis->GetStaticMesh()&&
        Chassis->GetStaticMesh()->GetFName()==TEXT("SM_CompactBody")&&Front&&Front->GetStaticMesh()&&Rear&&Rear->GetStaticMesh()))
    {
        const auto CheckTireContact=[&](const UStaticMeshComponent* Wheel)
        {
            const double AxleHeight=Car->GetActorTransform().InverseTransformPosition(Wheel->GetComponentLocation()).Z;
            return FMath::IsNearlyEqual(AxleHeight,Wheel->GetStaticMesh()->GetBounds().BoxExtent.Z,.05);
        };
        TestTrue(TEXT("The compact sits on its tires"),CheckTireContact(Front)&&CheckTireContact(Rear));
        TestTrue(TEXT("The car retains a separate paint material slot"),Chassis->GetMaterialIndex(TEXT("Paint"))!=INDEX_NONE);
        C.position.y+=.25;
        C.heading+=.05;
        Car->Present(C);
        const FRotator TurningWheel=Front->GetRelativeRotation();
        TestTrue(TEXT("The front wheel rolls forward and steers"),TurningWheel.Pitch<0&&TurningWheel.Yaw>0);
        TestTrue(TEXT("The rear wheel rolls without steering"),Rear->GetRelativeRotation().Pitch<0&&FMath::IsNearlyZero(Rear->GetRelativeRotation().Yaw));
        Car->Present(C);
        TestTrue(TEXT("A stationary or frozen car keeps its wheel pose"),TurningWheel.Equals(Front->GetRelativeRotation()));
        C.identity=2;
        Car->Present(C);
        TestTrue(TEXT("A second identity uses the sports car body and wheels"),
            Chassis->GetStaticMesh()&&Chassis->GetStaticMesh()->GetFName()==TEXT("SM_SportBody")&&
            Front->GetStaticMesh()&&Front->GetStaticMesh()->GetFName()==TEXT("SM_SportWheel")&&CheckTireContact(Front));
        C.phase=Delivery::CarPhase::Wrecked;
        C.roll=PI;
        Car->Present(C);
        const auto* Body=Chassis->GetAttachParent();
        TestTrue(TEXT("The wreck rolls the new chassis and attached wheels together"),
            Front->GetAttachParent()==Body&&Body->GetRelativeRotation().RotateVector(FVector::UpVector).Z<-.99);
        TestTrue(TEXT("The upside-down sports car rests at roof height"),Chassis->GetStaticMesh()&&
            FMath::IsNearlyEqual(Body->GetRelativeLocation().Z,Chassis->GetStaticMesh()->GetBoundingBox().Max.Z,.05));
    }
    TArray<USceneComponent*> Components;Bike->GetComponents(Components);
    int Wheels=0;
    for(auto* Component:Components) if(Component->GetFName()==TEXT("FrontWheel")||Component->GetFName()==TEXT("RearWheel"))
    {
        const FVector P=Component->GetComponentLocation();++Wheels;
        TestTrue(TEXT("Bicycle wheel stays near the terrain"),FMath::Abs(P.Z-35-Delivery::GroundHeight(P.X/100,P.Y/100)*100)<8);
    }
    TestEqual(TEXT("Both bicycle wheels were checked"),Wheels,2);
    World->DestroyActor(Bike);World->DestroyActor(Car);
    return !HasAnyErrors();
}
#endif
