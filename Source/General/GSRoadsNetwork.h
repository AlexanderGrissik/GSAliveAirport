#pragma once
#include "GSRequest.h"
#include "GSSimConnect.h"
#include "GSCoord.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <optional>
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

	enum RoadPathType {
		PATH_NORMAL,
		PATH_VEHICLE
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
		RoadPathType m_pathType;
		bool m_custom;
	};

	void AddNode(DWORD id, const GSCoord& loc, float heading, RoadNodeType tp, bool hasJetway);
	void AddPath(DWORD idA, DWORD idB, RoadPathType pathType);
	void AddPath(const RoadPath& path);
	bool HasNode(DWORD id) const { return m_roadNodes.contains(id); }

    std::optional<const RoadNode*> GetClosestNormalParking(const GSCoord& location) const;
    std::optional<const RoadNode*> GetRandomVehicleParking() const;
	std::optional<const RoadNode*> GetRandomNode() const;

    void MergeNetwork(GSRoadsNetwork& network);
    void FindShortestPath(const RoadNode& start, const RoadNode& end, std::list<const RoadNode*>& out) const;
    std::optional<const RoadNode*> GetClosestJetwayParking(const GSCoord& location) const;
	RoadPath GetClosestDisjointNodes(GSRoadsNetwork& other);
	void Print();

private:

	std::unordered_map<DWORD, RoadNode> m_roadNodes;
	std::list<RoadPath> m_roadPaths; // Removing elements will break pointers.
	std::vector<const RoadNode*> m_normalParkings;
	std::vector<const RoadNode*> m_vehicleParkings;
    std::vector<const RoadNode*> m_allNodes;
};

}
