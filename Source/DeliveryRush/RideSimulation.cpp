#include "RideSimulation.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

namespace Delivery
{
namespace
{
constexpr double Pi = 3.14159265358979323846;
double Dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
double Length(Vec2 a) { return std::sqrt(Dot(a,a)); }
Vec2 Normal(Vec2 a) { const double n=Length(a); return n>1e-8 ? a*(1/n) : Vec2{1,0}; }
double Wrap(double angle) { return std::remainder(angle,2*Pi); }
double Approach(double a,double b,double amount)
{ return a<b ? std::min(a+amount,b) : std::max(a-amount,b); }
constexpr double CarLength = 2.1, CarWidth = .92;
// Separating-axis test for two oriented rectangles. The normal points from B
// to A; the depth is the smallest translation that separates their footprints.
bool BoxContact(Vec2 a,Vec2 af,double ax,double ay,Vec2 b,Vec2 bf,double bx,double by,
                Vec2& normal,double& depth)
{
    const Vec2 as{-af.y,af.x}, bs{-bf.y,bf.x}, offset=a-b;
    const Vec2 axes[]={af,as,bf,bs};
    depth=std::numeric_limits<double>::max();
    for(Vec2 axis:axes)
    {
        const double ar=ax*std::abs(Dot(af,axis))+ay*std::abs(Dot(as,axis));
        const double br=bx*std::abs(Dot(bf,axis))+by*std::abs(Dot(bs,axis));
        const double projected=Dot(offset,axis), overlap=ar+br-std::abs(projected);
        if(overlap<=0) return false;
        if(overlap<depth) { depth=overlap; normal=axis*(projected>=0?1:-1); }
    }
    return true;
}
}
Vec2 Simulation::Forward(double angle) { return {std::cos(angle),std::sin(angle)}; }
Simulation::Simulation() : Simulation(std::random_device{}()) {}
Simulation::Simulation(unsigned trafficSeed) : layout(DefaultLevelLayout())
{
    std::seed_seq seed{trafficSeed};
    trafficRandom.seed(seed);
    Reset();
}
void Simulation::SetLayout(std::shared_ptr<const LevelLayout> level)
{
    campaignLayout.reset();
    money=0; deliveriesCompleted=0;
    stage=CampaignStage::CityRush;
    layout=level?std::move(level):DefaultLevelLayout();
    Reset();
}
void Simulation::BeginCampaign(std::shared_ptr<const LevelLayout> level)
{
    money=0;
    campaignLayout=level?std::move(level):DefaultLevelLayout();
    LoadStage(CampaignStage::Tutorial);
}
void Simulation::AdvanceStage()
{
    if(!campaignLayout||phase!=Phase::Delivered) return;
    switch(stage)
    {
        case CampaignStage::Tutorial: LoadStage(CampaignStage::FirstChase); break;
        case CampaignStage::FirstChase: LoadStage(CampaignStage::CityRush); break;
        case CampaignStage::CityRush: LoadStage(CampaignStage::Tutorial); break;
    }
}
void Simulation::LoadStage(CampaignStage nextStage)
{
    stage=nextStage;
    if(stage==CampaignStage::Tutorial) money=0;
    deliveriesCompleted=0;
    layout=campaignLayout;
    {
        auto route=std::make_shared<LevelLayout>(*campaignLayout);
        const Vec2 forward=Forward(route->startHeading), right{-forward.y,forward.x};
        const auto clearLeg=[&](Vec2 from,Vec2 to)
        {
            return std::abs(to.x)<route->limit-6&&std::abs(to.y)<route->limit-6&&route->Clear(from,to,2);
        };
        if(stage==CampaignStage::Tutorial)
        {
            for(double distance:{75.,60.,45.,30.,15.})
            {
                const Vec2 target=route->start+forward*distance;
                if(!clearLeg(route->start,target)) continue;
                route->goal=target;
                break;
            }
        }
        else
        {
            const Vec2 corner=route->start+forward*90;
            const Vec2 target=corner+right*90;
            if(clearLeg(route->start,corner)&&clearLeg(corner,target)) route->goal=target;
            deliveryStops={route->goal,route->start+forward*90+right*270,
                stage==CampaignStage::CityRush?campaignLayout->goal:route->start+forward*180+right*360};
            deliveryJunctions={corner,deliveryStops[0]+right*90,
                deliveryStops[1]+(stage==CampaignStage::CityRush?right:forward)*90};
            if(stage==CampaignStage::CityRush)
            {
                // Cross the city in three longer legs, ending at the authored clinic.
                const auto start=route->start;
                const std::array<Vec2,DeliveryCount> stops{
                    start+forward*180+right*90,start+right*270,campaignLayout->goal};
                const std::array<Vec2,DeliveryCount> junctions{
                    start+forward*180,start+forward*180+right*270,start+right*360};
                bool clear=true;
                Vec2 from=start;
                for(std::size_t i=0;i<stops.size()&&clear;++i)
                {
                    clear=clearLeg(from,junctions[i])&&clearLeg(junctions[i],stops[i]);
                    from=stops[i];
                }
                if(clear)
                {
                    deliveryStops=stops;
                    deliveryJunctions=junctions;
                }
            }
            route->goal=deliveryStops[0];
            route->timeLimit=stage==CampaignStage::FirstChase?90:60;
        }
        layout=std::move(route);
    }
    Reset();
}
TrafficRules Simulation::Traffic() const
{
    if(stage==CampaignStage::Tutorial) return {};
    const int order=HasDeliverySequence()?std::clamp(deliveriesCompleted,0,DeliveryCount-1):0;
    if(stage==CampaignStage::FirstChase)
        return {OncomingSpeed+order,4.-.5*order,24.-2*order,4+order};
    return {CityCruiseSpeed+order,FrontWaveInterval-1.5*order,FrontWaveTravel-5*order,
        HasDeliverySequence()?(order==0?5:order==1?6:MaxActiveCars):MaxActiveCars};
}
Vec2 Simulation::DeliveryJunction() const
{
    if(!HasDeliverySequence()) return layout->start+Forward(layout->startHeading)*90;
    return deliveryJunctions[std::clamp(deliveriesCompleted,0,DeliveryCount-1)];
}
bool Simulation::RouteClear(Vec2 a,Vec2 b,double padding) const { return layout->Clear(a,b,padding); }
bool Simulation::IsBlocked(Vec2 p,double radius) const { return layout->Blocked(p,radius); }
GroundPose Simulation::Pose(Vec2 p,double angle,double halfLength,double halfWidth) const
{ return PoseOnLevel(*layout,p.x,p.y,angle,halfLength,halfWidth); }
void Simulation::Reset()
{
    phase=Phase::Ready; bike=layout->start; heading=layout->startHeading; speed=0; time=layout->timeLimit;
    elapsed=travelled=steering=roll=rollVelocity=hitFlash=fallTimer=restartFlash=0;
    shove={}; falls=grazes=0; attempt=1;
    frontWavesEnabled=stage!=CampaignStage::Tutorial; wavesStarted=0;
    nextFrontWaveAt=stage==CampaignStage::FirstChase?2.5:0;
    travelAtLastWave=nextWaveCheckAt=0;
    pendingFrontWaveSize=0;
    for(std::size_t i=0;i<cars.size();++i)
    {
        cars[i]=Car{}; cars[i].identity=int(i)+1;
        if(stage==CampaignStage::CityRush&&i<InitialCarCount)
        { cars[i].wave=1; cars[i].formationIndex=int(i); cars[i].spawnAt=2.5; }
    }
    timePickups={}; pickupsCollected=0; timePickupFlash=0;
    freezePickups={}; freezeRemaining=trafficElapsed=0; freezesCollected=0;
    deliveryFlash=0;
    if(HasDeliverySequence()) PlaceOrderPickups();
}
void Simulation::CompleteDelivery()
{
    if(!HasDeliverySequence()||deliveriesCompleted==DeliveryCount)
    {
        phase=Phase::Delivered;
        return;
    }
    if(time<=0) { phase=Phase::Failed; return; }
    ++deliveriesCompleted;
    money+=DeliveryReward;
    deliveryFlash=2.5;
    if(deliveriesCompleted==DeliveryCount)
    {
        phase=Phase::Delivered;
        return;
    }
    auto next=std::make_shared<LevelLayout>(*layout);
    next->start=deliveryStops[deliveriesCompleted-1];
    const Vec2 firstLeg=DeliveryJunction()-next->start;
    next->startHeading=std::atan2(firstLeg.y,firstLeg.x);
    next->goal=deliveryStops[deliveriesCompleted];
    layout=std::move(next);
    time=layout->timeLimit;
    timePickups={}; pickupsCollected=0; timePickupFlash=0;
    freezePickups={};
    PlaceOrderPickups();
    nextFrontWaveAt=std::min(nextFrontWaveAt,elapsed+Traffic().waveInterval);
    nextWaveCheckAt=elapsed+.5;
}
void Simulation::PlaceOrderPickups()
{
    if(stage==CampaignStage::FirstChase) PlacePickups(timePickups.data(),1,false);
    else PlaceAlleyTimePickups();
    PlacePickups(freezePickups.data(),freezePickups.size(),true);
}
void Simulation::PlaceAlleyTimePickups()
{
    struct Candidate { Vec2 position; double routeProgress; };
    const std::array<Vec2,3> route{layout->start,DeliveryJunction(),layout->goal};
    std::vector<Candidate> candidates;
    for(const auto& path:layout->paths)
    {
        // City alleys are 8.5 metres wide. The main streets and park trails
        // have different widths in both the editable and built-in maps.
        if(path.width<8||path.width>9) continue;
        const Vec2 start{path.ax,path.ay}, segment=Vec2{path.bx,path.by}-start;
        const int samples=std::max(1,int(std::ceil(Length(segment)/6)));
        for(int i=0;i<samples;++i)
        {
            const Vec2 point=start+segment*((i+.5)/samples);
            if(std::abs(point.x)>layout->limit-3||std::abs(point.y)>layout->limit-3||IsBlocked(point,1.5)) continue;
            if(Length(point-route.front())<20||Length(point-route.back())<20) continue;
            Vec2 nearest;
            double nearestDistance=std::numeric_limits<double>::max(), progress=0, preceding=0;
            for(std::size_t leg=0;leg+1<route.size();++leg)
            {
                const Vec2 delta=route[leg+1]-route[leg];
                const double length=Length(delta);
                if(length<1e-6) continue;
                const Vec2 along=delta*(1/length);
                const double projection=std::clamp(Dot(point-route[leg],along),0.,length);
                const Vec2 onRoute=route[leg]+along*projection;
                const double distance=Length(point-onRoute);
                if(distance<nearestDistance)
                {
                    nearest=onRoute;
                    nearestDistance=distance;
                    progress=preceding+projection;
                }
                preceding+=length;
            }
            if(nearestDistance>=12&&nearestDistance<=60&&RouteClear(nearest,point,1.5))
                candidates.push_back({point,progress});
        }
    }
    std::mt19937 random(std::random_device{}());
    std::shuffle(candidates.begin(),candidates.end(),random);
    for(const auto& first:candidates) for(const auto& second:candidates)
    {
        if(second.routeProgress-first.routeProgress<60||Length(second.position-first.position)<60) continue;
        timePickups={Pickup{first.position,true},Pickup{second.position,true}};
        return;
    }
    if(!candidates.empty()) timePickups[0]={candidates.front().position,true};
}
void Simulation::PlacePickups(Pickup* pickups,std::size_t capacity,bool freezing)
{
    std::mt19937 random(std::random_device{}());
    const Vec2 junction=DeliveryJunction();
    const std::array<Vec2,3> route{layout->start,junction,layout->goal};
    std::vector<Vec2> candidates;
    for(std::size_t leg=0;leg+1<route.size();++leg)
    {
        const Vec2 from=route[leg], delta=route[leg+1]-from;
        const double length=Length(delta);
        if(length<=24) continue;
        const Vec2 along=delta*(1/length);
        for(Vec2 point:layout->spawnPoints)
        {
            const double progress=Dot(point-from,along);
            const Vec2 center=from+along*progress;
            if(progress<(freezing?40.:12.)||progress>length-12||Length(point-center)>6) continue;
            if(std::abs(point.x)>layout->limit-3||std::abs(point.y)>layout->limit-3||!RouteClear(center,point,1.5)) continue;
            candidates.push_back(point);
        }
    }
    std::shuffle(candidates.begin(),candidates.end(),random);
    std::size_t count=0;
    for(Vec2 point:candidates)
    {
        if(count==capacity) break;
        if(std::any_of(pickups,pickups+count,
            [&](const Pickup& pickup){ return Length(point-pickup.position)<12; })) continue;
        if(freezing&&std::any_of(timePickups.begin(),timePickups.end(),
            [&](const Pickup& pickup){ return pickup.available&&Length(point-pickup.position)<6; })) continue;
        pickups[count++]={point,true};
    }
}
void Simulation::CollectFreezePickups()
{
    for(Pickup& pickup:freezePickups)
    {
        if(!pickup.available||Length(bike-pickup.position)>2.1) continue;
        pickup.available=false;
        ++freezesCollected;
        freezeRemaining=FreezeDuration;
    }
}
void Simulation::CollectTimePickups()
{
    for(TimePickup& pickup:timePickups)
    {
        if(!pickup.available||Length(bike-pickup.position)>2.1) continue;
        pickup.available=false;
        ++pickupsCollected;
        time+=TimePickupBonus;
        timePickupFlash=1.5;
    }
}
void Simulation::Start() { if(phase==Phase::Ready) { phase=Phase::Riding; speed=HasTimeLimit()?4:0; } }
void Simulation::Restart()
{
    const int savedFalls=falls, savedAttempt=attempt;
    Reset(); falls=savedFalls; attempt=savedAttempt+1; Start(); restartFlash=1;
}
void Simulation::TogglePause()
{
    if(phase==Phase::Riding) phase=Phase::Paused;
    else if(phase==Phase::Paused) phase=Phase::Riding;
}
void Simulation::Fall(double direction)
{
    if(phase!=Phase::Riding) return;
    phase=Phase::Fallen; ++falls; fallTimer=0;
    fallDirection=direction<0?-1:1; rollVelocity=0; hitFlash=.5;
}
void Simulation::Tick(double dt,const Input& input)
{
    if(!std::isfinite(dt)||dt<=0||(phase!=Phase::Riding&&phase!=Phase::Fallen)) return;
    while(dt>1e-8&&(phase==Phase::Riding||phase==Phase::Fallen))
    {
        const double step=std::min(dt,1.0/120.0); dt-=step;
        if(phase==Phase::Fallen)
        {
            fallTimer+=step; speed=Approach(speed,0,step*20);
            roll=Approach(roll,fallDirection*1.45,step*4.5);
            if(fallTimer>=1.15) Restart();
        }
        else Step(step,input);
    }
}
void Simulation::Step(double dt,const Input& input)
{
    if(HasTimeLimit()) { dt=std::min(dt,time); time=std::max(0.0,time-dt); }
    elapsed+=dt;
    const double previousFreeze=freezeRemaining;
    freezeRemaining=std::max(0.0,freezeRemaining-dt);
    hitFlash=std::max(0.0,hitFlash-dt); restartFlash=std::max(0.0,restartFlash-dt);
    timePickupFlash=std::max(0.0,timePickupFlash-dt);
    deliveryFlash=std::max(0.0,deliveryFlash-dt);
    steering=std::clamp(input.steer,-1.0,1.0);
    if(input.brake) speed=Approach(speed,0,dt*14);
    else if(input.throttle>.1) speed=std::min(HasTimeLimit()?BikeMaxSpeed:11.,speed+dt*BikeAcceleration);
    else if(input.throttle<-.1) speed=std::max(-3.2,speed-dt*(speed>0?10.5:3));
    else speed=Approach(speed,0,dt*.8);
    heading=Wrap(heading+steering*SteeringResponse*(.65+std::min(std::abs(speed)/7,1.0))
        *std::min(std::abs(speed)/2,1.0)*(speed>=0?1:-1)*dt);
    rollVelocity+=(-15*roll-6*rollVelocity)*dt;
    roll+=rollVelocity*dt;
    shove=shove*std::exp(-4*dt);
    MoveBike(dt);
    CollectTimePickups();
    const int previousFreezes=freezesCollected;
    CollectFreezePickups();
    const double frozenStep=freezesCollected>previousFreezes?dt:std::min(dt,previousFreeze);
    const double trafficStep=dt-frozenStep;
    trafficElapsed+=trafficStep;
    if(frozenStep>0)
    {
        // Preserve scheduled entries and attack ages; thawing never catches up
        // five seconds of movement or immediately expires a one-pass car.
        nextFrontWaveAt+=frozenStep; nextWaveCheckAt+=frozenStep;
        for(Car& car:cars)
        {
            car.spawnAt+=frozenStep;
            if(car.passes>0) car.lastPassAt+=frozenStep;
            if(car.rearAttacks>0) car.lastRamAt+=frozenStep;
        }
    }
    if(trafficStep>1e-8)
    {
        for(Car& car:cars)
        {
            if(car.phase==CarPhase::Waiting)
            {
                if(car.wave==1&&elapsed>=car.spawnAt&&travelled>30&&ActiveCars()<Traffic().maxActiveCars) SpawnCar(car);
            }
            else if(car.phase==CarPhase::Pursuing) MoveCar(car,trafficStep);
            else MoveWreck(car,trafficStep);
        }
        SpawnFrontWave();
        CarContacts();
    }
    for(Car& car:cars)
    {
        if(car.phase!=CarPhase::Waiting) Contact(car);
        if(phase!=Phase::Riding) break;
    }
    if(phase!=Phase::Riding) return;
    if(std::abs(roll)>.95) { Fall(roll); return; }
    if(DistanceToGoal()<6) { CompleteDelivery(); return; }
    if(HasTimeLimit()&&time<=0) phase=Phase::Failed;
}
void Simulation::MoveBike(double dt)
{
    const Vec2 old=bike;
    const double oldHeight=layout->height(old.x,old.y);
    const double pitch=Pose(old,heading,.65,.25).pitch;
    bike=bike+(Forward(heading)*(speed*std::cos(pitch))+shove)*dt;
    for(const Building& b:layout->obstacles)
    {
        const Vec2 push=ObstaclePush(bike,b,.58);
        if(Length(push)>0)
        {
            bike=bike+push;
            const Vec2 n=Normal(push);
            if(Dot(Forward(heading)*speed,n)<0) speed*=.75;
            shove=shove+n*.15;
        }
    }
    bike.x=std::clamp(bike.x,-layout->limit+.7,layout->limit-.7);
    bike.y=std::clamp(bike.y,-layout->limit+.7,layout->limit-.7);
    const double rise=layout->height(bike.x,bike.y)-oldHeight;
    travelled+=std::sqrt(Dot(bike-old,bike-old)+rise*rise);
}
bool Simulation::InBuilding(Vec2 p,double radius) { return DefaultLevelLayout()->Blocked(p,radius); }
bool Simulation::SegmentClear(Vec2 a,Vec2 b,double padding) { return DefaultLevelLayout()->Clear(a,b,padding); }
bool Simulation::SpawnCar(Car& car)
{
    const Vec2 forward=Forward(heading);
    const bool onePass=stage==CampaignStage::FirstChase;
    Vec2 best; double bestScore=std::numeric_limits<double>::max();
    std::vector<Vec2> candidates;
    const auto consider=[&](Vec2 p)
    {
        const Vec2 offset=p-bike; const double d=Length(offset);
        double score;
        if(onePass)
        {
            if(d<34||d>85||Dot(offset,forward)<d*std::cos(Pi/4)||IsBlocked(p,2.3)) return;
            const Vec2 exit=p-Normal(offset)*(d+20);
            if(std::abs(exit.x)>layout->limit-2.3||std::abs(exit.y)>layout->limit-2.3||!RouteClear(p,exit,1.5)) return;
            score=0;
        }
        else
        {
            if(d<55||d>120||Dot(offset,forward)<d*.82||IsBlocked(p,2.3)||!RouteClear(bike,p,1.3)) return;
            score=std::abs(d-(82+car.formationIndex*12))+(1-Dot(Normal(offset),forward))*35;
        }
        for(const Car& other:cars)
            if(other.phase!=CarPhase::Waiting&&Length(other.position-p)<10) return;
        if(onePass) candidates.push_back(p);
        else if(score<bestScore) { bestScore=score; best=p; }
    };
    for(Vec2 point:layout->spawnPoints) consider(point);
    if(onePass)
    {
        if(candidates.empty()) return false;
        best=candidates[std::uniform_int_distribution<std::size_t>(0,candidates.size()-1)(trafficRandom)];
    }
    else if(bestScore==std::numeric_limits<double>::max()) return false;
    car.position=best;
    car.heading=std::atan2(bike.y-best.y,bike.x-best.x);
    if(onePass) car.attackHeading=car.heading;
    car.speed=onePass?Traffic().speed:11; car.phase=CarPhase::Pursuing; car.navigationTimer=0;
    return true;
}
void Simulation::SpawnFrontWave()
{
    if(!frontWavesEnabled) return;
    const bool onePass=stage==CampaignStage::FirstChase;
    const auto traffic=Traffic();
    if(!onePass&&wavesStarted==0)
    {
        // Start the cadence when the first group actually enters the world.
        for(const Car& car:cars) if(car.wave==1&&car.phase==CarPhase::Pursuing)
        {
            wavesStarted=1; nextFrontWaveAt=elapsed+traffic.waveInterval;
            travelAtLastWave=travelled; break;
        }
        return;
    }
    const double minTravel=onePass&&wavesStarted==0?12:traffic.waveTravel;
    if(elapsed<nextFrontWaveAt||elapsed<nextWaveCheckAt
        ||travelled-travelAtLastWave<minTravel) return;
    if(onePass&&pendingFrontWaveSize==0) pendingFrontWaveSize=std::uniform_int_distribution<int>(2,3)(trafficRandom);
    const std::size_t waveSize=onePass?std::size_t(pendingFrontWaveSize):std::size_t(FrontWaveSize);
    if(ActiveCars()+int(waveSize)>traffic.maxActiveCars) return;
    nextWaveCheckAt=elapsed+.25;
    std::array<std::size_t,3> slots{}; std::size_t found=0;
    for(std::size_t i=onePass?0:InitialCarCount;i<cars.size()&&found<waveSize;++i)
        if(cars[i].phase==CarPhase::Waiting&&cars[i].wave==0) slots[found++]=i;
    if(found!=waveSize) return;
    std::array<Car,3> saved{};
    for(std::size_t i=0;i<waveSize;++i) saved[i]=cars[slots[i]];
    for(std::size_t i=0;i<waveSize;++i)
    {
        Car& car=cars[slots[i]]; car.wave=wavesStarted+1; car.formationIndex=int(i);
        car.spawnAt=elapsed;
        if(!SpawnCar(car))
        {
            // Publish a wave only when every member has a clear entry position.
            for(std::size_t j=0;j<waveSize;++j) cars[slots[j]]=saved[j];
            return;
        }
    }
    ++wavesStarted; nextFrontWaveAt=elapsed+traffic.waveInterval; travelAtLastWave=travelled;
    pendingFrontWaveSize=0;
}
Vec2 Simulation::NavigationTarget(const Car& car,Vec2 target) const
{
    if(RouteClear(car.position,target,2.2)) return target;
    const auto& nodes=layout->nodes;
    const auto& edges=layout->edges;
    const std::size_t Count=nodes.size();
    if(Count==0) return car.position;
    std::vector<double> cost(Count,1e12);std::vector<bool> visited(Count,false);
    bool connected=false; std::size_t nearest=0; double nearestDistance=1e12;
    for(std::size_t i=0;i<Count;++i)
    {
        const Vec2 node{nodes[i].x,nodes[i].y};
        const double d=Length(node-target);
        if(d<nearestDistance) { nearest=i; nearestDistance=d; }
        if(RouteClear(node,target,2.2)) { cost[i]=d; connected=true; }
    }
    if(!connected) cost[nearest]=nearestDistance;
    for(std::size_t iteration=0;iteration<Count;++iteration)
    {
        std::size_t current=Count; double best=1e12;
        for(std::size_t i=0;i<Count;++i)
            if(!visited[i]&&cost[i]<best) { best=cost[i]; current=i; }
        if(current==Count) break;
        visited[current]=true;
        for(std::size_t i=0;i<Count;++i)
            cost[i]=std::min(cost[i],cost[current]+edges[current][i]);
    }
    Vec2 best=car.position; double bestCost=1e12;
    for(std::size_t i=0;i<Count;++i)
    {
        const Vec2 node{nodes[i].x,nodes[i].y}, delta=node-car.position;
        const double distance=Length(delta);
        if(distance<2||!RouteClear(car.position,node,2.2)) continue;
        const double turn=std::abs(Wrap(std::atan2(delta.y,delta.x)-car.heading));
        const double value=distance+cost[i]+turn*2;
        if(value<bestCost) { bestCost=value; best=node; }
    }
    return best;
}
void Simulation::BeginRecovery(Car& car)
{
    if(stage==CampaignStage::FirstChase||car.phase!=CarPhase::Pursuing||car.attackMode==AttackMode::Recovering) return;
    car.attackMode=AttackMode::Recovering;
    car.reattackIn=3.4+.1*((car.identity-1)%3);
    car.recoveryAge=car.ramTime=0;
    car.closestApproach=1e9; car.navigationTimer=0;
    car.lastPassAt=elapsed; ++car.passes;
}
void Simulation::MoveCar(Car& car,double dt)
{
    car.contactCooldown=std::max(0.0,car.contactCooldown-dt);
    if(stage==CampaignStage::FirstChase)
    {
        MoveOncomingCar(car,dt);
        return;
    }
    car.navigationTimer-=dt;
    const Vec2 bikeForward=Forward(heading), toBike=bike-car.position;
    const double distance=Length(toBike), behind=Dot(car.position-bike,bikeForward);
    car.closestApproach=std::min(car.closestApproach,distance);
    if(car.attackMode==AttackMode::Oncoming&&distance<18&&car.ramTime==0)
    {
        const Vec2 aim=bike+bikeForward*(std::max(0.0,speed)*.35)-car.position;
        car.attackHeading=std::atan2(aim.y,aim.x); car.ramTime=dt;
    }
    if(car.attackMode==AttackMode::Oncoming&&car.ramTime>0
        &&((behind< -4)||(distance>car.closestApproach+3))) BeginRecovery(car);
    if(car.attackMode==AttackMode::Ramming)
    {
        car.ramTime+=dt;
        if((behind>4&&car.closestApproach<10)||car.ramTime>1.15) BeginRecovery(car);
    }
    if(car.attackMode!=AttackMode::Oncoming)
        car.reattackIn=std::max(0.0,car.reattackIn-dt);
    if(car.attackMode==AttackMode::Recovering)
    {
        car.recoveryAge+=dt;
        if(behind< -7&&Dot(Forward(car.heading),bikeForward)>.8)
        { car.attackMode=AttackMode::Catching; car.navigationTimer=0; }
    }
    if(car.attackMode==AttackMode::Catching&&car.reattackIn<.75&&distance<14
        &&behind<1&&Dot(Forward(car.heading),bikeForward)>.75)
    {
        car.attackMode=AttackMode::Ramming; car.ramTime=0;
        const Vec2 aim=bike+bikeForward*(std::max(0.0,speed)*.35)-car.position;
        car.attackHeading=std::atan2(aim.y,aim.x);
        car.closestApproach=distance; car.lastRamAt=elapsed; ++car.rearAttacks;
    }
    Vec2 target=bike+bikeForward*(std::max(0.0,speed)*std::clamp(distance/25,.15,.6));
    if(car.attackMode==AttackMode::Recovering)
    {
        target=bike+bikeForward*30;
        // A stationary rider cannot overtake a braking car. Drive a small
        // loop to the rear instead of waiting forever for the rider to pass.
        if(std::abs(speed)<3&&behind> -10)
        {
            const Vec2 side{-bikeForward.y,bikeForward.x};
            target=bike-bikeForward*16+side*(car.identity%2?9:-9);
        }
    }
    if(car.navigationTimer<=0)
    { car.waypoint=NavigationTarget(car,target); car.navigationTimer=.12; }
    const Vec2 delta=car.waypoint-car.position;
    double difference=Wrap(std::atan2(delta.y,delta.x)-car.heading);
    double targetSpeed=std::abs(difference)>.65?5.5:Traffic().speed;
    double acceleration=6, turnRate=distance<18?1.6:1.05;
    // Brake for a navigation corner before leaving the faster straight.
    if(Length(car.waypoint-target)>3&&Length(delta)<car.speed*car.speed/12+8)
    { targetSpeed=std::min(targetSpeed,8.0); acceleration=12; }
    if(car.attackMode==AttackMode::Oncoming&&car.ramTime>0)
        difference=Wrap(car.attackHeading-car.heading);
    if(car.attackMode==AttackMode::Recovering)
    {
        // Brake before a rapid U-turn. If already facing the rider's direction,
        // let the rider pass, then accelerate from behind instead of circling.
        acceleration=45;
        targetSpeed=std::abs(speed)<3?9:(Dot(Forward(car.heading),bikeForward)>.5?std::max(0.0,speed-14):4.5);
        turnRate=car.speed>(std::abs(speed)<3?10:6.5)?.45:3.2;
    }
    else if(car.attackMode==AttackMode::Catching)
    {
        // Close a real gap by the next beat; never teleport or respawn the car.
        // Hold a run-up gap until the charge. Otherwise a short recovery can
        // turn into an immediate second hit with no useful dodge interval.
        targetSpeed=car.reattackIn>.75
            ?std::clamp(std::max(0.0,speed)+(distance-13)/std::max(.3,car.reattackIn-.65),4.0,42.0)
            :std::clamp(std::max(0.0,speed)+18,26.0,42.0);
        acceleration=42;
        turnRate=car.speed>12?std::min(1.25,14/car.speed):2.4;
        if(std::abs(difference)>.55) targetSpeed=8;
        if(Length(car.waypoint-target)>3&&Length(delta)<car.speed*car.speed/84+5) targetSpeed=8;
    }
    else if(car.attackMode==AttackMode::Ramming)
    {
        // A short committed charge can be dodged. After a miss it recovers again.
        difference=Wrap(car.attackHeading-car.heading);
        turnRate=2.0; acceleration=42;
        targetSpeed=std::clamp(std::max(0.0,speed)+18,26.0,42.0);
    }
    if(car.attackMode!=AttackMode::Ramming)
    {
        const Vec2 forward=Forward(car.heading),side{-forward.y,forward.x};
        for(const Car& other:cars)
        {
            if(&other==&car||other.phase==CarPhase::Waiting) continue;
            const Vec2 offset=other.position-car.position;
            const double ahead=Dot(offset,forward);
            if(ahead>3&&ahead<5+car.speed*.65&&std::abs(Dot(offset,side))<1.9
                &&Dot(Forward(other.heading),forward)>.5)
            {
                targetSpeed=std::min(targetSpeed,std::max(0.0,other.speed+(ahead-5)*1.2));
                acceleration=std::max(acceleration,35.0);
            }
        }
    }
    const double turn=std::clamp(difference,-turnRate*dt,turnRate*dt);
    car.speed=Approach(car.speed,targetSpeed,dt*acceleration);
    car.heading=Wrap(car.heading+turn);
    // Steering alone never wrecks a car. Real obstacle and vehicle impacts
    // below remain responsible for overturns, including during a fast turn.
    MoveCarForward(car,dt);
}
void Simulation::MoveOncomingCar(Car& car,double dt)
{
    const double oncomingSpeed=Traffic().speed;
    const Vec2 approach=Forward(car.attackHeading);
    const Vec2 toBike=bike-car.position;
    const double distance=Length(toBike);
    const bool pastRider=Dot(toBike,approach)<=0;
    const Vec2 bikeForward=Forward(heading);
    if(car.attackMode==AttackMode::Oncoming&&pastRider) car.attackMode=AttackMode::Leaving;
    if(car.attackMode==AttackMode::Oncoming&&distance<28&&Dot(toBike,approach)>distance*.65&&
        Dot(toBike,bikeForward)<0&&RouteClear(car.position,bike,1.2))
        car.attackMode=AttackMode::Ramming;

    if(car.attackMode==AttackMode::Ramming)
    {
        car.ramTime+=dt;
        // Commit before contact so a late swerve can escape the charge.
        if(distance<14||pastRider||Dot(toBike,bikeForward)>=0||car.ramTime>.85)
            car.attackMode=AttackMode::Leaving;
        else
        {
            const Vec2 riderVelocity=bikeForward*speed;
            const double closing=std::max(5.,oncomingSpeed-Dot(riderVelocity,Normal(toBike)));
            const Vec2 intercept=bike+riderVelocity*std::min(distance/closing,.9);
            if(RouteClear(car.position,intercept,1.2))
            {
                const Vec2 aim=intercept-car.position;
                const double fromLane=Wrap(std::atan2(aim.y,aim.x)-car.attackHeading);
                const double targetHeading=car.attackHeading+std::clamp(fromLane,-.55,.55);
                const double turn=std::clamp(Wrap(targetHeading-car.heading),-1.8*dt,1.8*dt);
                car.heading=Wrap(car.heading+turn);
            }
        }
    }

    car.speed=Approach(car.speed,oncomingSpeed,dt*6);
    MoveCarForward(car,dt);
    const Vec2 offset=car.position-bike;
    if(car.phase==CarPhase::Pursuing&&Length(offset)>24&&
        (Dot(offset,approach)>0||elapsed-car.spawnAt>10))
    {
        const int identity=car.identity;
        car=Car{};
        car.identity=identity;
    }
}
void Simulation::MoveCarForward(Car& car,double dt)
{
    Vec2 next=car.position+Forward(car.heading)*(car.speed*dt);
    bool blocked=false;
    for(const Building& b:layout->obstacles)
    {
        Vec2 normal; double depth=0;
        if(BoxContact(next,Forward(car.heading),CarLength,CarWidth,{b.x,b.y},Forward(b.yaw),b.hx,b.hy,normal,depth))
        {
            next=next+normal*(depth+.005); blocked=true;
            const double impact=std::max(0.0,-Dot(Forward(car.heading)*car.speed,normal));
            if(impact>8)
            {
                car.position=next;
                WreckCar(car,CrashCause::Building,Dot(normal,Vec2{-std::sin(car.heading),std::cos(car.heading)}));
                return;
            }
        }
    }
    if(blocked) { car.speed=std::min(car.speed,3.0); car.navigationTimer=0; }
    next.x=std::clamp(next.x,-layout->limit+2.3,layout->limit-2.3);
    next.y=std::clamp(next.y,-layout->limit+2.3,layout->limit-2.3);
    car.position=next;
}
void Simulation::WreckCar(Car& car,CrashCause cause,double direction)
{
    if(car.phase!=CarPhase::Pursuing) return;
    car.phase=CarPhase::Wrecked; car.crashCause=cause;
    car.rollDirection=direction<0?-1:1; car.wreckTimer=0;
    car.speed=std::min(car.speed*.25,3.0);
}
void Simulation::MoveWreck(Car& car,double dt)
{
    car.wreckTimer+=dt;
    car.contactCooldown=std::max(0.0,car.contactCooldown-dt);
    car.roll=Approach(car.roll,car.rollDirection*Pi*.52,dt*2.6);
    car.speed=Approach(car.speed,0,dt*14);
    Vec2 next=car.position+Forward(car.heading)*(car.speed*dt);
    for(const Building& b:layout->obstacles)
    {
        Vec2 normal; double depth=0;
        if(BoxContact(next,Forward(car.heading),CarLength,CarWidth,{b.x,b.y},Forward(b.yaw),b.hx,b.hy,normal,depth))
        { next=next+normal*(depth+.005); car.speed=0; }
    }
    next.x=std::clamp(next.x,-layout->limit+2.3,layout->limit-2.3);
    next.y=std::clamp(next.y,-layout->limit+2.3,layout->limit-2.3);
    car.position=next;
}
void Simulation::CarContacts()
{
    for(std::size_t i=0;i<cars.size();++i) for(std::size_t j=i+1;j<cars.size();++j)
    {
        Car& a=cars[i]; Car& b=cars[j];
        if(a.phase==CarPhase::Waiting||b.phase==CarPhase::Waiting) continue;
        const bool aActive=a.phase==CarPhase::Pursuing, bActive=b.phase==CarPhase::Pursuing;
        if(!aActive&&!bActive) continue;
        Vec2 normal; double depth=0;
        if(!BoxContact(a.position,Forward(a.heading),CarLength,CarWidth,
                       b.position,Forward(b.heading),CarLength,CarWidth,normal,depth)) continue;
        const Vec2 av=Forward(a.heading)*a.speed, bv=Forward(b.heading)*b.speed;
        const double impact=std::max(0.0,-Dot(av-bv,normal));
        // A stopped wreck is solid. It never resumes pursuit or gets pushed
        // back into the active fleet by another collision.
        const double aShare=aActive?(bActive?.5:1):0;
        if(aActive) a.position=a.position+normal*((depth+.01)*aShare);
        if(bActive) b.position=b.position-normal*((depth+.01)*(1-aShare));
        if(impact>10)
        {
            if(aActive) WreckCar(a,CrashCause::Car,Dot(normal,Vec2{-std::sin(a.heading),std::cos(a.heading)}));
            if(bActive) WreckCar(b,CrashCause::Car,-Dot(normal,Vec2{-std::sin(b.heading),std::cos(b.heading)}));
        }
        else if(impact>0)
        {
            if(aActive) { a.speed=std::max(0.0,a.speed-impact*.5); a.navigationTimer=0; }
            if(bActive) { b.speed=std::max(0.0,b.speed-impact*.5); b.navigationTimer=0; }
        }
    }
}
void Simulation::Contact(Car& car)
{
    const Vec2 forward=Forward(car.heading), side{-forward.y,forward.x};
    const Vec2 relative=bike-car.position;
    const double along=Dot(relative,forward), across=Dot(relative,side);
    const double localX=std::clamp(along,-2.1,2.1), localY=std::clamp(across,-.92,.92);
    const Vec2 closest=car.position+forward*localX+side*localY;
    const Vec2 offset=bike-closest;
    const double distance=Length(offset), radius=.56;
    if(distance>radius+.2) { car.touching=false; return; }
    if(distance>=radius) return;
    const Vec2 normal=distance>1e-6?offset*(1/distance):side*(across>=0?1:-1);
    const double penetration=radius-distance;
    bike=bike+normal*(penetration+.025);
    if(car.touching||car.contactCooldown>0) return;
    const bool frozen=TrafficFrozen();
    const Vec2 relativeVelocity=Forward(heading)*speed+shove-forward*(frozen?0:car.speed);
    const double closing=std::max(0.0,-Dot(relativeVelocity,normal));
    if(frozen&&closing<=1e-6) return;
    car.touching=true; car.contactCooldown=.75; ++car.contacts;
    const bool centered=std::abs(across)<.78;
    if((centered&&closing>6)||(penetration>.25&&closing>8))
    {
        Fall(Dot(normal,Vec2{-std::sin(heading),std::cos(heading)})); return;
    }
    ++grazes; hitFlash=.3;
    const Vec2 bikeSide{-std::sin(heading),std::cos(heading)};
    const double sign=Dot(normal,bikeSide)>=0?1:-1;
    rollVelocity+=sign*std::clamp(.7+closing*.045,.7,1.65);
    shove=shove+normal*std::clamp(1.2+closing*.12,1.2,3.0);
    speed*=.8;
    if(!frozen) { car.speed*=.82; BeginRecovery(car); }
}
double Simulation::DistanceToGoal() const { return Length(layout->goal-bike); }
int Simulation::ActiveCars() const
{
    int count=0; for(const Car& car:cars) if(car.phase==CarPhase::Pursuing) ++count;
    return count;
}
int Simulation::WreckedCars() const
{
    int count=0; for(const Car& car:cars) if(car.phase==CarPhase::Wrecked) ++count;
    return count;
}
}
