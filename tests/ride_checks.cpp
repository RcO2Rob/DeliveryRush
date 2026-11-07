#include "RideSimulation.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace Delivery;
constexpr double Pi=3.14159265358979323846;
int Checks=0;
void Require(bool ok,const char* message)
{
    if(!ok) { std::cerr<<"FAIL: "<<message<<'\n'; std::exit(1); }
    ++Checks; std::cout<<"PASS: "<<message<<'\n';
}
double Distance(Vec2 a,Vec2 b) { return std::hypot(a.x-b.x,a.y-b.y); }
void NoSpawn(Simulation& s) { s.frontWavesEnabled=false;for(Car& c:s.cars) c.spawnAt=1e9; }
void Run(Simulation& s,double seconds,Input input={},double frame=1.0/60)
{ for(double left=seconds;left>1e-8;left-=frame) s.Tick(std::min(left,frame),input); }
Simulation Started() { Simulation s;s.Start();NoSpawn(s);return s; }
Input DriveTo(const Simulation& s,Vec2 goal)
{
    const double angle=std::atan2(goal.y-s.bike.y,goal.x-s.bike.x);
    const double error=std::remainder(angle-s.heading,2*Pi);
    return {1,std::clamp(error*2.7,-1.0,1.0),std::abs(error)>.35&&s.speed>6};
}
Simulation Route(bool northFirst,double frame=1.0/60)
{
    Simulation s=Started();
    const Vec2 corner=northFirst?Vec2{StartX,DestinationY}:Vec2{DestinationX,StartY};
    bool turned=false;
    for(int i=0;i<int(100/frame)&&s.phase==Phase::Riding;++i)
    {
        if(!turned&&Distance(s.bike,corner)<11) turned=true;
        s.Tick(frame,DriveTo(s,turned?Vec2{DestinationX,DestinationY}:corner));
    }
    return s;
}
struct ChaseResult
{
    bool survived=true,continuous=true,fromBehind=true;
    int approaches=0; double minimumInterval=1e9,maximumInterval=0;
};
ChaseResult RepeatChase(double frame)
{
    auto s=Started();s.bike={-140,-180};s.speed=Simulation::BikeMaxSpeed;
    Car& c=s.cars[0];c.phase=CarPhase::Pursuing;c.position=s.bike+Vec2{-6,-3.5};c.heading=Pi;c.speed=14.2;
    AttackMode previous=c.attackMode;double dodge=0,sign=1;int observed=0;
    ChaseResult result;
    for(double t=0;t<18.5&&s.phase==Phase::Riding;t+=frame)
    {
        if(c.attackMode==AttackMode::Ramming&&previous!=AttackMode::Ramming)
        { dodge=.65; sign=s.bike.y> -179?-1:1; }
        previous=c.attackMode;
        Input input{1,dodge>0?(dodge>.325?sign:-sign):0,false};dodge-=frame;
        const Vec2 old=c.position;
        s.Tick(frame,input);
        result.continuous=result.continuous&&Distance(old,c.position)<=42*frame+.01&&c.identity==1;
        if(c.attackMode==AttackMode::Ramming&&c.rearAttacks>observed&&Distance(c.position,s.bike)<6)
        {
            observed=c.rearAttacks;++result.approaches;
            const double interval=s.elapsed-c.lastPassAt;
            result.minimumInterval=std::min(result.minimumInterval,interval);
            result.maximumInterval=std::max(result.maximumInterval,interval);
            const Vec2 relative=c.position-s.bike,forward=Simulation::Forward(s.heading);
            result.fromBehind=result.fromBehind&&relative.x*forward.x+relative.y*forward.y<0;
        }
    }
    result.survived=s.phase==Phase::Riding&&s.falls==0&&c.phase==CarPhase::Pursuing;
    return result;
}
Simulation WaveReady()
{
    auto s=Started();s.frontWavesEnabled=true;s.wavesStarted=1;
    s.elapsed=8;s.time=92;s.travelled=80;s.nextFrontWaveAt=8;
    s.bike={-60,-180};s.speed=Simulation::BikeMaxSpeed;
    auto& rear=s.cars[0];rear.phase=CarPhase::Pursuing;rear.position={-100,-180};
    rear.heading=0;rear.speed=Simulation::BikeMaxSpeed;rear.attackMode=AttackMode::Catching;rear.reattackIn=20;
    return s;
}
int main()
{
    double highest=0,steepest=0;
    for(double x=-208;x<=208;x+=2) for(double y=-208;y<=208;y+=2)
    {
        highest=std::max(highest,GroundHeight(x,y));
        steepest=std::max(steepest,std::hypot((GroundHeight(x+1,y)-GroundHeight(x-1,y))/2,
            (GroundHeight(x,y+1)-GroundHeight(x,y-1))/2));
    }
    Require(highest>11&&highest<18,"The playable ground has visible rolling hills rather than a flat grass plane");
    Require(steepest>.15&&steepest<.5,"Hills have traversable gradients without vertical steps");
    const auto uphill=SurfacePose(130,35,Pi/2,.65,.25),downhill=SurfacePose(130,35,-Pi/2,.65,.25);
    Require(uphill.height>4&&uphill.pitch>.1&&downhill.pitch<-.1,"Vehicle poses rise and pitch in the correct direction on a slope");
    Require(SurfacePose(130,35,0,.65,.25).bank>.1,"Cross-slope riding also banks with the ground");
    Require(GroundHeight(StartX,StartY)==0&&GroundHeight(DestinationX,DestinationY)==0,"Start and delivery areas remain level");
    double seamJump=0;
    for(double y=-180;y<=180;y+=3)
        seamJump=std::max(seamJump,std::abs(GroundHeight(-.0001,y)-GroundHeight(.0001,y)));
    Require(seamJump<.001,"City and park ground meet continuously at their boundary");
    auto slopeRide=Started();slopeRide.bike={130,35};slopeRide.heading=Pi/2;
    const auto slopeStart=slopeRide.bike;const double initialHeight=GroundHeight(130,35);
    Run(slopeRide,1,{1,0,false});
    Require(GroundHeight(slopeRide.bike.x,slopeRide.bike.y)>initialHeight+1&&slopeRide.phase==Phase::Riding,
        "Normal bicycle input can climb a real hill");
    Require(slopeRide.travelled>Distance(slopeStart,slopeRide.bike)+.1,"Riding progress measures the sloping surface distance");
    auto responsive=Started();Run(responsive,1,{1,0,false});
    Require(responsive.speed>9.9&&responsive.speed<10.1,"Pedalling builds speed faster during the first second");
    Run(responsive,4,{1,0,false});
    Require(std::abs(responsive.speed-20)<.001&&responsive.bike.x>StartX+76,"The higher top speed covers more ground within five seconds");
    Run(responsive,1.5,{0,0,true});
    Require(responsive.speed==0,"Brakes can still stop the faster bicycle promptly");
    auto cityCruise=Started();cityCruise.bike={-50,-180};cityCruise.speed=0;
    auto& cruisingCar=cityCruise.cars[0];
    cruisingCar.phase=CarPhase::Pursuing;cruisingCar.position={-200,-180};cruisingCar.heading=0;cruisingCar.speed=11;
    Run(cityCruise,3,{0,0,true});
    Require(std::abs(cruisingCar.speed-21)<.001&&cruisingCar.position.x> -149&&
        cruisingCar.phase==CarPhase::Pursuing&&cityCruise.phase==Phase::Riding,
        "Final-stage cars accelerate to a faster straight cruising speed on a clear road");
    auto sensitive=Started();sensitive.bike={0,0};sensitive.speed=5;Run(sensitive,1,{0,1,false});
    Require(sensitive.heading>1.5&&sensitive.heading<1.6,"The same steering input produces a more responsive turn");
    Simulation ready;Run(ready,10,{1,1,false});
    Require(ready.phase==Phase::Ready&&ready.time==100&&ready.bike.x==StartX,"Ready screen freezes the clock and position");
    auto free=Started();Run(free,5,{1,0,false});
    Require(free.speed>15&&free.bike.x>StartX+40,"Pedalling accelerates through the city");
    free.bike={0,0};free.heading=0;free.speed=5;Run(free,1,{1,1,false});
    Require(std::abs(free.heading)>.8,"Steering changes the bicycle's world heading");
    Require(free.bike.y>3,"Steering can leave the original east/west street");
    Require(free.bike.x>0&&free.bike.x<12,"Turning does not force constant forward progress along X");
    auto reverse=Started();Run(reverse,3,{0,0,true});Run(reverse,3,{-1,0,false});
    Require(reverse.speed<-3,"S can reverse after braking to a stop");
    auto coast=Started();Run(coast,7);
    Require(coast.speed==0,"Coasting can stop without forced auto-scrolling");

    auto pause=Started();Run(pause,2,{1,0,false});pause.TogglePause();
    const Vec2 before=pause.bike;const double beforeTime=pause.time;
    Run(pause,8,{1,1,false});
    Require(pause.bike.x==before.x&&pause.bike.y==before.y&&pause.time==beforeTime,"Pause freezes free movement and the deadline");
    pause.TogglePause();Run(pause,.2,{1,0,false});
    Require(pause.bike.x>before.x,"Resume preserves the current delivery");

    const auto& building=CityBuildings[0];
    auto wall=Started();wall.bike={building.x-building.hx-3,building.y};wall.heading=0;wall.speed=15;
    Run(wall,1,{1,0,false});
    Require(!Simulation::InBuilding(wall.bike),"Buildings block the bicycle even at full speed");
    Require(wall.bike.x<building.x-building.hx,"The bicycle cannot tunnel through a building");
    Require(!Simulation::SegmentClear({building.x-30,building.y},{building.x+30,building.y}),"Navigation recognises a building across a route");
    Require(Simulation::SegmentClear({-180,-180},{180,-180}),"Main streets remain connected");
    Require(Simulation::SegmentClear({-135,-180},{-135,-90}),"The block alley is a genuinely traversable alternate route");

    Simulation arrival;arrival.Start();
    while(arrival.ActiveCars()==0&&arrival.elapsed<12) arrival.Tick(1.0/60,{1,0,false});
    Require(arrival.ActiveCars()==3,"The initial group contains three attackers without a warning phase");
    const Car spawned=arrival.cars[0];const Vec2 to=spawned.position-arrival.bike,forward=Simulation::Forward(arrival.heading);
    Require(to.x*forward.x+to.y*forward.y>.8*Distance(spawned.position,arrival.bike),"New attackers originate in front of the rider");
    Require(Distance(spawned.position,arrival.bike)>55,"An attacker starts far enough ahead to be seen");
    Require(Simulation::SegmentClear(arrival.bike,spawned.position,1.3),"A new attacker is not hidden behind a building");
    bool groupVisible=true,groupSeparated=true;
    for(std::size_t i=0;i<arrival.cars.size();++i)
    {
        if(arrival.cars[i].phase!=CarPhase::Pursuing) continue;
        const Vec2 offset=arrival.cars[i].position-arrival.bike;
        groupVisible=groupVisible&&offset.x*forward.x+offset.y*forward.y>.8*Distance(arrival.cars[i].position,arrival.bike)
            &&Simulation::SegmentClear(arrival.bike,arrival.cars[i].position,1.3);
        for(std::size_t j=0;j<i;++j)
            groupSeparated=groupSeparated&&Distance(arrival.cars[i].position,arrival.cars[j].position)>=9.5;
    }
    Require(groupVisible&&groupSeparated,"All three cars enter on visible roads ahead without overlapping");
    Require(arrival.wavesStarted==1&&arrival.nextFrontWaveAt>arrival.elapsed+7.9,
        "The next wave clock starts when the initial group actually appears");

    auto mixed=WaveReady();const Vec2 oldRear=mixed.cars[0].position;
    mixed.Tick(1.0/120,{1,0,false});
    Require(mixed.wavesStarted==2&&mixed.ActiveCars()==3,"A fresh two-car front wave joins an existing rear pursuer");
    bool ahead=true;
    const Vec2 mixedForward=Simulation::Forward(mixed.heading);
    for(std::size_t i=3;i<5;++i)
    {
        const auto& car=mixed.cars[i];const Vec2 offset=car.position-mixed.bike;
        ahead=ahead&&car.wave==2&&car.attackMode==AttackMode::Oncoming
            &&offset.x*mixedForward.x+offset.y*mixedForward.y>.82*Distance(car.position,mixed.bike)
            &&Simulation::SegmentClear(mixed.bike,car.position,1.3)&&!Simulation::InBuilding(car.position,2.3);
    }
    Require(ahead&&Distance(mixed.cars[3].position,mixed.cars[4].position)>=10,
        "New-wave cars face the rider from clear, separated positions ahead");
    Require(mixed.cars[0].identity==1&&mixed.cars[0].phase==CarPhase::Pursuing
        &&mixed.cars[0].position.x<mixed.bike.x&&Distance(oldRear,mixed.cars[0].position)<.36,
        "The old car continues behind the rider without being reset or teleported when a front wave appears");
    Run(mixed,.5,{1,0,false});
    Require(mixed.wavesStarted==2&&mixed.ActiveCars()==3,"A new front wave and the old rear pursuer remain active together");
    auto early=WaveReady();early.nextFrontWaveAt=10;Run(early,.5,{1,0,false});
    Require(early.wavesStarted==1,"A new wave cannot arrive before its interval has elapsed");
    auto progress=WaveReady();progress.travelled=0;progress.speed=0;
    progress.Tick(1.0/120,{0,0,true});
    Require(progress.wavesStarted==1,"Waiting in place does not repeatedly create front waves");
    progress.travelled=39;progress.speed=Simulation::BikeMaxSpeed;Run(progress,.1,{1,0,false});
    Require(progress.wavesStarted==2,"Forward riding can release a due wave after the progress gate is met");

    auto blocked=WaveReady();blocked.bike={-135,-135};blocked.speed=0;
    blocked.Tick(1.0/120,{0,0,true});
    Require(blocked.wavesStarted==1&&blocked.cars[3].wave==0&&blocked.cars[4].wave==0,
        "A building blocking the front view defers the complete wave without leaving partial spawns");
    auto edge=WaveReady();edge.bike={195,-180};edge.speed=0;
    edge.Tick(1.0/120,{0,0,true});
    Require(edge.wavesStarted==1,"The world edge cannot force attackers to appear behind the rider");
    edge.heading=Pi;Run(edge,.3,{0,0,true});
    Require(edge.wavesStarted==2&&edge.cars[3].position.x<edge.bike.x,
        "A deferred wave uses the rider's new forward direction after a turn");

    auto capped=WaveReady();capped.bike={-60,0};capped.speed=0;
    for(int i=0;i<Simulation::MaxActiveCars;++i)
    {
        auto& car=capped.cars[i];car.phase=CarPhase::Pursuing;car.wave=1;
        car.position={-180+double(i)*20,-180};car.heading=0;car.speed=0;
        car.waypoint=car.position+Vec2{40,0};car.navigationTimer=10;
    }
    capped.Tick(1.0/120,{0,0,true});
    Require(capped.ActiveCars()==Simulation::MaxActiveCars&&capped.wavesStarted==1,
        "Front waves wait while the simultaneous pursuit limit is full");
    capped.cars[0].phase=CarPhase::Wrecked;capped.cars[1].phase=CarPhase::Wrecked;
    capped.Tick(1.0/120,{0,0,true});
    Require(capped.ActiveCars()==Simulation::MaxActiveCars&&capped.wavesStarted==2
        &&capped.cars[0].phase==CarPhase::Wrecked&&capped.cars[1].phase==CarPhase::Wrecked
        &&capped.cars[8].identity==9&&capped.cars[9].identity==10,
        "A later wave uses new cars while old wrecks remain permanently out");

    auto lateCar=Started();lateCar.bike={-140,-180};lateCar.speed=Simulation::BikeMaxSpeed;
    auto& late=lateCar.cars[31];late.phase=CarPhase::Pursuing;late.wave=12;
    late.position=lateCar.bike+Vec2{-6,-3.5};late.heading=Pi;late.speed=14.2;
    lateCar.Tick(1.0/120,{1,0,false});
    Require(late.attackMode==AttackMode::Recovering&&late.reattackIn>3.3&&late.reattackIn<3.7,
        "Late-wave identities retain the same three-to-four-second rear recovery cadence");
    auto wavePause=WaveReady();wavePause.TogglePause();Run(wavePause,20,{1,0,false});
    Require(wavePause.wavesStarted==1&&wavePause.elapsed==8&&wavePause.travelled==80,
        "Pause freezes both front-wave timing and its riding-progress requirement");
    wavePause.Restart();
    Require(wavePause.wavesStarted==0&&wavePause.ActiveCars()==0&&wavePause.WreckedCars()==0
        &&wavePause.cars[3].wave==0,"A new attempt resets future wave slots as well as existing pursuers");
    auto mixedLow=WaveReady();mixedLow.Tick(1.0/20,{1,0,false});
    Require(mixedLow.wavesStarted==2&&mixedLow.cars[3].phase==CarPhase::Pursuing
        &&mixedLow.cars[0].phase==CarPhase::Pursuing,"Front and rear pursuit also coexist at 20 FPS");
    Simulation firstHit;firstHit.Start();
    while(firstHit.phase==Phase::Riding&&firstHit.elapsed<9) firstHit.Tick(1.0/120,{1,0,false});
    std::cout<<"FIRST_COLLISION_SECONDS: "<<firstHit.elapsed<<'\n';
    Require(firstHit.phase==Phase::Fallen&&firstHit.elapsed>=4.5&&firstHit.elapsed<=6.5,
        "A straight accelerating rider meets the first collision within 4.5 to 6.5 seconds");

    auto head=Started();head.speed=12;
    Car& h=head.cars[0];h.phase=CarPhase::Pursuing;h.position=head.bike+Vec2{2.55,0};h.heading=Pi;h.speed=12;
    head.Tick(1.0/120,{1,0,false});
    Require(head.phase==Phase::Fallen&&head.falls==1,"A solid oncoming collision knocks the bicycle down");
    Run(head,.45);
    Require(head.phase==Phase::Fallen&&std::abs(head.roll)>1,"The fall is visibly animated before restarting");
    Run(head,.8);
    Require(head.phase==Phase::Riding&&head.attempt==2,"A fall automatically starts a new attempt");
    Require(Distance(head.bike,{StartX,StartY})<2&&head.time>99.7,"Automatic restart resets position and the complete deadline");
    Require(head.ActiveCars()==0,"A restarted attempt resets all pursuers");

    auto brush=Started();brush.speed=12;
    Car& g=brush.cars[0];g.phase=CarPhase::Pursuing;g.position=brush.bike+Vec2{2.5,1.38};g.heading=Pi;g.speed=12;
    brush.Tick(1.0/120,{1,0,false});
    Require(brush.phase==Phase::Riding&&brush.grazes==1&&brush.falls==0,"An off-centre brush can leave the bicycle upright");
    Require(brush.speed>0&&brush.speed<12,"A brush slows the bicycle without stopping the delivery");
    Require(100-brush.time<.02,"A brush has no arbitrary time penalty");
    Require(std::hypot(brush.shove.x,brush.shove.y)>0&&std::abs(brush.rollVelocity)>0,"A brush physically displaces and wobbles the bicycle");
    Require(g.phase==CarPhase::Pursuing&&g.identity==1,"The same car remains active after contact");

    auto chase=Started();chase.bike={0,-180};chase.speed=0;
    Car& c=chase.cars[0];c.phase=CarPhase::Pursuing;c.position={-100,-180};c.heading=Pi;c.speed=10;c.contacts=1;
    Run(chase,6,{0,0,true});
    Require(c.phase==CarPhase::Pursuing&&c.identity==1&&c.contacts==1,"A missed/past car never disappears or gets replaced");
    Require(std::cos(c.heading)>.5&&c.position.x>-100,"A passed car turns around and resumes pursuit");

    auto navigate=Started();navigate.bike={0,0};navigate.speed=0;
    Car& n=navigate.cars[0];n.phase=CarPhase::Pursuing;n.position={-90,-180};n.heading=0;n.speed=10;
    bool clear=true;
    for(int i=0;i<60*12;++i) { navigate.Tick(1.0/60,{0,0,true});clear=clear&&!Simulation::InBuilding(n.position,.9); }
    Require(clear,"A pursuer routes around buildings instead of driving through them");
    Require(Distance(n.position,navigate.bike)<150,"A pursuer makes progress through the street network");

    const auto repeated=RepeatChase(1.0/120),repeatedLow=RepeatChase(1.0/20);
    Require(repeated.survived&&repeated.approaches>=5,"The same car repeatedly catches a full-speed dodging rider for at least five attacks");
    Require(repeated.minimumInterval>=2.8&&repeated.maximumInterval<=4,"Each rear approach reaches within six metres about three to four seconds after the previous miss");
    Require(repeated.fromBehind&&repeated.continuous,"Repeated attacks approach from behind without teleportation or car replacement");
    Require(repeatedLow.survived&&repeatedLow.approaches>=5&&repeatedLow.minimumInterval>=2.8&&repeatedLow.maximumInterval<=4,
        "Repeated three-to-four-second rear approaches also work at 20 FPS");
    std::cout<<"REPEAT_CHASE_METRICS: 120Hz approaches="<<repeated.approaches
        <<" gap_seconds="<<repeated.minimumInterval<<".."<<repeated.maximumInterval
        <<"; 20FPS approaches="<<repeatedLow.approaches
        <<" gap_seconds="<<repeatedLow.minimumInterval<<".."<<repeatedLow.maximumInterval<<'\n';
    auto rearHit=Started();rearHit.bike={-140,-180};rearHit.speed=Simulation::BikeMaxSpeed;
    auto& rh=rearHit.cars[0];rh.phase=CarPhase::Pursuing;rh.position=rearHit.bike+Vec2{-6,-3.5};rh.heading=Pi;rh.speed=14.2;
    while(rearHit.phase==Phase::Riding&&rearHit.elapsed<5) rearHit.Tick(1.0/120,{1,0,false});
    Require(rearHit.phase==Phase::Fallen&&rearHit.elapsed>=2.8&&rearHit.elapsed<=4,"A rear attacker can actually hit a full-speed rider who does not dodge");
    auto chasePause=Started();chasePause.bike={-140,-180};chasePause.speed=Simulation::BikeMaxSpeed;
    auto& cp=chasePause.cars[0];cp.phase=CarPhase::Pursuing;cp.position=chasePause.bike+Vec2{-6,-3.5};cp.heading=Pi;cp.speed=14.2;
    Run(chasePause,.5,{1,0,false});chasePause.TogglePause();const double remaining=cp.reattackIn;
    Run(chasePause,5,{1,0,false});
    Require(cp.reattackIn==remaining,"Pause freezes the repeat-attack countdown too");
    auto stopped=Started();stopped.bike={0,-180};stopped.speed=0;
    auto& sc=stopped.cars[0];sc.phase=CarPhase::Pursuing;sc.position={6,-180};sc.heading=0;sc.speed=14;
    sc.attackMode=AttackMode::Recovering;sc.reattackIn=3.4;
    while(stopped.phase==Phase::Riding&&stopped.elapsed<9&&sc.rearAttacks==0) stopped.Tick(1.0/120,{0,0,true});
    Require(sc.rearAttacks>0||stopped.phase==Phase::Fallen,"Stopping the bicycle cannot leave a recovering pursuer waiting forever");

    Require(ParkMinX==0&&CityBuildings.size()==17&&ParkObstacles.size()>20,"The new layout dedicates half the world to a park with solid trees and rocks");
    bool clearPaths=true;
    for(const auto& p:ParkPathSegments) clearPaths=clearPaths&&Simulation::SegmentClear({p.ax,p.ay},{p.bx,p.by},2.2);
    Require(clearPaths,"All authored winding park paths have collision-clear connections");
    const auto& tree=ParkObstacles[0];
    auto parkWall=Started();parkWall.bike={tree.x-tree.hx-3,tree.y};parkWall.speed=15;parkWall.heading=0;
    Run(parkWall,.8,{1,0,false});
    Require(!Simulation::InBuilding(parkWall.bike)&&parkWall.bike.x<tree.x,"Park tree trunks physically block the rider");
    auto parkRoute=Started();parkRoute.bike={-180,0};std::size_t next=0;
    const Vec2 route[]={{0,0},{40,20},{75,-5},{115,30},{150,70},{180,90},{180,180}};
    for(int i=0;i<60*90&&parkRoute.phase==Phase::Riding;++i)
    {
        if(next+1<std::size(route)&&Distance(parkRoute.bike,route[next])<8) ++next;
        parkRoute.Tick(1.0/60,DriveTo(parkRoute,route[next]));
    }
    Require(parkRoute.phase==Phase::Delivered,"A route from city streets through winding park paths can reach the clinic");

    auto buildingCrash=Started();buildingCrash.bike={0,0};buildingCrash.speed=0;
    Car& bc=buildingCrash.cars[0];bc.phase=CarPhase::Pursuing;
    bc.position={building.x-building.hx-2.15,building.y};bc.heading=0;bc.speed=14;
    bc.waypoint=bc.position+Vec2{30,0};bc.navigationTimer=2;
    for(std::size_t i=1;i<Simulation::InitialCarCount;++i)
    {
        Car& survivor=buildingCrash.cars[i];survivor.phase=CarPhase::Pursuing;
        survivor.position={-90+double(i)*60,-180};survivor.heading=0;survivor.speed=5;
        survivor.waypoint=survivor.position+Vec2{40,0};survivor.navigationTimer=20;
    }
    buildingCrash.Tick(1.0/120,{0,0,true});
    Require(bc.phase==CarPhase::Wrecked&&bc.crashCause==CrashCause::Building,
        "A fast direct collision with a building overturns the enemy car");
    Require(buildingCrash.ActiveCars()==2&&buildingCrash.WreckedCars()==1,
        "One overturned car reduces the initial group from three pursuers to two");
    Run(buildingCrash,1,{0,0,true});
    Require(std::abs(bc.roll)>1.5&&bc.speed==0,"An overturned car visibly rolls onto its side and comes to rest");
    const Vec2 wreckPosition=bc.position;const int wreckIdentity=bc.identity;
    Run(buildingCrash,6,{0,0,true});
    Require(bc.phase==CarPhase::Wrecked&&bc.identity==wreckIdentity&&Distance(bc.position,wreckPosition)<.001,
        "The same wreck stays in the scene and never resumes pursuit");
    Require(buildingCrash.ActiveCars()<=2,"No replacement is added after an enemy overturns");

    auto slowWall=Started();slowWall.bike={0,0};slowWall.speed=0;
    Car& sw=slowWall.cars[0];sw.phase=CarPhase::Pursuing;
    sw.position={building.x-building.hx-2.11,building.y};sw.heading=0;sw.speed=3;
    sw.waypoint=sw.position+Vec2{30,0};sw.navigationTimer=2;
    Run(slowWall,.05,{0,0,true});
    Require(sw.phase==CarPhase::Pursuing,"A low-speed wall bump does not automatically overturn a car");

    auto sharp=Started();sharp.bike={0,-180};sharp.speed=0;
    Car& st=sharp.cars[0];st.phase=CarPhase::Pursuing;st.position={6,-180};st.heading=Pi/2;st.speed=14.2;
    Run(sharp,.4,{0,0,true});
    Require(st.phase==CarPhase::Pursuing&&st.crashCause==CrashCause::None&&st.roll==0,
        "A fast emergency turn without contact stays upright instead of causing a spontaneous wreck");
    auto slowTurn=Started();slowTurn.bike={0,-180};slowTurn.speed=0;
    Car& sl=slowTurn.cars[0];sl.phase=CarPhase::Pursuing;sl.position={6,-180};sl.heading=Pi/2;sl.speed=5;
    Run(slowTurn,.6,{0,0,true});
    Require(sl.phase==CarPhase::Pursuing&&sl.crashCause==CrashCause::None,
        "The same emergency turn at low speed remains upright");

    auto traffic=Started();traffic.bike={0,0};traffic.speed=0;
    for(int i=0;i<2;++i)
    {
        Car& t=traffic.cars[i];t.phase=CarPhase::Pursuing;
        t.position={i==0?-2.15:2.15,-180};t.heading=i==0?0:Pi;t.speed=14;
        t.waypoint=t.position+Simulation::Forward(t.heading)*50;t.navigationTimer=2;
    }
    traffic.Tick(1.0/120,{0,0,true});
    Require(traffic.cars[0].phase==CarPhase::Wrecked&&traffic.cars[1].phase==CarPhase::Wrecked
        &&traffic.cars[0].crashCause==CrashCause::Car&&traffic.cars[1].crashCause==CrashCause::Car,
        "A heavy head-on collision between enemies can overturn both cars");
    Require(traffic.trafficImpact.speed>10&&Distance(traffic.trafficImpact.position,{0,-180})<.1,
        "A car-to-car crash supplies one impact at the collision location");
    traffic.Tick(1.0/120,{0,0,true});
    Require(traffic.trafficImpact.speed==0,"The collision sound event clears on the next frame");
    auto slowTraffic=Started();slowTraffic.bike={0,0};slowTraffic.speed=0;
    for(int i=0;i<2;++i)
    {
        Car& t=slowTraffic.cars[i];t.phase=CarPhase::Pursuing;
        t.position={i==0?-2.105:2.105,-180};t.heading=i==0?0:Pi;t.speed=2;
        t.waypoint=t.position+Simulation::Forward(t.heading)*50;t.navigationTimer=2;
    }
    slowTraffic.Tick(1.0/120,{0,0,true});
    Require(slowTraffic.ActiveCars()==2&&slowTraffic.WreckedCars()==0&&slowTraffic.trafficImpact.speed>1.5,
        "A car-to-car bump can play an impact sound without overturning either car");

    auto allGone=Started();allGone.bike={0,0};allGone.speed=0;
    for(std::size_t i=0;i<Simulation::InitialCarCount;++i)
    {
        Car& w=allGone.cars[i];w.phase=CarPhase::Wrecked;w.position={double(i)*12-90,-180};
        w.crashCause=CrashCause::Building;w.spawnAt=0;
    }
    Run(allGone,20,{0,0,true});
    Require(allGone.phase==Phase::Riding&&allGone.ActiveCars()==0&&allGone.WreckedCars()==3,
        "With future waves disabled, eliminated enemies stay out while the delivery continues");
    Require(allGone.time>79.9&&allGone.time<80.1,"Enemy elimination neither completes the delivery nor resets its clock");
    allGone.TogglePause();const double pausedRoll=allGone.cars[0].roll,pausedWreckTime=allGone.cars[0].wreckTimer;
    Run(allGone,3);
    Require(allGone.cars[0].roll==pausedRoll&&allGone.cars[0].wreckTimer==pausedWreckTime,"Pause also freezes enemy wreck animation");
    allGone.Restart();
    Require(allGone.WreckedCars()==0&&allGone.ActiveCars()==0,"A full new attempt clears the previous wrecks");
    Run(allGone,3.5,{1,0,false});
    Require(allGone.ActiveCars()==3,"A new attempt restores the initial three-car group");

    const auto east=Route(false),north=Route(true),low=Route(false,1.0/20);
    Require(east.phase==Phase::Delivered,"The east-then-north delivery route is traversable");
    Require(north.phase==Phase::Delivered,"The north-then-east delivery route is also traversable");
    Require(low.phase==Phase::Delivered&&std::abs(low.time-east.time)<.8,"Turning and delivery also work at 20 FPS");
    Require(east.time>20&&north.time>20,"Both alternate routes fit the delivery deadline");

    auto timeout=Started();Run(timeout,100,{0,0,true});
    Require(timeout.phase==Phase::Failed&&timeout.time==0,"Waiting out the deadline fails the delivery");
    timeout.Restart();Require(timeout.phase==Phase::Riding&&timeout.time==100&&Distance(timeout.bike,{StartX,StartY})==0,"Timeout retry always starts at the beginning");
    auto invalid=Started();invalid.Tick(std::numeric_limits<double>::quiet_NaN(),{});invalid.Tick(-1,{});
    Require(invalid.time==100,"Invalid frame durations cannot corrupt gameplay");
    auto editable=std::make_shared<LevelLayout>();
    editable->obstacles={{0,0,5,1,8,Pi/2}};
    editable->nodes={{-10,-10},{10,-10},{10,10},{-10,10}};
    editable->start={-30,-30};editable->goal={-10,-30};editable->timeLimit=40;
    editable->startHeading=Pi/4;editable->height=[](double x,double y){ return x*.1+y*.05; };
    editable->BuildNavigation();
    Simulation custom;custom.SetLayout(editable);
    Require(custom.bike.x==-30&&custom.time==40&&custom.heading==Pi/4,"Edited start, heading and deadline replace template values");
    Require(custom.IsBlocked({0,4})&&!custom.IsBlocked({4,0}),"Rotated level geometry uses oriented footprints");
    Require(!custom.RouteClear({-10,0},{10,0})&&custom.RouteClear({-10,10},{10,10}),"Navigation uses the instance's edited obstacles");
    const auto pose=custom.Pose({2,3},0,.65,.25);
    Require(std::abs(pose.height-.35)<1e-8&&pose.pitch>.09&&pose.bank>.04,"Bike pose uses the level's actual surface sampler");
    custom.Start();custom.Restart();
    Require(custom.layout==editable&&custom.bike.x==-30&&custom.time==40,"Restart retains the authored level layout");
    custom.bike=editable->goal;custom.Tick(.01,{0,0,true});
    Require(custom.phase==Phase::Delivered,"Delivery uses the edited goal position");
    auto empty=std::make_shared<LevelLayout>();empty->height=[](double,double){ return 0.; };empty->BuildNavigation();
    custom.SetLayout(empty);
    Require(!custom.IsBlocked({0,4})&&custom.RouteClear({-10,0},{10,0}),"Deleting all level props leaves no template collision behind");
    Require(Simulation{}.InBuilding({CityBuildings[0].x,CityBuildings[0].y}),"Independent worlds do not leak obstacle edits into one another");

    Simulation campaign;
    const auto city=DefaultLevelLayout();
    campaign.BeginCampaign(city);
    campaign.AdvanceStage();
    Require(campaign.Stage()==CampaignStage::Tutorial&&campaign.phase==Phase::Ready&&campaign.time==60&&campaign.ActiveCars()==0,
        "The lesson waits for the player to start before its one-minute countdown and traffic");
    Require(campaign.timePickups[0].available&&!campaign.timePickups[1].available&&campaign.freezePickups[0].available,
        "The teaching route shows one time coin and one snowflake");
    auto lateLesson=campaign;
    NoSpawn(lateLesson);lateLesson.Start();
    Run(lateLesson,61,{0,0,true});
    Require(lateLesson.phase==Phase::Failed,"The teaching delivery has a working deadline");
    campaign.Start();
    while(campaign.phase==Phase::Riding&&!campaign.TrafficFrozen()&&campaign.elapsed<8)
        campaign.Tick(1./120,{1,0,false});
    Require(campaign.pickupsCollected==1&&campaign.time>60&&campaign.freezesCollected==1&&campaign.TrafficFrozen()&&
        campaign.ActiveCars()==1&&campaign.wavesStarted==1&&campaign.cars[0].speed==8.5,
        "Pedaling along the teaching street collects both pickups and freezes one slow car");
    Run(campaign,.28,{1,1,false});
    Run(campaign,.4,{1,0,false});
    Run(campaign,.28,{1,-1,false});
    Run(campaign,8,{1,0,false});
    Require(campaign.phase==Phase::Delivered,"A new rider can steer around the practice car and finish the lesson");
    Require(!campaign.TickStageTransition(1.5)&&campaign.nextStageIn==.5&&campaign.TickStageTransition(.5)&&
        !campaign.TickStageTransition(.5),"The teaching result holds for two seconds and requests the next stage once");
    campaign.AdvanceStage();
    Require(campaign.Stage()==CampaignStage::FirstChase&&campaign.phase==Phase::Ready&&campaign.time==60,
        "Finishing the lesson unlocks the timed first chase");
    auto retry=campaign;
    retry.Start();
    Run(retry,61,{0,0,true});
    Require(retry.phase==Phase::Failed,"The first chase has a working deadline");
    retry.Restart();
    Require(retry.Stage()==CampaignStage::FirstChase&&retry.time==60&&retry.ActiveCars()==0,
        "A failed chase can be retried without returning to the tutorial");

    auto pickupRide=campaign;
    Require(std::count_if(pickupRide.timePickups.begin(),pickupRide.timePickups.end(),
        [](const TimePickup& pickup){ return pickup.available; })==1,
        "Stage two places one clock along the delivery streets");
    pickupRide.Start();
    pickupRide.bike=pickupRide.timePickups[0].position;
    pickupRide.Tick(.01,{0,0,true});
    Require(!pickupRide.timePickups[0].available&&pickupRide.pickupsCollected==1&&
        std::abs(pickupRide.time-(60+Simulation::TimePickupBonus-.01))<1e-8,
        "Riding over a clock consumes it and adds ten seconds");
    pickupRide.Tick(.01,{0,0,true});
    Require(pickupRide.pickupsCollected==1&&pickupRide.time<70,
        "Staying at a collected clock cannot award more time");
    pickupRide.Restart();
    Require(pickupRide.timePickups[0].available&&pickupRide.pickupsCollected==0&&pickupRide.time==60,
        "Retrying places fresh clocks and resets the clock bonus");

    auto trafficRide=campaign;
    trafficRide.freezePickups={};
    trafficRide.Start();
    while(trafficRide.wavesStarted==0&&trafficRide.elapsed<4) trafficRide.Tick(1.0/120,{1,0,false});
    Require((trafficRide.ActiveCars()==2||trafficRide.ActiveCars()==3)&&trafficRide.cars[0].position.x>trafficRide.bike.x,
        "Stage two starts with a complete group of two or three cars in front of the rider");
    {
        auto frozenRide=campaign;
        frozenRide.Start();frozenRide.bike={-140,-180};frozenRide.speed=0;
        frozenRide.elapsed=2.5;frozenRide.travelled=20;
        frozenRide.timePickups={};frozenRide.freezePickups={};
        frozenRide.freezePickups[0]={frozenRide.bike,true};
        auto& frozenCar=frozenRide.cars[0];
        frozenCar.phase=CarPhase::Pursuing;frozenCar.position={-100,-180};
        frozenCar.heading=frozenCar.attackHeading=Pi;frozenCar.speed=17;
        const auto parkedAt=frozenCar.position;
        frozenRide.Tick(1./120,{0,0,true});
        Require(!frozenRide.freezePickups[0].available&&frozenRide.freezesCollected==1&&
            frozenRide.freezeRemaining==Simulation::FreezeDuration&&Distance(parkedAt,frozenCar.position)==0,
            "Collecting a snowflake freezes a moving car immediately for three seconds");
        const double beforeFreezeTime=frozenRide.time, beforeTraffic=frozenRide.trafficElapsed;
        Run(frozenRide,1,{1,0,false},1./20);
        Require(frozenRide.bike.x> -137.1&&frozenRide.time==beforeFreezeTime,
            "The player can keep cycling while the delivery clock is frozen");
        const double beforePause=frozenRide.freezeRemaining;
        frozenRide.TogglePause();Run(frozenRide,2);frozenRide.TogglePause();
        Require(frozenRide.freezeRemaining==beforePause,"Pausing the game also pauses the freeze countdown");
        auto refreshed=frozenRide;
        refreshed.bike=refreshed.layout->goal;refreshed.speed=0;
        refreshed.Tick(1./120,{0,0,true});
        Require(refreshed.deliveriesCompleted==1&&refreshed.freezePickups[0].available&&refreshed.TrafficFrozen(),
            "The next order places one new snowflake without cancelling an active freeze");
        refreshed.bike=refreshed.freezePickups[0].position;
        refreshed.Tick(1./120,{0,0,true});
        Require(refreshed.freezesCollected==2&&refreshed.freezeRemaining==Simulation::FreezeDuration,
            "Another snowflake refreshes the freeze to three seconds without stacking");
        refreshed.Restart();
        Require(!refreshed.TrafficFrozen()&&refreshed.freezesCollected==0&&refreshed.trafficElapsed==0&&
            refreshed.freezePickups[0].available,"Retrying clears the effect and places new snowflakes");
        Run(frozenRide,1.99,{0,0,true},1./20);
        Require(frozenRide.TrafficFrozen()&&frozenRide.wavesStarted==0&&frozenRide.ActiveCars()==1&&
            frozenRide.trafficElapsed==beforeTraffic&&Distance(parkedAt,frozenCar.position)==0&&frozenCar.speed==17,
            "Frozen traffic holds position, speed and pedestrian time and spawns no new waves");
        Run(frozenRide,.01,{0,0,true});
        Require(!frozenRide.TrafficFrozen()&&Distance(parkedAt,frozenCar.position)<1e-8,
            "Traffic stays in place until the full three-second freeze ends");
        Run(frozenRide,.1,{0,0,true});
        Require(Distance(parkedAt,frozenCar.position)>1&&Distance(parkedAt,frozenCar.position)<2&&
            frozenRide.trafficElapsed>beforeTraffic+.09&&std::abs(frozenRide.time-(beforeFreezeTime-.1))<1e-8,
            "Traffic and the timer resume without catching up the frozen time");
    }
    {
        auto iceBump=campaign;
        NoSpawn(iceBump);iceBump.Start();iceBump.bike={-140,-180};iceBump.speed=0;
        iceBump.freezePickups[0]={iceBump.bike,true};
        auto& icyCar=iceBump.cars[0];
        icyCar.phase=CarPhase::Pursuing;icyCar.position=iceBump.bike+Vec2{2.55,0};
        icyCar.heading=icyCar.attackHeading=Pi;icyCar.speed=17;
        const auto icePosition=icyCar.position;
        iceBump.Tick(1./120,{0,0,true});
        Require(iceBump.phase==Phase::Riding&&iceBump.grazes==0&&icyCar.speed==17,
            "The stored speed of a frozen car does not create a phantom hit on a stationary rider");
        iceBump.speed=10;
        iceBump.Tick(1./120,{1,0,false});
        Require(iceBump.phase==Phase::Fallen&&Distance(icyCar.position,icePosition)==0,
            "A frozen car is still a solid obstacle if the rider crashes into it");
    }
    auto aimingRide=campaign;
    NoSpawn(aimingRide);aimingRide.Start();
    aimingRide.bike={-140,-180};aimingRide.speed=18;
    auto& aimingCar=aimingRide.cars[0];
    aimingCar.phase=CarPhase::Pursuing;aimingCar.position={-110,-176.4};
    aimingCar.heading=aimingCar.attackHeading=Pi;aimingCar.speed=Simulation::OncomingSpeed;
    Run(aimingRide,.2,{1,0,false});
    Require(aimingCar.attackMode==AttackMode::Ramming&&
        std::abs(std::remainder(aimingCar.heading-aimingCar.attackHeading,2*Pi))>.05,
        "An approaching car turns out of its lane to aim at the rider");
    Run(aimingRide,1,{1,0,false});
    Require(aimingRide.falls>0,"Ignoring an aimed oncoming charge results in a collision");

    auto field=std::make_shared<LevelLayout>();
    field->height=[](double,double){ return 0.; };
    for(double radius:{40.,60.,80.}) for(int angle=-180;angle<180;angle+=15)
        field->spawnPoints.push_back(Simulation::Forward(angle*Pi/180)*radius);
    const auto randomWave=[&](unsigned seed,int completedOrders=0)
    {
        Simulation sample(seed);
        sample.BeginCampaign(field);sample.Start();sample.bike=sample.layout->goal;
        sample.Tick(.01,{0,0,true});sample.AdvanceStage();sample.Start();
        for(int order=0;order<completedOrders;++order)
        { sample.bike=sample.layout->goal;sample.speed=0;sample.Tick(.01,{0,0,true}); }
        sample.bike={0,0};sample.heading=(seed%4)*Pi/2;sample.speed=0;sample.elapsed=2.5;sample.travelled=20;
        sample.Tick(.001,{0,0,true});
        return sample;
    };
    bool sawPair=false,sawTriple=false,sawLeft=false,sawRight=false,validWaves=true;
    for(unsigned seed=1;seed<=32;++seed)
    {
        const auto sample=randomWave(seed);
        const int active=sample.ActiveCars();
        sawPair=sawPair||active==2;sawTriple=sawTriple||active==3;
        validWaves=validWaves&&(active==2||active==3)&&sample.wavesStarted==1;
        for(std::size_t i=0;i<sample.cars.size();++i)
        {
            const auto& car=sample.cars[i];
            if(car.phase!=CarPhase::Pursuing) continue;
            const Vec2 delta=car.position-sample.bike;
            const double angle=std::remainder(std::atan2(delta.y,delta.x)-sample.heading,2*Pi);
            const Vec2 facing=Simulation::Forward(car.heading);
            validWaves=validWaves&&std::abs(angle)<=Pi/4+1e-8&&Distance(car.position,sample.bike)>=34&&
                (facing.x*delta.x+facing.y*delta.y)/Distance(car.position,sample.bike)<-.999;
            sawLeft=sawLeft||angle<-.25;sawRight=sawRight||angle>.25;
            for(std::size_t j=0;j<i;++j)
                if(sample.cars[j].phase==CarPhase::Pursuing) validWaves=validWaves&&Distance(car.position,sample.cars[j].position)>=10;
        }
    }
    Require(validWaves&&sawPair&&sawTriple,
        "Random waves contain both pairs and triples, face the rider, and remain separated inside the forward ninety-degree cone");
    Require(sawLeft&&sawRight,"Random entry positions include both front-left and front-right approaches");
    const auto repeatA=randomWave(19),repeatB=randomWave(19);
    bool reproducible=repeatA.ActiveCars()==repeatB.ActiveCars();
    for(std::size_t i=0;i<repeatA.cars.size();++i)
        reproducible=reproducible&&Distance(repeatA.cars[i].position,repeatB.cars[i].position)<1e-8;
    Require(reproducible,"A traffic seed reproduces the same wave for debugging");
    for(int order=0;order<Simulation::DeliveryCount;++order)
    {
        const auto sample=randomWave(19,order);
        bool matched=sample.ActiveCars()>=2&&sample.ActiveCars()<=3;
        for(const auto& car:sample.cars)
            if(car.phase==CarPhase::Pursuing) matched=matched&&car.speed==17+order&&car.rearAttacks==0;
        Require(matched&&std::abs(sample.nextFrontWaveAt-sample.elapsed-(4.-.5*order))<1e-8,
            "Later stage-two orders spawn faster random groups on the shorter cadence without rear attacks");
    }
    field->spawnPoints={{40,0}};
    const auto cramped=randomWave(20);
    Require(cramped.ActiveCars()==0&&cramped.wavesStarted==0&&
        std::none_of(cramped.cars.begin(),cramped.cars.end(),[](const Car& car){ return car.wave!=0; }),
        "A wave with insufficient entry space leaves no partially spawned cars");

    auto dodge=campaign;
    NoSpawn(dodge);
    dodge.Start();
    dodge.bike={-140,-180};dodge.speed=18;
    auto& attacker=dodge.cars[0];
    attacker.phase=CarPhase::Pursuing;attacker.position={-110,-176.4};
    attacker.heading=attacker.attackHeading=Pi;attacker.speed=Simulation::OncomingSpeed;
    for(int frame=0;frame<120&&attacker.attackMode!=AttackMode::Leaving&&dodge.phase==Phase::Riding;++frame)
        dodge.Tick(1.0/120,{1,0,false});
    Require(attacker.attackMode==AttackMode::Leaving&&dodge.phase==Phase::Riding,
        "An oncoming car commits to its charge before reaching the rider");
    const double committedHeading=attacker.heading;
    Run(dodge,.55,{1,1,false});
    Require(dodge.phase==Phase::Riding&&dodge.falls==0,
        "A rider can dodge the committed charge with a timely steering input");
    Require(attacker.position.x<dodge.bike.x&&attacker.heading==committedHeading&&attacker.rearAttacks==0,
        "After a missed charge, the car drives past without steering back toward the rider");

    campaign.Start();
    NoSpawn(campaign);
    for(int order=0;order<Simulation::DeliveryCount;++order)
    {
        const auto oncomingRules=campaign.Traffic();
        Require(oncomingRules.speed==17+order&&oncomingRules.waveInterval==4.-.5*order&&oncomingRules.maxActiveCars==4+order,
            "Each stage-two order raises speed and capacity while shortening the wave interval");
        const Vec2 corner=campaign.layout->start+Simulation::Forward(campaign.layout->startHeading)*90;
        bool reachedCorner=false;
        for(int frame=0;frame<60*40&&campaign.phase==Phase::Riding&&campaign.deliveriesCompleted==order;++frame)
        {
            if(Distance(campaign.bike,corner)<5) reachedCorner=true;
            campaign.Tick(1.0/60,DriveTo(campaign,reachedCorner?campaign.layout->goal:corner));
        }
        Require(campaign.deliveriesCompleted==order+1,"The next delivery can be reached along the city streets");
    }
    Require(campaign.phase==Phase::Delivered&&campaign.money==300,
        "The stage ends after three deliveries and awards three payments");
    Require(campaign.TickStageTransition(Simulation::StageTransitionDelay),"The second-stage result also advances automatically");

    auto orders=retry;
    NoSpawn(orders);
    const Vec2 firstDrop=orders.layout->goal;
    orders.bike=firstDrop;
    orders.Tick(.01,{0,0,true});
    Require(orders.phase==Phase::Riding&&orders.deliveriesCompleted==1&&orders.money==100&&
        Distance(orders.layout->goal,firstDrop)>30&&orders.time>59.9&&orders.time<=60,
        "An on-time drop pays once and immediately starts a fresh deadline for the next destination");
    orders.Tick(.01,{0,0,true});
    Require(orders.money==100,"Waiting at the previous destination cannot award another payment");
    orders.Fall();
    Run(orders,1.2,{0,0,true});
    Require(orders.deliveriesCompleted==1&&orders.money==100&&Distance(orders.bike,firstDrop)<.3,
        "A crash retries from the last delivery checkpoint and retains earned money");
    NoSpawn(orders);
    Run(orders,61,{0,0,true});
    Require(orders.phase==Phase::Failed&&orders.money==100&&orders.deliveriesCompleted==1,
        "A missed deadline awards no extra money and preserves completed orders");
    orders.Restart();
    orders.bike=orders.layout->goal;orders.Tick(.01,{0,0,true});
    Require(orders.phase==Phase::Riding&&orders.deliveriesCompleted==2&&orders.money==200,
        "Retrying the failed order continues to the third destination");
    orders.bike=orders.layout->goal;orders.Tick(.01,{0,0,true});
    orders.Restart();
    orders.bike=orders.layout->goal;orders.Tick(.01,{0,0,true});
    Require(orders.phase==Phase::Delivered&&orders.money==300,
        "Replaying the last stop cannot duplicate its reward");
    auto lateOrder=retry;
    lateOrder.time=.005;lateOrder.bike=lateOrder.layout->goal;
    lateOrder.Tick(.01,{0,0,true});
    Require(lateOrder.phase==Phase::Failed&&lateOrder.money==0&&lateOrder.deliveriesCompleted==0,
        "Arriving as the deadline expires does not count as an on-time paid delivery");
    campaign.AdvanceStage();
    Require(campaign.Stage()==CampaignStage::CityRush&&campaign.HasDeliverySequence()&&campaign.time==60&&campaign.frontWavesEnabled&&
        Distance(campaign.layout->goal,city->goal)>30&&!campaign.IsClinicDelivery(),
        "The final stage begins a three-stop route on the same map with full chase rules");
    Require(campaign.money==300,"Earned cash is retained when advancing to the final stage");
    campaign.Start();
    Run(campaign,3.5,{1,0,false});
    Require(campaign.ActiveCars()==3,"The final stage still opens with three pursuers");
    auto finalOrders=campaign;
    finalOrders.Restart();
    NoSpawn(finalOrders);
    {
        auto coinRide=finalOrders;
        coinRide.bike=coinRide.timePickups[0].position;
        coinRide.speed=0;
        coinRide.Tick(.01,{0,0,true});
        Require(coinRide.pickupsCollected==1&&!coinRide.timePickups[0].available&&
            std::abs(coinRide.time-69.99)<1e-8,"An alley coin adds ten seconds to the final-stage countdown once");
        coinRide.Tick(.01,{0,0,true});
        Require(coinRide.pickupsCollected==1&&coinRide.time<69.99,"A collected alley coin does not respawn during the order");
    }
    for(int order=0;order<Simulation::DeliveryCount;++order)
    {
        const auto& firstCoin=finalOrders.timePickups[0];
        const auto& secondCoin=finalOrders.timePickups[1];
        Require(firstCoin.available&&secondCoin.available&&Distance(firstCoin.position,secondCoin.position)>=60&&
            !finalOrders.IsBlocked(firstCoin.position,1.5)&&!finalOrders.IsBlocked(secondCoin.position,1.5),
            "Each final-stage order places two separated, reachable alley coins");
        const auto finalRules=finalOrders.Traffic();
        Require(finalRules.speed==21+order&&finalRules.waveInterval==8.-1.5*order&&
            finalRules.maxActiveCars==(order==0?5:order==1?6:8),
            "Each final-stage order increases cruise speed and capacity and brings waves sooner");
        auto fasterTraffic=finalOrders;
        fasterTraffic.bike={-50,-180};fasterTraffic.speed=0;
        auto& fasterCar=fasterTraffic.cars[0];
        fasterCar.phase=CarPhase::Pursuing;fasterCar.position={-200,-180};fasterCar.heading=0;fasterCar.speed=11;
        Run(fasterTraffic,3,{0,0,true});
        Require(std::abs(fasterCar.speed-(21+order))<.001&&fasterCar.phase==CarPhase::Pursuing,
            "Final-stage cars actually reach the higher cruising speed on each subsequent order");
        for(double frame:{1./120,1./20})
        {
            auto turningTraffic=finalOrders;
            turningTraffic.bike={0,-180};turningTraffic.speed=0;
            auto& turningCar=turningTraffic.cars[0];
            turningCar.phase=CarPhase::Pursuing;turningCar.position={6,-180};
            turningCar.heading=Pi/2;turningCar.speed=21+order;
            Run(turningTraffic,.4,{0,0,true},frame);
            Require(turningCar.phase==CarPhase::Pursuing&&turningCar.crashCause==CrashCause::None&&turningCar.roll==0,
                "Every final-stage traffic tier stays upright during a collision-free fast turn at high and low frame rates");
        }
        const Vec2 corner=finalOrders.DeliveryJunction();
        const double routeLength=Distance(finalOrders.layout->start,corner)+Distance(corner,finalOrders.layout->goal);
        Require(routeLength>=269.9+90*order,"Each final-stage delivery uses a longer city route");
        Require(finalOrders.RouteClear(finalOrders.layout->start,corner,2)&&
            finalOrders.RouteClear(corner,finalOrders.layout->goal,2),
            "Each final-stage destination has a clear route through the streets");
        if(order==Simulation::DeliveryCount-1)
            Require(finalOrders.IsClinicDelivery()&&Distance(finalOrders.layout->goal,city->goal)<.01,
                "The final delivery returns to the original clinic destination");
        bool reachedCorner=false;
        for(int frame=0;frame<60*50&&finalOrders.phase==Phase::Riding&&finalOrders.deliveriesCompleted==order;++frame)
        {
            if(Distance(finalOrders.bike,corner)<5) reachedCorner=true;
            finalOrders.Tick(1.0/60,DriveTo(finalOrders,reachedCorner?finalOrders.layout->goal:corner));
        }
        Require(finalOrders.deliveriesCompleted==order+1&&finalOrders.money==400+100*order,
            "Riding to each final-stage destination pays exactly once");
        if(order<Simulation::DeliveryCount-1)
        {
            Require(finalOrders.phase==Phase::Riding&&finalOrders.time>59.9&&finalOrders.time<=60,
                "Intermediate final-stage drops continue with a fresh per-order deadline");
            const auto checkpoint=finalOrders.layout->start;
            finalOrders.Fall();Run(finalOrders,1.2,{0,0,true});NoSpawn(finalOrders);
            Require(finalOrders.deliveriesCompleted==order+1&&finalOrders.money==400+100*order&&
                Distance(finalOrders.bike,checkpoint)<.3&&finalOrders.Traffic().speed==22+order,
                "Final-stage crash retries preserve the checkpoint, reward and increased difficulty");
            Run(finalOrders,61,{0,0,true});
            Require(finalOrders.phase==Phase::Failed&&finalOrders.money==400+100*order,
                "A missed final-stage deadline awards nothing and preserves earlier payments");
            finalOrders.Restart();NoSpawn(finalOrders);
        }
    }
    Require(finalOrders.phase==Phase::Delivered&&finalOrders.money==600,
        "The campaign finishes only after all three final-stage deliveries");
    finalOrders.Restart();NoSpawn(finalOrders);
    finalOrders.bike=finalOrders.layout->goal;finalOrders.Tick(.01,{0,0,true});
    Require(finalOrders.phase==Phase::Delivered&&finalOrders.money==600,
        "Replaying the final clinic cannot duplicate its reward");
    campaign=finalOrders;
    Require(campaign.TickStageTransition(Simulation::StageTransitionDelay)&&!campaign.TickStageTransition(.5),
        "The city finale requests the park map exactly once after the result delay");
    campaign.AdvanceStage();
    Require(campaign.Stage()==CampaignStage::CityRush&&campaign.money==600,"City-to-park travel preserves the result until the new map loads");
    campaign.BeginCampaign(city);
    Require(campaign.Stage()==CampaignStage::Tutorial&&campaign.phase==Phase::Ready&&campaign.ActiveCars()==0&&campaign.money==0,
        "A new campaign clears earlier rewards and starts at the lesson");
    std::cout<<"DELIVERY_CITY_CHECKS_PASSED: "<<Checks<<'\n';
}
