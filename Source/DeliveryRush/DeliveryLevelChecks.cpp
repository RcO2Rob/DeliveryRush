#if WITH_DEV_AUTOMATION_TESTS
#include "DeliveryLevel.h"
#include "DeliveryTerrain.h"
#include "DeliveryActors.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include <algorithm>
#include <cmath>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeliveryLevelEditCheck,"DeliveryRush.Editor.LevelEdits",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeliveryLevelEditCheck::RunTest(const FString& Parameters)
{
    UWorld* World=GWorld;
    if(!TestNotNull(TEXT("A loaded editable world is required"),World)) return false;
    TActorIterator<ADeliveryLevelSettings> SettingsIt(World);
    TActorIterator<ADeliveryTerrain> TerrainIt(World);
    TActorIterator<ADeliveryHill> HillIt(World);
    TActorIterator<ADeliveryRoad> RoadIt(World);
    if(!TestTrue(TEXT("Editable map has settings, terrain, hills and spline roads"),bool(SettingsIt)&&bool(TerrainIt)&&bool(HillIt)&&bool(RoadIt))) return false;
    auto* Settings=*SettingsIt;auto* Terrain=*TerrainIt;auto* Hill=*HillIt;auto* Road=*RoadIt;
    auto Baseline=Delivery::ReadLevelLayout(World);
    Delivery::Simulation Campaign;
    Campaign.BeginCampaign(Baseline);
    TestTrue(TEXT("The live map provides a short, clear teaching route"),
        Campaign.DistanceToGoal()<=75.1&&Campaign.DistanceToGoal()>6&&
        Campaign.RouteClear(Campaign.bike,Campaign.layout->goal,2));
    Campaign.Start();
    Campaign.bike=Campaign.layout->goal;
    Campaign.Tick(.01,{0,0,true});
    Campaign.AdvanceStage();
    const auto Junction=Baseline->start+Delivery::Simulation::Forward(Baseline->startHeading)*90;
    TestTrue(TEXT("The first chase has a nearby delivery accessible by a turn on the live map"),
        Campaign.Stage()==Delivery::CampaignStage::FirstChase&&Campaign.DistanceToGoal()<130&&
        Campaign.RouteClear(Baseline->start,Junction,2)&&Campaign.RouteClear(Junction,Campaign.layout->goal,2));
    int Clocks=0;
    for(const auto& Pickup:Campaign.timePickups)
    {
        if(!Pickup.available) continue;
        ++Clocks;
        TestFalse(TEXT("The clock has free space on the live map"),Campaign.IsBlocked(Pickup.position,1.5));
    }
    TestEqual(TEXT("The live map has one time pickup per stage-two order"),Clocks,1);
    const auto CheckFreezePickup=[this](const Delivery::Simulation& Ride)
    {
        int Snowflakes=0;
        for(const auto& Snowflake:Ride.freezePickups)
        {
            if(!Snowflake.available) continue;
            ++Snowflakes;
            TestFalse(TEXT("The snowflake has free space on the live map"),Ride.IsBlocked(Snowflake.position,1.5));
            for(const auto& Clock:Ride.timePickups)
                if(Clock.available) TestTrue(TEXT("Snowflakes and clocks have separate locations"),
                    std::hypot(Clock.position.x-Snowflake.position.x,Clock.position.y-Snowflake.position.y)>=6);
        }
        TestEqual(TEXT("The live map has one freeze pickup per order"),Snowflakes,1);
    };
    if(auto* Snowflake=World->SpawnActor<ADeliveryFreezePickup>();
        TestNotNull(TEXT("The snowflake pickup can be displayed on the live map"),Snowflake))
    {
        TInlineComponentArray<UStaticMeshComponent*> Crystals;
        Snowflake->GetComponents(Crystals);
        TestTrue(TEXT("The snowflake has visible mesh geometry"),Crystals.Num()>0&&Crystals[0]->GetStaticMesh()!=nullptr);
        Snowflake->Destroy();
    }
    Campaign.Start();
    auto MovingTraffic=Campaign;
    MovingTraffic.freezePickups={};
    while(MovingTraffic.wavesStarted==0&&MovingTraffic.elapsed<4) MovingTraffic.Tick(1.0/120,{1,0,false});
    TestTrue(TEXT("The live map spawns a complete random group"),MovingTraffic.ActiveCars()==2||MovingTraffic.ActiveCars()==3);
    TestEqual(TEXT("The first oncoming car starts at the new traffic speed"),MovingTraffic.cars[0].speed,Delivery::Simulation::OncomingSpeed);
    for(const auto& Car:MovingTraffic.cars)
    {
        if(Car.phase!=Delivery::CarPhase::Pursuing) continue;
        const auto Offset=Car.position-MovingTraffic.bike;
        const auto Forward=Delivery::Simulation::Forward(MovingTraffic.heading);
        const double Range=std::hypot(Offset.x,Offset.y);
        TestTrue(TEXT("A live-map spawn lies inside the forward ninety-degree cone"),
            (Offset.x*Forward.x+Offset.y*Forward.y)/Range>=FMath::Cos(PI/4)-.01);
        TestTrue(TEXT("A live-map entry has a clear approach to the rider"),MovingTraffic.RouteClear(Car.position,MovingTraffic.bike,1.2));
    }
    const auto Walks=Delivery::RoadsideWalks(*Campaign.layout,Junction,27);
    TestTrue(TEXT("The live streets have several pedestrian routes"),Walks.size()>=8);
    for(const auto& Walk:Walks)
        TestTrue(TEXT("A sidewalk route clears solid obstacles"),Campaign.RouteClear(Walk.start,Walk.end,.65));
    if(!Walks.empty())
    {
        auto* Pedestrian=World->SpawnActor<ADeliveryPedestrian>();
        if(TestNotNull(TEXT("A pedestrian can spawn on the live map"),Pedestrian))
        {
            Pedestrian->SetWalk(Walks.front(),1.4f,0,0);
            Pedestrian->Present(0,*Campaign.layout);
            const FVector StartPosition=Pedestrian->GetActorLocation();
            Pedestrian->Present(1,*Campaign.layout);
            TestTrue(TEXT("The pedestrian walks along the route"),FVector::Dist2D(StartPosition,Pedestrian->GetActorLocation())>100);
            TestFalse(TEXT("The pedestrian does not block the bicycle"),Pedestrian->GetActorEnableCollision());
            auto IceRide=Campaign;
            IceRide.bike=IceRide.freezePickups[0].position;IceRide.speed=0;
            Pedestrian->Present(IceRide.trafficElapsed,*IceRide.layout);
            const FTransform FrozenPose=Pedestrian->GetActorTransform();
            IceRide.Tick(1./120,{0,0,true});
            IceRide.Tick(4.9,{0,0,true});
            Pedestrian->Present(IceRide.trafficElapsed,*IceRide.layout);
            TestTrue(TEXT("An active snowflake holds the pedestrian's position and facing"),
                IceRide.TrafficFrozen()&&Pedestrian->GetActorTransform().Equals(FrozenPose));
            IceRide.Tick(.2,{0,0,true});
            Pedestrian->Present(IceRide.trafficElapsed,*IceRide.layout);
            TestTrue(TEXT("The pedestrian resumes walking without jumping ahead after thawing"),
                !IceRide.TrafficFrozen()&&FVector::Dist2D(Pedestrian->GetActorLocation(),FrozenPose.GetLocation())>5&&
                FVector::Dist2D(Pedestrian->GetActorLocation(),FrozenPose.GetLocation())<20);
            Pedestrian->Destroy();
        }
    }
    for(int Order=0;Order<Delivery::Simulation::DeliveryCount;++Order)
    {
        CheckFreezePickup(Campaign);
        TestTrue(TEXT("Each stage-two order has only one clock"),Campaign.timePickups[0].available&&!Campaign.timePickups[1].available);
        Campaign.bike=Campaign.layout->goal;
        Campaign.speed=0;
        Campaign.Tick(.01,{0,0,true});
        TestEqual(TEXT("Each live-map delivery pays once"),Campaign.money,(Order+1)*Delivery::Simulation::DeliveryReward);
        TestEqual(TEXT("Each live-map delivery advances the order counter"),Campaign.deliveriesCompleted,Order+1);
        if(Order+1<Delivery::Simulation::DeliveryCount)
        {
            const auto Corner=Campaign.layout->start+Delivery::Simulation::Forward(Campaign.layout->startHeading)*90;
            TestTrue(TEXT("The next drop has a clear route through the live streets"),
                Campaign.RouteClear(Campaign.layout->start,Corner,2)&&Campaign.RouteClear(Corner,Campaign.layout->goal,2));
            TestTrue(TEXT("The next order immediately starts with its own deadline"),Campaign.phase==Delivery::Phase::Riding&&Campaign.time>89.9);
        }
    }
    TestTrue(TEXT("Only the third drop completes stage two"),Campaign.phase==Delivery::Phase::Delivered);
    Campaign.AdvanceStage();
    Campaign.Start();
    Campaign.frontWavesEnabled=false;
    for(auto& Car:Campaign.cars) Car.spawnAt=1e9;
    for(int Order=0;Order<Delivery::Simulation::DeliveryCount;++Order)
    {
        CheckFreezePickup(Campaign);
        TestTrue(TEXT("Every final-stage order starts with one minute"),
            Campaign.layout->timeLimit==60&&Campaign.time>59.99&&Campaign.time<=60);
        const auto& FirstCoin=Campaign.timePickups[0];
        const auto& SecondCoin=Campaign.timePickups[1];
        TestTrue(TEXT("Each final-stage order has two well-separated time coins"),
            FirstCoin.available&&SecondCoin.available&&
            std::hypot(FirstCoin.position.x-SecondCoin.position.x,FirstCoin.position.y-SecondCoin.position.y)>=60);
        for(const auto& Coin:Campaign.timePickups)
        {
            TestFalse(TEXT("The time coin is clear of obstacles"),Campaign.IsBlocked(Coin.position,1.5));
            TestTrue(TEXT("The time coin lies on an authored alley"),std::any_of(Baseline->paths.begin(),Baseline->paths.end(),
                [&](const auto& Path)
                {
                    return Path.width>=8&&Path.width<=9&&std::abs(Coin.position.x-Path.ax)<.1&&
                        Coin.position.y>=FMath::Min(Path.ay,Path.by)&&Coin.position.y<=FMath::Max(Path.ay,Path.by);
                }));
        }
        const auto Traffic=Campaign.Traffic();
        TestEqual(TEXT("Final-stage traffic speed rises for each order"),Traffic.speed,21.+Order);
        TestEqual(TEXT("Final-stage waves arrive sooner for each order"),Traffic.waveInterval,8.-1.5*Order);
        TestEqual(TEXT("Only the last final-stage drop uses the clinic marker"),Campaign.IsClinicDelivery(),Order==2);
        TestFalse(TEXT("Every final-stage drop has free space on the live map"),Campaign.IsBlocked(Campaign.layout->goal,2));
        const auto Corner=Campaign.DeliveryJunction();
        TestTrue(TEXT("Every final-stage drop has a clear street route on the live map"),
            Campaign.RouteClear(Campaign.layout->start,Corner,2)&&Campaign.RouteClear(Corner,Campaign.layout->goal,2));
        const auto FirstLeg=Corner-Campaign.layout->start, LastLeg=Campaign.layout->goal-Corner;
        TestTrue(TEXT("Each final-stage live-map route is longer than before"),
            std::hypot(FirstLeg.x,FirstLeg.y)+std::hypot(LastLeg.x,LastLeg.y)>=269.9+90*Order);
        Campaign.bike=Campaign.layout->goal;Campaign.speed=0;
        Campaign.Tick(.01,{0,0,true});
        TestEqual(TEXT("Final-stage deliveries keep paying once per drop"),Campaign.money,400+100*Order);
        TestEqual(TEXT("Final-stage deliveries advance the order counter"),Campaign.deliveriesCompleted,Order+1);
        TestEqual(TEXT("Only the last final-stage drop finishes the campaign"),Campaign.phase,
            Order==2?Delivery::Phase::Delivered:Delivery::Phase::Riding);
    }
    TestTrue(TEXT("Actual terrain collision supplies height"),FMath::Abs(Baseline->height(130,35)-Terrain->SampleHeight(130,35))<.005);
    auto* Prop=World->SpawnActor<AStaticMeshActor>();auto* Mesh=Prop->GetStaticMeshComponent();
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Prop->SetActorLocation(FVector(7000,-19000,400));Prop->SetActorScale3D(FVector(10,2,6));
    auto First=Delivery::ReadLevelLayout(World);
    TestFalse(TEXT("The fixture position was free before insertion"),Baseline->Blocked({70,-190}));
    TestTrue(TEXT("A newly placed collidable cube blocks the ride"),First->Blocked({70,-190}));
    Prop->SetActorLocation(FVector(7000,-17500,400));Prop->SetActorScale3D(FVector(20,4,6));Prop->SetActorRotation(FRotator(0,90,0));
    auto Moved=Delivery::ReadLevelLayout(World);
    TestFalse(TEXT("Moving a prop leaves no invisible obstacle behind"),Moved->Blocked({70,-190}));
    TestTrue(TEXT("Scaled and rotated prop uses its real footprint"),Moved->Blocked({70,-168}));
    TestFalse(TEXT("Rotation does not leave an axis-aligned phantom wing"),Moved->Blocked({77,-175}));
    TestFalse(TEXT("Navigation tests the edited footprint"),Moved->Clear({65,-175},{75,-175},2.2));
    TestTrue(TEXT("Navigation graph has a route around the moved prop"),Moved->nodes.size()>0&&Moved->edges.size()==Moved->nodes.size());
    World->DestroyActor(Prop);
    auto Removed=Delivery::ReadLevelLayout(World);
    TestFalse(TEXT("Deleting a prop removes its collision from gameplay"),Removed->Blocked({70,-175}));
    if(Settings->StartActor&&Settings->GoalActor)
    {
        const FTransform OldStart=Settings->StartActor->GetActorTransform(),OldGoal=Settings->GoalActor->GetActorTransform();
        Settings->StartActor->SetActorLocation(FVector(-18000,-17000,100));
        Settings->StartActor->SetActorRotation(FRotator(0,45,0));
        Settings->GoalActor->SetActorLocation(FVector(17500,17500,10));
        auto Edited=Delivery::ReadLevelLayout(World);Delivery::Simulation Ride;Ride.SetLayout(Edited);
        TestTrue(TEXT("PlayerStart controls position and heading"),Ride.bike.x==-180&&Ride.bike.y==-170&&FMath::Abs(Ride.heading-PI/4)<.001);
        Ride.Restart();TestTrue(TEXT("Restart retains edited start and level rules"),Ride.bike.y==-170&&Ride.layout==Edited);
        Ride.bike=Edited->goal;Ride.Tick(.01,{0,0,true});
        TestTrue(TEXT("The moved destination completes delivery"),Ride.phase==Delivery::Phase::Delivered);
        Settings->StartActor->SetActorTransform(OldStart);Settings->GoalActor->SetActorTransform(OldGoal);
    }
    const FVector RoadLocation=Road->GetActorLocation();
    const FVector A=Road->Spline->GetLocationAtSplinePoint(0,ESplineCoordinateSpace::World);
    Road->SetActorLocation(RoadLocation+FVector(1000,0,0));Road->RebuildRoad();
    auto RoadEdit=Delivery::ReadLevelLayout(World);bool UpdatedRoad=false;
    for(const auto& Path:RoadEdit->paths) if(FMath::Abs(Path.ax-(A.X/100+10))<.001&&FMath::Abs(Path.ay-A.Y/100)<.001) UpdatedRoad=true;
    TestTrue(TEXT("Moving a spline updates runtime road/minimap/spawn data"),UpdatedRoad);
    Road->SetActorLocation(RoadLocation);Road->RebuildRoad();
    const double OldHeight=Hill->Height;const FVector Center=Hill->GetActorLocation()/100;
    const double Before=Terrain->SampleHeight(Center.X,Center.Y);
    Hill->Height+=400;Terrain->BuildSurface();
    auto Raised=Delivery::ReadLevelLayout(World);
    const double After=Raised->height(Center.X,Center.Y);
    TestTrue(TEXT("Editing a hill changes the real collider height"),After>Before+3.8);
    Delivery::Simulation Ride;Ride.SetLayout(Raised);Ride.bike={Center.X,Center.Y};
    auto* Bike=World->SpawnActor<ADeliveryBike>();Bike->Present(Ride,0);
    TestTrue(TEXT("Bike placement reads the edited collider"),FMath::Abs(Bike->GetActorLocation().Z-Ride.Pose(Ride.bike,0,.65,.25).height*100)<.1);
    World->DestroyActor(Bike);Hill->Height=OldHeight;Terrain->BuildSurface();
    return !HasAnyErrors();
}
#endif
