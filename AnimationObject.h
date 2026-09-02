#pragma once

#include "Aircraft.h"
#include "GroundServicesConfig.h"
#include "SimConnectThread.h"

#include <chrono>
#include <optional>
#include <vector>

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

class AnimationFrame final
{
  public:
    explicit AnimationFrame(SimConnectThread &simConnect);

    void Begin(std::chrono::steady_clock::time_point now);
    [[nodiscard]] std::chrono::steady_clock::time_point Now() const;
    void FreezeObject(AircraftId objectId);
    void CancelObjectAnimation(AircraftId objectId);
    void QueueMotionUpdate(AnimationUpdate update);
    void QueuePositionUpdate(ObjectPositionUpdate update);
    void QueueCarrierUpdate(AnimationCarrierUpdate update);
    void Flush();

  private:
    SimConnectThread &m_simConnect;
    std::chrono::steady_clock::time_point m_now{};
    std::vector<AnimationUpdate> m_motionUpdates;
    std::vector<ObjectPositionUpdate> m_positionUpdates;
    std::vector<AnimationCarrierUpdate> m_carrierUpdates;
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
