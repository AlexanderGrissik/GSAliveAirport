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

void GSRoadsNetwork::AddPath(DWORD idA, DWORD idB)
{
	auto itrA = m_roadNodes.find(idA);
	auto itrB = m_roadNodes.find(idB);
	if (itrA == m_roadNodes.end() || itrB == m_roadNodes.end()) {
		GSLogStream::LogError("GSRoadsNetwork::AddPath - Missing endpoint(s): ") << idA << ", " << idB;
		return;
	}

	RoadPath& path = m_roadPaths.emplace_back(
		&itrA->second, &itrB->second, GSGeography::DistanceMeters(itrA->second.m_loc, itrB->second.m_loc));

	itrA->second.m_paths.push_back(&path);
	itrB->second.m_paths.push_back(&path);
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

	std::unordered_map<const RoadNode*, RoadNode*> nodeMapping;
	nodeMapping.reserve(network.m_roadNodes.size());

	for (const auto& [id, sourceNode] : network.m_roadNodes) {
		auto [destinationIt, inserted] = m_roadNodes.try_emplace(
			id, sourceNode.m_loc, sourceNode.m_pntType, sourceNode.m_heading, sourceNode.m_hasJetway);

		if (!inserted && destinationIt->second.m_pntType != sourceNode.m_pntType) {
			GSLogStream::LogError("GSRoadsNetwork::MergeNetwork - Duplicate node ID with different type: ")
				<< destinationIt->second.m_pntType << " vs " << sourceNode.m_pntType;
		}

		if (inserted) {
			m_allNodes.push_back(&destinationIt->second);
			if (sourceNode.m_pntType == PARKING) {
				m_normalParkings.push_back(&destinationIt->second);
			} else if (sourceNode.m_pntType == VEHICLE) {
				m_vehicleParkings.push_back(&destinationIt->second);
			}
		}

		nodeMapping.emplace(&sourceNode, &destinationIt->second);
	}

	for (const RoadPath& sourcePath : network.m_roadPaths) {
		const auto nodeAIt = nodeMapping.find(sourcePath.m_nodeA);
		const auto nodeBIt = nodeMapping.find(sourcePath.m_nodeB);
		if (nodeAIt == nodeMapping.end() || nodeBIt == nodeMapping.end()) {
			GSLogStream::LogError("GSRoadsNetwork::MergeNetwork - Path endpoint missing from source network");
			continue;
		}

		RoadPath& destinationPath = m_roadPaths.emplace_back(
			nodeAIt->second, nodeBIt->second, sourcePath.m_distMeters);
		nodeAIt->second->m_paths.push_back(&destinationPath);
		nodeBIt->second->m_paths.push_back(&destinationPath);
	}
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

// A* search using road-segment distance as cost and straight-line geographic distance to the end as the heuristic.
void GSRoadsNetwork::FindShortestPath(const RoadNode& start, const RoadNode& end, std::list<const RoadNode*>& out) const
{
	struct OpenEntry {
		const RoadNode* m_node;
		double m_distanceFromStart;
		double m_estimatedTotalDistance;
	};
	struct LowestEstimatedDistance {
		bool operator()(const OpenEntry& left, const OpenEntry& right) const
		{
			return left.m_estimatedTotalDistance > right.m_estimatedTotalDistance;
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
		if (distanceIt == distanceFromStart.end() || current.m_distanceFromStart > distanceIt->second) {
			continue;
		}
		const double currentDistance = distanceIt->second;

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

			const double candidateDistance = currentDistance + path->m_distMeters;
			auto [nextDistanceIt, inserted] = distanceFromStart.try_emplace(next, candidateDistance);
			if (!inserted && candidateDistance >= nextDistanceIt->second) {
				continue;
			}

			nextDistanceIt->second = candidateDistance;
			previousNode.insert_or_assign(next, current.m_node);
			open.push({ next, candidateDistance, candidateDistance + estimatedDistanceToEnd(next) });
		}
	}
}

}
