#pragma once
#include "GSRequest.h"
#include "GSSimConnect.h"
#include "GSCoord.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <optional>
#include <deque>
#include <list>

namespace NS_GSLiveAirportMSFS
{

class GSRoadsNetwork
{
public:
	GSRoadsNetwork() = default;
	GSRoadsNetwork(const GSRoadsNetwork&) = delete;
	GSRoadsNetwork& operator=(const GSRoadsNetwork&) = delete;
	GSRoadsNetwork(GSRoadsNetwork&&) = default;
	GSRoadsNetwork& operator=(GSRoadsNetwork&&) = default;

	enum RoadNodeType {
		NORMAL,
		PARKING,
		VEHICLE
	};

	struct RoadPath;

	struct RoadNode {
		GSCoord m_loc;
		RoadNodeType m_pntType;
		float m_heading;
        bool m_hasJetway;
		std::vector<RoadPath*> m_paths;
	};

	struct RoadPath {
		RoadNode* m_nodeA;
		RoadNode* m_nodeB;
        double m_distMeters;
	};

	void AddNode(DWORD id, const GSCoord& loc, float heading, RoadNodeType tp, bool hasJetway);
	void AddPath(DWORD idA, DWORD idB);
	bool HasNode(DWORD id) const { return m_roadNodes.contains(id); }

    std::optional<const RoadNode*> GetClosestNormalParking(const GSCoord& location) const;
    std::optional<const RoadNode*> GetRandomVehicleParking() const;

    void MergeNetwork(GSRoadsNetwork& network);
    void FindShortestPath(const RoadNode& start, const RoadNode& end, std::list<const RoadNode*>& out) const;
    std::optional<const RoadNode*> GetClosestJetwayParking(const GSCoord& location) const;
    
private:

	std::unordered_map<DWORD, RoadNode> m_roadNodes;
	std::deque<RoadPath> m_roadPaths; // Removing elements will break pointers.
	std::vector<RoadNode*> m_normalParkings;
	std::vector<RoadNode*> m_vehicleParkings;
};

}
