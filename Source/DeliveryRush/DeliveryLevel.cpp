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
#include "ProceduralMeshComponent.h"

namespace
{
using RoadPolygon = TArray<FVector2D, TInlineAllocator<12>>;

RoadPolygon ClipRoadPolygon(const RoadPolygon& Polygon, FVector2D Normal, double Offset)
{
    RoadPolygon Result;
    if (Polygon.IsEmpty()) return Result;
    FVector2D Previous = Polygon.Last();
    double PreviousSide = FVector2D::DotProduct(Previous, Normal) - Offset;
    for (const FVector2D& Point : Polygon)
    {
        const double Side = FVector2D::DotProduct(Point, Normal) - Offset;
        if ((Side >= 0) != (PreviousSide >= 0))
            Result.Add(FMath::Lerp(Previous, Point, PreviousSide / (PreviousSide - Side)));
        if (Side >= 0) Result.Add(Point);
        Previous = Point;
        PreviousSide = Side;
    }
    return Result;
}

struct RoadMesh
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;

    void Add(const RoadPolygon& Polygon, const ADeliveryTerrain* Terrain,
        const FTransform& RoadTransform, double Lift, double FallbackZ)
    {
        if (Polygon.Num() < 3) return;
        double PolygonArea = 0;
        for (int32 Index = 1; Index + 1 < Polygon.Num(); ++Index)
            PolygonArea += FVector2D::CrossProduct(Polygon[Index] - Polygon[0], Polygon[Index + 1] - Polygon[0]);
        if (FMath::Abs(PolygonArea) < .01) return;
        const int32 Base = Vertices.Num();
        for (const auto& Point : Polygon)
        {
            const double Height = Terrain ? Terrain->SampleHeight(Point.X / 100, Point.Y / 100) * 100 : FallbackZ;
            Vertices.Add(RoadTransform.InverseTransformPosition(FVector(Point.X, Point.Y, Height + Lift)));
            FVector Normal = FVector::UpVector;
            if (Terrain)
            {
                const double X = Point.X / 100, Y = Point.Y / 100;
                const double SlopeX = (Terrain->SampleHeight(X + .2, Y) - Terrain->SampleHeight(X - .2, Y)) / .4;
                const double SlopeY = (Terrain->SampleHeight(X, Y + .2) - Terrain->SampleHeight(X, Y - .2)) / .4;
                Normal = FVector(-SlopeX, -SlopeY, 1).GetSafeNormal();
            }
            Normals.Add(RoadTransform.InverseTransformVectorNoScale(Normal));
            UVs.Add(Point / 1000);
        }
        for (int32 Index = 1; Index + 1 < Polygon.Num(); ++Index)
        {
            const double Area = FVector2D::CrossProduct(Polygon[Index] - Polygon[0], Polygon[Index + 1] - Polygon[0]);
            if (FMath::Abs(Area) < .01) continue;
            if (Area < 0) Triangles.Append({Base, Base + Index, Base + Index + 1});
            else Triangles.Append({Base, Base + Index + 1, Base + Index});
        }
    }
};

void DrapeRoadStrip(RoadMesh& Mesh, RoadPolygon Strip, const ADeliveryTerrain* Terrain,
    const FTransform& RoadTransform, double Limit, double Lift, double FallbackZ)
{
    Strip = ClipRoadPolygon(Strip, {1, 0}, -Limit);
    Strip = ClipRoadPolygon(Strip, {-1, 0}, -Limit);
    Strip = ClipRoadPolygon(Strip, {0, 1}, -Limit);
    Strip = ClipRoadPolygon(Strip, {0, -1}, -Limit);
    if (Strip.Num() < 3) return;
    if (!Terrain)
    {
        Mesh.Add(Strip, nullptr, RoadTransform, Lift, FallbackZ);
        return;
    }

    // Split at the same grid edges and diagonal as the terrain. A road triangle
    // then follows one ground triangle exactly, including across steep slopes.
    constexpr double Grid = Delivery::TerrainGridStep * 100;
    constexpr double Origin = -Delivery::TerrainExtent * 100;
    FBox2D Bounds(ForceInit);
    for (const auto& Point : Strip) Bounds += Point;
    const int32 MinX = FMath::FloorToInt((Bounds.Min.X - Origin) / Grid);
    const int32 MaxX = FMath::FloorToInt((Bounds.Max.X - Origin) / Grid);
    const int32 MinY = FMath::FloorToInt((Bounds.Min.Y - Origin) / Grid);
    const int32 MaxY = FMath::FloorToInt((Bounds.Max.Y - Origin) / Grid);
    for (int32 X = MinX; X <= MaxX; ++X)
    {
        const double CellX = Origin + X * Grid;
        auto Column = ClipRoadPolygon(Strip, {1, 0}, CellX);
        Column = ClipRoadPolygon(Column, {-1, 0}, -CellX - Grid);
        for (int32 Y = MinY; Y <= MaxY; ++Y)
        {
            const double CellY = Origin + Y * Grid;
            auto Cell = ClipRoadPolygon(Column, {0, 1}, CellY);
            Cell = ClipRoadPolygon(Cell, {0, -1}, -CellY - Grid);
            Mesh.Add(ClipRoadPolygon(Cell, {1, -1}, CellX - CellY), Terrain, RoadTransform, Lift, FallbackZ);
            Mesh.Add(ClipRoadPolygon(Cell, {-1, 1}, CellY - CellX), Terrain, RoadTransform, Lift, FallbackZ);
        }
    }
}
}

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
    Ribbon = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ConformingRoad"));
    Ribbon->SetFlags(RF_Transient);
    Ribbon->SetupAttachment(Spline);
    Ribbon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Ribbon->SetCastShadow(false);
    Tags.Add(TEXT("DeliveryDecoration"));
}
void ADeliveryRoad::OnConstruction(const FTransform& Transform) { Super::OnConstruction(Transform);RebuildRoad(); }
void ADeliveryRoad::PostRegisterAllComponents()
{
    Super::PostRegisterAllComponents();
    if (GetWorld() && !GetWorld()->IsGameWorld()) RebuildRoad();
}
void ADeliveryRoad::RebuildRoad()
{
    Pieces->ClearInstances();
    Pieces->SetVisibility(false);
    const TCHAR* Material=bDirtPath?TEXT("/Game/Art/M_Dirt.M_Dirt"):TEXT("/Game/Art/M_Road.M_Road");
    Ribbon->ClearAllMeshSections();
    Ribbon->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,Material));
    ADeliveryTerrain* Terrain=nullptr;
    double Limit = Delivery::TerrainExtent * 100;
    int32 Layer = 0, LayerCount = 0;
    if (UWorld* World = GetWorld())
    {
        TActorIterator<ADeliveryTerrain> It(World);
        if (It) Terrain = *It;
        TActorIterator<ADeliveryLevelSettings> Settings(World);
        if (Settings) Limit = FMath::Min(Limit, Settings->WorldHalfExtent);
        for (TActorIterator<ADeliveryRoad> Road(World); Road; ++Road)
        {
            if (Road->bDirtPath != bDirtPath) continue;
            ++LayerCount;
            if (Road->GetFName().LexicalLess(GetFName())) ++Layer;
        }
    }
    // Keep intersections ordered by material, with pavement covering dirt ends.
    const double Lift = (bDirtPath ? 2. : 8.) + 3. * (Layer + 1) / (LayerCount + 1);
    TArray<FVector2D> Centers;
    const int32 Points = Spline->GetNumberOfSplinePoints();
    for (int32 Index = 0; Index + 1 < Points; ++Index)
    {
        const double Start = Spline->GetDistanceAlongSplineAtSplinePoint(Index);
        const double End = Spline->GetDistanceAlongSplineAtSplinePoint(Index + 1);
        const bool Straight = Spline->GetSplinePointType(Index) == ESplinePointType::Linear;
        const int32 Steps = Straight ? 1 : FMath::Max(1, FMath::CeilToInt((End - Start) / 200));
        for (int32 Step = 0; Step < Steps; ++Step)
        {
            const double Distance = FMath::Lerp(Start, End, double(Step) / Steps);
            const FVector2D Point(Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World));
            if (Centers.IsEmpty() || !Point.Equals(Centers.Last(), .01)) Centers.Add(Point);
        }
    }
    if (Points < 2) return;
    const FVector2D End(Spline->GetLocationAtSplinePoint(Points - 1, ESplineCoordinateSpace::World));
    if (Centers.IsEmpty() || !End.Equals(Centers.Last(), .01)) Centers.Add(End);
    if (Centers.Num() < 2) return;
    TArray<FVector2D> Left, Right;
    for (int32 Index = 0; Index < Centers.Num(); ++Index)
    {
        const auto Incoming = (Centers[Index] - Centers[FMath::Max(0, Index - 1)]).GetSafeNormal();
        const auto Outgoing = (Centers[FMath::Min(Index + 1, Centers.Num() - 1)] - Centers[Index]).GetSafeNormal();
        auto Tangent = (Incoming + Outgoing).GetSafeNormal();
        if (Tangent.IsNearlyZero()) Tangent = Outgoing;
        const FVector2D Side(-Tangent.Y, Tangent.X);
        const double Miter = FMath::Max(.65, FVector2D::DotProduct(Side, FVector2D(-Outgoing.Y, Outgoing.X)));
        const FVector2D Offset = Side * (Width * .5 / (Index + 1 == Centers.Num() ? 1. : Miter));
        Left.Add(Centers[Index] + Offset);
        Right.Add(Centers[Index] - Offset);
    }
    RoadMesh Mesh;
    const FTransform Transform = Ribbon->GetComponentTransform();
    for (int32 Index = 0; Index + 1 < Centers.Num(); ++Index)
        DrapeRoadStrip(Mesh, {Left[Index], Left[Index + 1], Right[Index + 1], Right[Index]},
            Terrain, Transform, Limit, Lift, GetActorLocation().Z);
    Ribbon->CreateMeshSection_LinearColor(0, Mesh.Vertices, Mesh.Triangles, Mesh.Normals, Mesh.UVs,
        TArray<FLinearColor>{}, TArray<FProcMeshTangent>{}, false);
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
