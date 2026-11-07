#include "DeliveryLevel.h"
#include "DeliveryTerrain.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Materials/MaterialInterface.h"

ADeliveryHill::ADeliveryHill()
{
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Outline=CreateDefaultSubobject<UBoxComponent>(TEXT("HillBounds"));Outline->SetupAttachment(RootComponent);
    Outline->SetCollisionEnabled(ECollisionEnabled::NoCollision);Outline->SetHiddenInGame(true);
    Outline->ShapeColor=FColor(100,220,100);SetActorHiddenInGame(true);
}
void ADeliveryHill::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Outline->SetBoxExtent(FVector(RadiusX,RadiusY,FMath::Max(100.,FMath::Abs(Height)*.5)));
    Outline->SetRelativeLocation(FVector(0,0,Height*.5));
}
void ADeliveryHill::RefreshTerrain()
{
#if WITH_EDITOR
    if(!GetWorld()||GetWorld()->IsGameWorld()||GetWorld()->bIsTearingDown) return;
    for(TActorIterator<ADeliveryTerrain> It(GetWorld());It;++It) if(It->bUseLevelHills)
    { It->Modify();It->BuildSurface();It->BakeSurface(); }
    for(TActorIterator<ADeliveryRoad> It(GetWorld());It;++It) It->RebuildRoad();
#endif
}
#if WITH_EDITOR
void ADeliveryHill::PostEditMove(bool bFinished) { Super::PostEditMove(bFinished);if(bFinished) RefreshTerrain(); }
void ADeliveryHill::PostEditUndo() { Super::PostEditUndo();RefreshTerrain(); }
void ADeliveryHill::Destroyed() { Super::Destroyed();RefreshTerrain(); }
void ADeliveryHill::PostEditChangeProperty(FPropertyChangedEvent& Event)
{ Super::PostEditChangeProperty(Event);RefreshTerrain(); }
#endif
ADeliveryRoad::ADeliveryRoad()
{
    Spline=CreateDefaultSubobject<USplineComponent>(TEXT("RoadSpline"));RootComponent=Spline;
    Spline->bInputSplinePointsToConstructionScript=true;
    Pieces=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RoadSurface"));Pieces->SetupAttachment(Spline);
    Pieces->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Pieces->SetCollisionEnabled(ECollisionEnabled::NoCollision);Pieces->SetCastShadow(false);
    Tags.Add(TEXT("DeliveryDecoration"));
}
void ADeliveryRoad::OnConstruction(const FTransform& Transform) { Super::OnConstruction(Transform);RebuildRoad(); }
void ADeliveryRoad::RebuildRoad()
{
    Pieces->ClearInstances();
    const TCHAR* Material=bDirtPath?TEXT("/Game/Art/M_Dirt.M_Dirt"):TEXT("/Game/Art/M_Road.M_Road");
    Pieces->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,Material));
    ADeliveryTerrain* Terrain=nullptr;
    if(GetWorld()) { TActorIterator<ADeliveryTerrain> It(GetWorld());if(It) Terrain=*It; }
    const double Length=Spline->GetSplineLength();const int Count=FMath::Max(1,FMath::CeilToInt(Length/200));
    auto Sample=[&](double Distance)
    {
        FVector P=Spline->GetLocationAtDistanceAlongSpline(Distance,ESplineCoordinateSpace::World);
        if(Terrain) P.Z=Terrain->SampleHeight(P.X/100,P.Y/100)*100+3;
        return P;
    };
    for(int i=0;i<Count;++i)
    {
        const FVector A=Sample(Length*i/Count),B=Sample(Length*(i+1)/Count),D=B-A;
        if(D.Size()<1) continue;
        const FTransform T(D.Rotation(),(A+B)*.5,FVector((D.Size()+3)/100,Width/100, .04));
        Pieces->AddInstance(T,true);
    }
    MarkPackageDirty();
}
ADeliveryLevelSettings::ADeliveryLevelSettings()
{ RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root")); }

void ADeliveryLevelSettings::MergeDecoration(AStaticMeshActor* Prop,AStaticMeshActor* Decoration)
{
    if(!Prop||!Decoration||Prop==Decoration) return;
    auto* Source=Decoration->GetStaticMeshComponent();
    auto* Component=NewObject<UStaticMeshComponent>(Prop,NAME_None,RF_Transactional);
    Prop->Modify();Component->SetStaticMesh(Source->GetStaticMesh());
    Component->SetMobility(Prop->GetStaticMeshComponent()->Mobility);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->ComponentTags.Add(TEXT("DeliveryDecoration"));
    for(int I=0;I<Source->GetNumMaterials();++I) Component->SetMaterial(I,Source->GetMaterial(I));
    Component->SetupAttachment(Prop->GetRootComponent());
    Component->SetRelativeTransform(Source->GetComponentTransform().GetRelativeTransform(Prop->GetRootComponent()->GetComponentTransform()));
    Prop->AddInstanceComponent(Component);Component->RegisterComponent();Prop->MarkPackageDirty();
}

namespace Delivery
{
std::shared_ptr<const LevelLayout> ReadLevelLayout(UWorld* World)
{
    if(!World) return DefaultLevelLayout();
    ADeliveryLevelSettings* Settings=nullptr;
    { TActorIterator<ADeliveryLevelSettings> It(World);if(It) Settings=*It; }
    if(!Settings) return DefaultLevelLayout(); // Historical maps keep their original rules.
    auto Level=std::make_shared<LevelLayout>();
    Level->limit=Settings->WorldHalfExtent/100;Level->timeLimit=Settings->TimeLimit;
    AActor* Start=Settings->StartActor;
    if(!Start) { TActorIterator<APlayerStart> It(World);if(It) Start=*It; }
    if(Start) { const FVector P=Start->GetActorLocation()/100;Level->start={P.X,P.Y};Level->startHeading=FMath::DegreesToRadians(Start->GetActorRotation().Yaw); }
    if(Settings->GoalActor) { const FVector P=Settings->GoalActor->GetActorLocation()/100;Level->goal={P.X,P.Y}; }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DeliveryLevelGround),true);
    if(Start) Query.AddIgnoredActor(Start);
    if(Settings->GoalActor) Query.AddIgnoredActor(Settings->GoalActor);
    for(TActorIterator<AActor> It(World);It;++It)
    {
        AActor* Actor=*It;
        if(Actor->ActorHasTag(TEXT("DeliveryGround"))||Actor->IsA<ADeliveryTerrain>()) continue;
        if(Actor->ActorHasTag(TEXT("DeliveryDecoration"))) { Query.AddIgnoredActor(Actor);continue; }
        TArray<UStaticMeshComponent*> Meshes;Actor->GetComponents(Meshes);
        bool Obstacle=false;
        for(auto* Mesh:Meshes)
        {
            if(!Mesh->GetStaticMesh()||Mesh->GetCollisionEnabled()==ECollisionEnabled::NoCollision||Mesh->ComponentHasTag(TEXT("DeliveryDecoration"))) continue;
            const FBox Box=Mesh->GetStaticMesh()->GetBoundingBox();
            const FTransform Transform=Mesh->GetComponentTransform();
            const double Yaw=FMath::DegreesToRadians(Transform.Rotator().Yaw);
            const FVector Center=Transform.TransformPosition(Box.GetCenter());
            double HalfX=0,HalfY=0,Low=1e12,High=-1e12;
            for(int Mask=0;Mask<8;++Mask)
            {
                const FVector V((Mask&1)?Box.Max.X:Box.Min.X,(Mask&2)?Box.Max.Y:Box.Min.Y,(Mask&4)?Box.Max.Z:Box.Min.Z);
                const FVector P=Transform.TransformPosition(V),D=P-Center;
                HalfX=FMath::Max(HalfX,FMath::Abs(FMath::Cos(Yaw)*D.X+FMath::Sin(Yaw)*D.Y));
                HalfY=FMath::Max(HalfY,FMath::Abs(-FMath::Sin(Yaw)*D.X+FMath::Cos(Yaw)*D.Y));
                Low=FMath::Min(Low,P.Z);High=FMath::Max(High,P.Z);
            }
            if(HalfX<1||HalfY<1) continue;
            Level->obstacles.push_back({Center.X/100,Center.Y/100,HalfX/100,HalfY/100,(High-Low)/100,Yaw});
            Obstacle=true;
        }
        if(Obstacle) Query.AddIgnoredActor(Actor);
    }
    // Query the actual native terrain, Landscape or tagged collidable ground.
    // Props are excluded so a roof cannot be mistaken for the riding surface.
    const TWeakObjectPtr<UWorld> WeakWorld(World);
    Level->height=[WeakWorld,Query](double X,double Y)
    {
        if(auto* W=WeakWorld.Get())
        {
            FHitResult Hit;
            if(W->LineTraceSingleByChannel(Hit,FVector(X*100,Y*100,100000),FVector(X*100,Y*100,-100000),ECC_Visibility,Query)) return double(Hit.ImpactPoint.Z/100);
        }
        return 0.;
    };
    for(TActorIterator<ADeliveryRoad> It(World);It;++It)
    {
        auto* Spline=It->Spline;const double Length=Spline->GetSplineLength();
        const int Count=FMath::Max(1,FMath::CeilToInt(Length/800));
        for(int i=0;i<=Count;++i)
        {
            const FVector P=Spline->GetLocationAtDistanceAlongSpline(Length*i/Count,ESplineCoordinateSpace::World)/100;
            Level->nodes.push_back({P.X,P.Y});Level->spawnPoints.push_back({P.X,P.Y});
            const FVector Right=Spline->GetRightVectorAtDistanceAlongSpline(Length*i/Count,ESplineCoordinateSpace::World);
            for(double Lane:{-1.,1.}) Level->spawnPoints.push_back({P.X+Right.X*It->Width/400*Lane,P.Y+Right.Y*It->Width/400*Lane});
            if(i<Count)
            {
                const FVector B=Spline->GetLocationAtDistanceAlongSpline(Length*(i+1)/Count,ESplineCoordinateSpace::World)/100;
                Level->paths.push_back({P.X,P.Y,B.X,B.Y,It->Width/100});
            }
        }
    }
    // Unmarked open ground remains navigable when roads are moved or deleted.
    for(double X=-Level->limit+18;X<Level->limit;X+=40) for(double Y=-Level->limit+18;Y<Level->limit;Y+=40)
        Level->nodes.push_back({X,Y});
    if(Level->spawnPoints.empty()) Level->spawnPoints=Level->nodes;
    Level->BuildNavigation();
    UE_LOG(LogTemp,Display,TEXT("DELIVERY_LEVEL_LAYOUT: obstacles=%d nodes=%d roads=%d start=(%.1f,%.1f) goal=(%.1f,%.1f)"),
        int(Level->obstacles.size()),int(Level->nodes.size()),int(Level->paths.size()),Level->start.x,Level->start.y,Level->goal.x,Level->goal.y);
    return Level;
}
}
