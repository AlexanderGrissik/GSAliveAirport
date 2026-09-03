#pragma once

#include "Aircraft.h"
#include "GroundServiceTypes.h"
#include "ISimConnectHandler.h"

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace parking_services
{
struct AnimationProximityTarget
{
    double latitude{};
    double longitude{};
};

struct AnimationRoute
{
    double latitude1{};
    double longitude1{};
    double altitude1Feet{};
    double latitude2{};
    double longitude2{};
    double altitude2Feet{};
};

struct AnimationUpdate
{
    DWORD objectId{};
    double latitude{};
    double longitude{};
    double altitudeFeet{};
    double headingDegrees{};
    double velocityBodyYMetersPerSecond{};
};

struct ObjectPositionUpdate
{
    DWORD objectId{};
    double latitude{};
    double longitude{};
    double altitudeFeet{};
    double headingDegrees{};
};

struct AnimationCarrierUpdate
{
    DWORD objectId{};
    std::string carrier;
    double value{};
};

class AnimationFrame final
{
  public:
    explicit AnimationFrame(ISimConnectHandler &simConnect);

    void Begin(std::chrono::steady_clock::time_point now);
    [[nodiscard]] std::chrono::steady_clock::time_point Now() const;
    void FreezeObject(AircraftId objectId);
    void CancelObjectAnimation(AircraftId objectId);
    void QueueMotionUpdate(AnimationUpdate update);
    void QueuePositionUpdate(ObjectPositionUpdate update);
    void QueueCarrierUpdate(AnimationCarrierUpdate update);
    void Flush();
    void ResetDefinitions();

  private:
    ISimConnectHandler &m_simConnect;
    std::chrono::steady_clock::time_point m_now{};
    std::map<DWORD, AnimationUpdate> m_motionUpdates;
    std::map<DWORD, ObjectPositionUpdate> m_positionUpdates;
    std::map<std::pair<DWORD, std::string>, AnimationCarrierUpdate> m_carrierUpdates;
    std::map<std::string, SIMCONNECT_DATA_DEFINITION_ID, std::less<>>
        m_carrierDefinitions;
    DWORD m_nextCarrierDefinitionId = 100;
};

class AnimationObject
{
  public:
    virtual ~AnimationObject() = default;

    [[nodiscard]] virtual bool OwnsObject(AircraftId objectId) const = 0;
    [[nodiscard]] virtual std::optional<AnimationProximityTarget>
        ProximityTarget() const;
    virtual bool SetDistanceRankedActive(bool active, AnimationFrame &frame);
    virtual void Start(AnimationFrame &frame) = 0;
    virtual void Animate(AnimationFrame &frame) = 0;
    virtual void Cancel(AnimationFrame &frame) const = 0;
    [[nodiscard]] virtual bool IsWalking() const;
};
} // namespace parking_services
