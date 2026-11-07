#pragma once
#include "LevelLayout.h"
#include <array>
#include <cstddef>
#include <random>

namespace Delivery
{
enum class Phase { Ready, Riding, Paused, Fallen, Delivered, Failed };
enum class CampaignStage { Tutorial = 1, FirstChase, CityRush, ParkCrossing };
enum class CarPhase { Waiting, Pursuing, Wrecked };
enum class CrashCause { None, Building, Car };
enum class AttackMode { Oncoming, Recovering, Catching, Ramming, Leaving };
struct Input { double throttle = 0, steer = 0; bool brake = false; };
struct Pickup
{
    Vec2 position;
    bool available = false;
};
using TimePickup = Pickup;
struct TrafficImpact
{
    Vec2 position;
    double speed = 0;
};
struct TrafficRules
{
    double speed = 0, waveInterval = 0, waveTravel = 0;
    int maxActiveCars = 0;
};
struct WalkPose
{
    Vec2 position;
    double heading, distance;
};
struct PedestrianWalk
{
    WalkRoute route;
    double speed=1.4, startProgress=0;
    bool touchingBike=false;
    WalkPose Sample(double trafficTime) const;
};
struct Car
{
    CarPhase phase = CarPhase::Waiting;
    Vec2 position, waypoint;
    double heading = 0, speed = 0, navigationTimer = 0, contactCooldown = 0;
    double spawnAt = 0;
    double roll = 0, rollDirection = 1, wreckTimer = 0;
    CrashCause crashCause = CrashCause::None;
    AttackMode attackMode = AttackMode::Oncoming;
    double reattackIn = 0, recoveryAge = 0, ramTime = 0, attackHeading = 0;
    double closestApproach = 1e9, lastPassAt = 0, lastRamAt = 0;
    int passes = 0, rearAttacks = 0;
    int wave = 0, formationIndex = 0;
    bool touching = false;
    int identity = 0, contacts = 0;
};
struct Simulation
{
    // Distinct lifetime slots preserve old pursuers and wrecks across new waves.
    static constexpr std::size_t CarCount = 32;
    static constexpr std::size_t InitialCarCount = 3;
    static constexpr int FrontWaveSize = 2, MaxActiveCars = 8;
    static constexpr double FrontWaveInterval = 8, FrontWaveTravel = 40;
    static constexpr double TimeLimit = DeliveryTime;
    static constexpr std::size_t TimePickupCount = 2;
    static constexpr std::size_t FreezePickupCount = 1;
    static constexpr double FreezeDuration = 3;
    static constexpr double StageTransitionDelay = 2;
    static constexpr int TimePickupBonus = 10;
    static constexpr double OncomingSpeed = 17;
    static constexpr double CityCruiseSpeed = 21;
    static constexpr int DeliveryCount = 3, DeliveryReward = 100;
    static constexpr double BikeMaxSpeed = 20, BikeAcceleration = 6, SteeringResponse = 1.18;
    Phase phase = Phase::Ready;
    Vec2 bike{StartX, StartY}, shove;
    double heading = 0, speed = 0, time = TimeLimit, elapsed = 0, travelled = 0;
    double steering = 0, roll = 0, rollVelocity = 0, hitFlash = 0;
    double fallTimer = 0, fallDirection = 1, restartFlash = 0;
    int falls = 0, grazes = 0, attempt = 1;
    int pedestrianHits = 0;
    TrafficImpact trafficImpact;
    bool frontWavesEnabled = true;
    int wavesStarted = 0;
    double nextFrontWaveAt = 0, travelAtLastWave = 0, nextWaveCheckAt = 0;
    std::array<Car, CarCount> cars;
    std::vector<PedestrianWalk> pedestrians;
    std::array<TimePickup, TimePickupCount> timePickups;
    std::array<Pickup, FreezePickupCount> freezePickups;
    int pickupsCollected = 0;
    double timePickupFlash = 0;
    double freezeRemaining = 0, trafficElapsed = 0;
    int freezesCollected = 0;
    int deliveriesCompleted = 0, money = 0;
    std::size_t bridgesCrossed = 0;
    double deliveryFlash = 0;
    double nextStageIn = 0;

    std::shared_ptr<const LevelLayout> layout;
    Simulation();
    explicit Simulation(unsigned trafficSeed);
    void SetLayout(std::shared_ptr<const LevelLayout> level);
    void BeginCampaign(std::shared_ptr<const LevelLayout> level);
    void BeginPark(std::shared_ptr<const LevelLayout> level,int carriedMoney);
    void AdvanceStage();
    bool TickStageTransition(double dt);
    CampaignStage Stage() const { return stage; }
    bool TrafficFrozen() const { return freezeRemaining>1e-8; }
    bool HasPendingBridge() const { return stage==CampaignStage::ParkCrossing&&bridgesCrossed<layout->bridgeCheckpoints.size(); }
    bool HasDeliverySequence() const { return campaignLayout&&stage!=CampaignStage::Tutorial; }
    bool IsClinicDelivery() const
    { return stage==CampaignStage::CityRush&&(!HasDeliverySequence()||deliveriesCompleted>=DeliveryCount-1); }
    TrafficRules Traffic() const;
    Vec2 DeliveryJunction() const;
    bool RouteClear(Vec2 a,Vec2 b,double padding=.6) const;
    bool IsBlocked(Vec2 point,double radius=.55) const;
    GroundPose Pose(Vec2 point,double angle,double halfLength,double halfWidth) const;
    void Reset();
    void Start();
    void Restart();
    void TogglePause();
    void Tick(double dt, const Input& input);
    void Fall(double direction = 1);
    double DistanceToGoal() const;
    Vec2 CurrentTarget() const;
    int ActiveCars() const;
    int WreckedCars() const;
    static bool SegmentClear(Vec2 a, Vec2 b, double padding = .6);
    static bool InBuilding(Vec2 p, double radius = .55);
    static Vec2 Forward(double angle);
private:
    CampaignStage stage = CampaignStage::CityRush;
    std::shared_ptr<const LevelLayout> campaignLayout;
    std::array<Vec2, DeliveryCount> deliveryStops;
    std::array<Vec2, DeliveryCount> deliveryJunctions;
    std::minstd_rand trafficRandom;
    int pendingFrontWaveSize = 0;
    void LoadStage(CampaignStage nextStage);
    void CompleteDelivery();
    void Step(double dt, const Input& input);
    void MoveBike(double dt);
    void ContactPedestrians(Vec2 previousBike);
    void MoveCar(Car& car, double dt);
    void MoveParkCar(Car& car,double dt);
    void MoveOncomingCar(Car& car, double dt);
    void MoveCarForward(Car& car, double dt);
    void PlacePickups(Pickup* pickups,std::size_t capacity,bool freezing);
    void PlaceOrderPickups();
    void PlaceAlleyTimePickups();
    void CollectTimePickups();
    void CollectFreezePickups();
    void BeginRecovery(Car& car);
    void MoveWreck(Car& car, double dt);
    void WreckCar(Car& car, CrashCause cause, double direction);
    void CarContacts();
    bool SpawnCar(Car& car);
    void SpawnFrontWave();
    void Contact(Car& car);
    Vec2 NavigationTarget(const Car& car, Vec2 target) const;
};
}
