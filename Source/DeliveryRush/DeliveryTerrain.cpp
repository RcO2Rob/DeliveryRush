#include "DeliveryTerrain.h"
#include "DeliveryLevel.h"
#include "EngineUtils.h"
#include <functional>
#include "TerrainSurface.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#if WITH_EDITOR
#include "ProceduralMeshConversion.h"
#include "MeshDescription.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#endif
#include <cmath>

namespace
{
struct MeshData
{
    std::function<double(double,double)> Height=Delivery::GroundHeight;
    TArray<FVector> Vertices,Normals;
    TArray<int32> Triangles;
    TArray<FVector2D> UV;
    void Vertex(double x,double y,double lift)
    {
        Vertices.Add(FVector(x*100,y*100,(Height(x,y)+lift)*100));
        const double dx=(Height(x+.2,y)-Height(x-.2,y))/.4;
        const double dy=(Height(x,y+.2)-Height(x,y-.2))/.4;
        Normals.Add(FVector(-dx,-dy,1).GetSafeNormal());UV.Add(FVector2D(x*.1,y*.1));
    }
    void Ribbon(double ax,double ay,double bx,double by,double width,double lift,double step=2)
    {
        const double dx=bx-ax,dy=by-ay,length=std::hypot(dx,dy);
        if(length<.01||width<.01) return;
        const double sx=-dy/length,sy=dx/length;
        const int32 along=FMath::CeilToInt(length/step),across=FMath::CeilToInt(width/step);
        const int32 base=Vertices.Num();
        for(int32 i=0;i<=along;++i) for(int32 j=0;j<=across;++j)
        {
            const double t=double(i)/along,side=(double(j)/across-.5)*width;
            Vertex(ax+dx*t+sx*side,ay+dy*t+sy*side,lift);
        }
        for(int32 i=0;i<along;++i) for(int32 j=0;j<across;++j)
        {
            const int32 a=base+i*(across+1)+j,b=a+across+1,c=b+1,d=a+1;
            // Unreal front faces use clockwise winding viewed from above.
            Triangles.Append({a,d,c,a,c,b});
        }
    }
    void Disc(double x,double y,double diameter,double lift)
    {
        const int32 base=Vertices.Num();Vertex(x,y,lift);
        constexpr int32 Count=24;
        for(int32 i=0;i<Count;++i)
        { const double angle=i*2*PI/Count;Vertex(x+std::cos(angle)*diameter*.5,y+std::sin(angle)*diameter*.5,lift); }
        for(int32 i=0;i<Count;++i) Triangles.Append({base,base+1+(i+1)%Count,base+1+i});
    }
};
}
ADeliveryTerrain::ADeliveryTerrain()
{
    PrimaryActorTick.bCanEverTick=false;
    Surface=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainSurface"));
    RootComponent=Surface;Surface->bUseComplexAsSimpleCollision=true;
    Surface->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Surface->SetCollisionObjectType(ECC_WorldStatic);
    Surface->SetCollisionResponseToAllChannels(ECR_Block);
    Surface->SetCastShadow(true);
    BakedSurface=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BakedTerrain"));
    BakedSurface->SetupAttachment(Surface);
    BakedSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BakedSurface->SetVisibility(false);
}
void ADeliveryTerrain::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);BuildSurface();
    Surface->SetVisibility(true,false);BakedSurface->SetVisibility(false);
}
void ADeliveryTerrain::BeginPlay()
{
    Super::BeginPlay();
    // Recreate game render buffers from the shared terrain, rather than reuse
    // the editor's serialized preview buffers.
    BuildSurface();
#if WITH_EDITOR
    if(bUseLevelHills) BakeSurface(); // Include unsaved editor changes when entering PIE.
#endif
    if(BakedSurface->GetStaticMesh())
    {
        Surface->SetVisibility(false,false);BakedSurface->SetVisibility(true);
        for(int32 i=0;i<6;++i) BakedSurface->SetMaterial(i,Surface->GetMaterial(i));
    }
    Surface->MarkRenderStateDirty();
    for(int32 i=0;i<6;++i)
    {
        auto* Material=Surface->GetMaterial(i);
        auto* Base=Material?Material->GetMaterial():nullptr;
        UE_LOG(LogTemp,Display,TEXT("DELIVERY_TERRAIN_MATERIAL: section=%d material=%s static_mesh=%d shaders_ready=%d"),
            i,*GetNameSafe(Material),Base&&Base->GetUsageByFlag(MATUSAGE_StaticMesh),
            Base&&!Base->IsCompilingOrHadCompileError(GetFeatureLevelShaderPlatform_Checked(GetWorld()->GetFeatureLevel())));
    }
}
UStaticMesh* ADeliveryTerrain::BakeSurface()
{
#if WITH_EDITOR
    const FString AssetName=bUseLevelHills?TEXT("SM_")+UWorld::RemovePIEPrefix(GetWorld()->GetMapName()):TEXT("SM_DeliveryHills");
    const FString PackageName=TEXT("/Game/Terrain/")+AssetName;
    UStaticMesh* Mesh=LoadObject<UStaticMesh>(nullptr,*(PackageName+TEXT(".")+AssetName));
    if(!Mesh||bUseLevelHills)
    {
        FMeshDescription Description=BuildMeshDescription(Surface);
        UPackage* Package=CreatePackage(*PackageName);
        const bool bNew=!Mesh;
        if(!Mesh) Mesh=NewObject<UStaticMesh>(Package,*AssetName,RF_Public|RF_Standalone);
        Mesh->SetLightingGuid();
        if(Mesh->GetNumSourceModels()==0) Mesh->AddSourceModel();
        auto& Source=Mesh->GetSourceModel(0);
        Source.BuildSettings.bRecomputeNormals=false;
        Source.BuildSettings.bRecomputeTangents=true;
        Source.BuildSettings.bGenerateLightmapUVs=false;
        Source.BuildSettings.bUseFullPrecisionUVs=true;
        Mesh->CreateMeshDescription(0,MoveTemp(Description));Mesh->CommitMeshDescription(0);
        Mesh->GetStaticMaterials().Empty();
        for(int32 i=0;i<6;++i)
        {
            auto* Material=Surface->GetMaterial(i);
            Mesh->GetStaticMaterials().Add(FStaticMaterial(Material,Material->GetFName(),Material->GetFName()));
        }
        Mesh->NaniteSettings.bEnabled=true;
        Mesh->SetImportVersion(EImportStaticMeshVersion::LastVersion);
        Mesh->Build(false);Mesh->PostEditChange();
        if(bNew) FAssetRegistryModule::AssetCreated(Mesh);Mesh->MarkPackageDirty();
    }
    BakedSurface->SetStaticMesh(Mesh);MarkPackageDirty();
    return Mesh;
#else
    return BakedSurface->GetStaticMesh();
#endif
}
double ADeliveryTerrain::GetGroundHeight(double X,double Y)
{ return Delivery::GroundHeight(X,Y); }
#if WITH_EDITOR
void ADeliveryTerrain::PreSave(FObjectPreSaveContext SaveContext)
{
    if(bUseLevelHills&&!SaveContext.IsCooking()&&GetWorld()&&!GetWorld()->IsGameWorld())
    {
        BuildSurface();
        if(auto* Mesh=BakeSurface())
        {
            FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
            const FString Filename=FPackageName::LongPackageNameToFilename(Mesh->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
            if(!UPackage::SavePackage(Mesh->GetOutermost(),Mesh,*Filename,Args))
                UE_LOG(LogTemp,Error,TEXT("Could not save edited terrain visual: %s"),*Filename);
        }
    }
    Super::PreSave(SaveContext);
}
#endif
double ADeliveryTerrain::SampleHeight(double X,double Y) const
{
    using namespace Delivery;
    constexpr int Side=int(2*TerrainExtent/TerrainGridStep)+1;
    if(!bUseLevelHills||HeightSamples.Num()!=Side*Side) return GroundHeight(X,Y);
    X=FMath::Clamp(X,-TerrainExtent,TerrainExtent);Y=FMath::Clamp(Y,-TerrainExtent,TerrainExtent);
    const int IX=FMath::Min(FMath::FloorToInt((X+TerrainExtent)/TerrainGridStep),Side-2);
    const int IY=FMath::Min(FMath::FloorToInt((Y+TerrainExtent)/TerrainGridStep),Side-2);
    const double U=(X+TerrainExtent)/TerrainGridStep-IX,V=(Y+TerrainExtent)/TerrainGridStep-IY;
    const int Index=IX*Side+IY;
    const double A=HeightSamples[Index],B=HeightSamples[Index+Side],C=HeightSamples[Index+1],D=HeightSamples[Index+Side+1];
    return U>=V?A*(1-U)+B*(U-V)+D*V:A*(1-V)+C*(V-U)+D*U;
}
void ADeliveryTerrain::BuildSurface()
{
    using namespace Delivery;
    if(bUseLevelHills)
    {
        TArray<ADeliveryHill*> Hills;
        if(GetWorld()) for(TActorIterator<ADeliveryHill> It(GetWorld());It;++It) Hills.Add(*It);
        constexpr int Side=int(2*TerrainExtent/TerrainGridStep)+1;
        HeightSamples.SetNumZeroed(Side*Side);
        for(int X=0;X<Side;++X) for(int Y=0;Y<Side;++Y)
        {
            const double PX=(-TerrainExtent+X*TerrainGridStep)*100,PY=(-TerrainExtent+Y*TerrainGridStep)*100;
            double H=0;
            for(auto* Hill:Hills)
            {
                const FVector Local=Hill->GetActorTransform().InverseTransformPosition(FVector(PX,PY,0));
                const double DX=Local.X/Hill->RadiusX,DY=Local.Y/Hill->RadiusY;
                const double Influence=FMath::Max(0.,1-DX*DX-DY*DY);
                H+=(Hill->Height*Hill->GetActorScale3D().Z+Hill->GetActorLocation().Z)/100*Influence*Influence*Influence;
            }
            HeightSamples[X*Side+Y]=H;
        }
    }
    MeshData City,Grass,Road,Pavement,Dirt,Markings;
    MeshData* Sections[]={&City,&Grass,&Road,&Pavement,&Dirt,&Markings};
    for(auto* Section:Sections) Section->Height=[this](double X,double Y){ return SampleHeight(X,Y); };
    // Ground samples align to the shared grid and diagonal in GroundHeight().
    City.Ribbon(-TerrainExtent,0,ParkMinX,0,2*TerrainExtent,0);
    Grass.Ribbon(ParkMinX,0,TerrainExtent,0,2*TerrainExtent,0);
    if(!bUseLevelHills)
    {
    for(int axis=-180;axis<=180;axis+=90)
    {
        Road.Ribbon(-208,axis,ParkMinX,axis,18,.02);
        if(axis<=ParkMinX) Road.Ribbon(axis,-208,axis,208,18,.02);
        for(int center:{-135,-45,45,135}) for(int side:{-1,1})
        {
            if(center<ParkMinX) Pavement.Ribbon(center-33,axis+side*10.8,center+33,axis+side*10.8,3.4,.04);
            if(axis<=ParkMinX) Pavement.Ribbon(axis+side*10.8,center-33,axis+side*10.8,center+33,3.4,.04);
        }
        for(int along=-198;along<200;along+=12)
        {
            bool junction=false;for(int cross=-180;cross<=180;cross+=90) if(std::abs(along-cross)<12) junction=true;
            if(junction) continue;
            if(along<ParkMinX-2) Markings.Ribbon(along-1.75,axis,along+1.75,axis,.14,.045,1);
            if(axis<=ParkMinX) Markings.Ribbon(axis,along-1.75,axis,along+1.75,.14,.045,1);
        }
    }
    for(int alley:{-135,-45}) Pavement.Ribbon(alley,-180,alley,180,8.5,.025);
    for(const auto& path:ParkPathSegments)
    {
        Dirt.Ribbon(path.ax,path.ay,path.bx,path.by,path.width,.025,1);
        Dirt.Disc(path.ax,path.ay,path.width,.025);Dirt.Disc(path.bx,path.by,path.width,.025);
    }
    }
    Surface->ClearAllMeshSections();
    MeshData* sections[]={&City,&Grass,&Road,&Pavement,&Dirt,&Markings};
    for(int32 i=0;i<6;++i)
    {
        const MeshData& data=*sections[i];
        Surface->CreateMeshSection_LinearColor(i,data.Vertices,data.Triangles,data.Normals,data.UV,
            TArray<FLinearColor>{},TArray<FProcMeshTangent>{},i<2);
    }
    BindMaterials();
}
void ADeliveryTerrain::BindMaterials()
{
    const TCHAR* names[]={TEXT("Ground"),TEXT("Grass"),TEXT("Road"),TEXT("Pavement"),TEXT("Dirt"),TEXT("Amber")};
    for(int32 i=0;i<6;++i)
    {
        const FString path=FString::Printf(TEXT("/Game/Art/M_%s.M_%s"),names[i],names[i]);
        if(auto* material=LoadObject<UMaterialInterface>(nullptr,*path)) Surface->SetMaterial(i,material);
    }
}
