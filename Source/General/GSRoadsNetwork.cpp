#include "GSRoadsNetwork.h"
#include "GSLogStream.h"
#include "GSGeography.h"
#include "GSRandom.h"
#include <cstddef>
#include <queue>
#include <random>
#include <unordered_map>

namespace NS_GSLiveAirportMSFS
{

void GSRoadsNetwork::AddNode(DWORD id, const GSCoord& loc, float heading, GSRoadsNetwork::RoadNodeType tp, bool hasJetway)
{
	auto itr = m_roadNodes.try_emplace(id, loc, tp, heading, hasJetway);
	if (!itr.second) {
        if (itr.first->second.m_pntType != tp) {
		    GSLogStream::LogError("Duplicate Node ID with different type: ") << itr.first->second.m_pntType << " vs " << tp;
        }
	} else {
        m_allNodes.emplace_back(&itr.first->second);
        if (tp == PARKING) {
            m_normalParkings.emplace_back(&itr.first->second);
        } else if (tp == VEHICLE) {
            m_vehicleParkings.emplace_back(&itr.first->second);
        }
    }
}

void GSRoadsNetwork::AddPath(DWORD idA, DWORD idB, RoadPathType pathType)
{
	auto itrA = m_roadNodes.find(idA);
	auto itrB = m_roadNodes.find(idB);
	if (itrA == m_roadNodes.end() || itrB == m_roadNodes.end()) {
		GSLogStream::LogError("GSRoadsNetwork::AddPath - Missing endpoint(s): ") << idA << ", " << idB;
		return;
	}

	RoadPath& path = m_roadPaths.emplace_back(
		&itrA->second, &itrB->second, GSGeography::DistanceMeters(itrA->second.m_loc, itrB->second.m_loc), pathType, false);

	itrA->second.m_paths.push_back(&path);
	itrB->second.m_paths.push_back(&path);
}

void GSRoadsNetwork::AddPath(const RoadPath& path)
{
    if (!path.m_nodeA || !path.m_nodeB) {
        GSLogStream::LogError("GSRoadsNetwork::AddPath - Null endpoint(s)");
        return;
    }

    RoadPath& newPath = m_roadPaths.emplace_back(path);   // copies m_distMeters as-is
    path.m_nodeA->m_paths.push_back(&newPath);
    path.m_nodeB->m_paths.push_back(&newPath);
}

std::optional<const GSRoadsNetwork::RoadNode*> GSRoadsNetwork::GetClosestNormalParking(const GSCoord& location) const
{
    const RoadNode* closest = nullptr;
	double closestDist = 9999999.0;
	double tempDist = 0.0;

    for (const auto* itr : m_normalParkings) {
        tempDist = GSGeography::DistanceMeters(location, itr->m_loc);
		if (!closest || tempDist < closestDist) {
            closest = itr;
			closestDist = tempDist;
        }
    }

    if (!closest) {
    	return std::nullopt;
	}

	return closest;
}

std::optional<const GSRoadsNetwork::RoadNode*> GSRoadsNetwork::GetRandomVehicleParking() const
{
	if (m_vehicleParkings.empty()) {
		return std::nullopt;
	}

	std::uniform_int_distribution<std::size_t> distribution(0, m_vehicleParkings.size() - 1);
	return m_vehicleParkings[distribution(GSRandom::Engine())];
}

std::optional<const GSRoadsNetwork::RoadNode*> GSRoadsNetwork::GetRandomNode() const
{
	if (m_allNodes.empty()) {
		return std::nullopt;
	}

	std::uniform_int_distribution<std::size_t> distribution(0, m_allNodes.size() - 1);
	return m_allNodes[distribution(GSRandom::Engine())];
}

void GSRoadsNetwork::MergeNetwork(GSRoadsNetwork& network)
{
	if (this == &network) { return; }

	// Move the nodes in. unordered_map::merge moves the node handles, so every RoadNode
    // keeps its address — the source paths' m_nodeA/m_nodeB therefore stay valid. Node IDs
    // are unique per network, so nothing collides (on a collision merge keeps *this*'s node,
    // the same "destination wins" rule the old try_emplace loop used).
    m_roadNodes.merge(network.m_roadNodes);

	if (!network.m_roadNodes.empty()) {
		GSLogStream::LogError("Duplicate Node IDs on MergeNetwork.");
	}

    // Move the paths in. list::splice moves the list nodes, so every RoadPath keeps its
    // address — the nodes' m_paths therefore stay valid.
    m_roadPaths.splice(m_roadPaths.end(), network.m_roadPaths);

    // The index vectors hold RoadNode* which are still valid — just append them.
    m_allNodes.insert(m_allNodes.end(), network.m_allNodes.begin(), network.m_allNodes.end());
    m_normalParkings.insert(m_normalParkings.end(), network.m_normalParkings.begin(), network.m_normalParkings.end());
    m_vehicleParkings.insert(m_vehicleParkings.end(), network.m_vehicleParkings.begin(), network.m_vehicleParkings.end());
}

std::optional<const GSRoadsNetwork::RoadNode*> GSRoadsNetwork::GetClosestJetwayParking(const GSCoord& location) const
{
	const RoadNode* closest = nullptr;
	double closestDistance = 0.0;

	for (const RoadNode* parking : m_normalParkings) {
		if (!parking->m_hasJetway) {
			continue;
		}

		const double distance = GSGeography::DistanceMeters(location, parking->m_loc);
		if (!closest || distance < closestDistance) {
			closest = parking;
			closestDistance = distance;
		}
	}

	if (!closest) {
		return std::nullopt;
	}
	return closest;
}

// A* search using weighted road-segment distance. Vehicle paths use their real
// length; normal paths cost 100 times their length to strongly prefer vehicle paths.
void GSRoadsNetwork::FindShortestPath(const RoadNode& start, const RoadNode& end, std::list<const RoadNode*>& out) const
{
	struct OpenEntry {
		const RoadNode* m_node;
		double m_costFromStart;
		double m_estimatedTotalCost;
	};
	struct LowestEstimatedDistance {
		bool operator()(const OpenEntry& left, const OpenEntry& right) const
		{
			return left.m_estimatedTotalCost > right.m_estimatedTotalCost;
		}
	};

	out.clear();
	if (&start == &end) {
		out.push_back(&start);
		return;
	}

	auto estimatedDistanceToEnd = [&end](const RoadNode* node) {
		return GSGeography::DistanceMeters(node->m_loc, end.m_loc);
	};

	std::unordered_map<const RoadNode*, double> distanceFromStart;
	std::unordered_map<const RoadNode*, const RoadNode*> previousNode;
	std::priority_queue<OpenEntry, std::vector<OpenEntry>, LowestEstimatedDistance> open;

	distanceFromStart.emplace(&start, 0.0);
	open.push({ &start, 0.0, estimatedDistanceToEnd(&start) });

	while (!open.empty()) {
		const OpenEntry current = open.top();
		open.pop();

		const auto distanceIt = distanceFromStart.find(current.m_node);
		if (distanceIt == distanceFromStart.end() || current.m_costFromStart > distanceIt->second) {
			continue;
		}
		const double currentCost = distanceIt->second;

		if (current.m_node == &end) {
			for (const RoadNode* node = &end;; node = previousNode.at(node)) {
				out.push_front(node);
				if (node == &start) {
					return;
				}
			}
		}

		for (const RoadPath* path : current.m_node->m_paths) {
			const RoadNode* next = nullptr;
			if (path->m_nodeA == current.m_node) {
				next = path->m_nodeB;
			} else if (path->m_nodeB == current.m_node) {
				next = path->m_nodeA;
			} else {
				continue;
			}

			const double edgeCost = path->m_distMeters *
				(path->m_pathType == PATH_VEHICLE ? 1.0 : 100.0);
			const double candidateCost = currentCost + edgeCost;
			auto [nextDistanceIt, inserted] = distanceFromStart.try_emplace(next, candidateCost);
			if (!inserted && candidateCost >= nextDistanceIt->second) {
				continue;
			}

			nextDistanceIt->second = candidateCost;
			previousNode.insert_or_assign(next, current.m_node);
			open.push({ next, candidateCost, candidateCost + estimatedDistanceToEnd(next) });
		}
	}
}

GSRoadsNetwork::RoadPath GSRoadsNetwork::GetClosestDisjointNodes(GSRoadsNetwork& other)
{
	RoadPath shrt{ nullptr, nullptr, 9999999.0, PATH_VEHICLE, true };

    for (auto& [idA, nodeA] : m_roadNodes) {
        for (auto& [idB, nodeB] : other.m_roadNodes) {
            const double dist = GSGeography::DistanceMeters(nodeA.m_loc, nodeB.m_loc);
            if (dist < shrt.m_distMeters) { 
				shrt.m_distMeters = dist; 
				shrt.m_nodeA = &nodeA;
				shrt.m_nodeB = &nodeB; 
			}
        }
    }

    return shrt;
}

void GSRoadsNetwork::Print()
{
	/*for (const auto& pt : m_roadPaths) {
		if (pt.m_custom) {
			GSLogStream::Log("Path Dsit: ") << pt.m_distMeters << ", "
				<< pt.m_nodeA->m_loc.Long() << "," << pt.m_nodeA->m_loc.Lat()
				<< " <-> " << pt.m_nodeB->m_loc.Long() << "," << pt.m_nodeB->m_loc.Lat();
		}
	}*/
}

}
