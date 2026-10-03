#include "Rando/Logic/Logic.h"

#include <cstring>

namespace Rando {
namespace Logic {

thread_local const SearchGate* tSearchGate = nullptr;

namespace {
struct ScopedSearchGate {
    const SearchGate* previous;
    explicit ScopedSearchGate(const SearchGate* g) : previous(tSearchGate) {
        tSearchGate = g;
    }
    ~ScopedSearchGate() {
        tSearchGate = previous;
    }
};

void ExpandReachable(SearchResult& result, std::set<const void*>& appliedEvents, const SearchGate* gate) {
    std::set<RandoRegionId>& reachableRegions = result.regions;
    std::unordered_map<RandoRegionId, RegionTimeState>& regionTimeStates = result.regionTimes;
    bool changed = true;
    while (changed) {
        changed = false;
        auto prevSize = reachableRegions.size();
        std::set<RandoRegionId> regionsToExplore = reachableRegions;
        for (RandoRegionId regionId : regionsToExplore) {
            FindReachableRegions(regionId, reachableRegions, regionTimeStates);
        }
        for (RandoRegionId regionId : reachableRegions) {
            auto& randoRegion = Regions[regionId];
            SetCurrentRegionTime(regionTimeStates, regionId);
            for (auto& event : randoRegion.events) {
                if (gate != nullptr && !gate->Event(regionId, event.first)) {
                    continue;
                }
                if (!appliedEvents.contains(&event) && event.second()) {
                    result.state.events[event.first]++;
                    appliedEvents.insert(&event);
                    changed = true;
                }
            }
        }
        if (reachableRegions.size() != prevSize) {
            changed = true;
        }
    }
}

void CollectChecksAndExits(SearchResult& result, const SearchGate* gate, bool skipReached) {
    for (RandoRegionId regionId : result.regions) {
        auto& randoRegion = Regions[regionId];
        SetCurrentRegionTime(result.regionTimes, regionId);
        for (auto& [randoCheckId, accessLogicFunc] : randoRegion.checks) {
            if ((skipReached && result.checkReached[randoCheckId]) ||
                (gate != nullptr && !gate->Check(regionId, randoCheckId))) {
                continue;
            }
            if (accessLogicFunc.first()) {
                result.checks.push_back(randoCheckId);
                if (skipReached) {
                    result.checkReached[randoCheckId] = 1;
                }
            }
        }
        for (auto& [entrance, regionExit] : randoRegion.exits) {
            if (regionExit.condition()) {
                result.exits.push_back({ .fromRegion = regionId,
                                         .entrance = entrance,
                                         .toRegion = GetRegionIdFromEntrance(entrance),
                                         .timeSlices = LS_REGION_TIME });
            }
        }
    }
}
}

SearchResult Search(const State& state, const std::vector<Arrival>& arrivals, const SearchGate* gate) {
    SearchResult result;
    result.state = state;
    ScopedState scope(result.state);
    ScopedSearchGate scopedGate(gate);

    bool firstArrival = true;
    for (const Arrival& arrival : arrivals) {
        result.regions.insert(arrival.region);
        if (arrival.timeSlices != kArrivalDefaultTime) {
            result.regionTimes[arrival.region] = { .timeSlices = arrival.timeSlices,
                                                   .canStayOverTime = Regions[arrival.region].canStayOverTime };
        } else if (firstArrival) {
            result.regionTimes = InitializeRegionTimeStates(arrival.region);
        }
        firstArrival = false;
    }

    std::set<const void*> appliedEvents;
    ExpandReachable(result, appliedEvents, gate);
    CollectChecksAndExits(result, gate, false);
    return result;
}

void SearchResume(SearchResult& result, const State& state, const std::vector<Arrival>& arrivals,
                  const SearchGate* gate) {
    u8 events[RE_MAX];
    memcpy(events, result.state.events, sizeof(events));
    result.state = state;
    memcpy(result.state.events, events, sizeof(events));
    ScopedState scope(result.state);
    ScopedSearchGate scopedGate(gate);
    if (result.checkReached.empty()) {
        result.checkReached.assign(RC_MAX, 0);
    }

    bool firstArrival = true;
    for (const Arrival& arrival : arrivals) {
        result.regions.insert(arrival.region);
        RegionTimeState incoming;
        if (arrival.timeSlices != kArrivalDefaultTime) {
            incoming = { .timeSlices = arrival.timeSlices, .canStayOverTime = Regions[arrival.region].canStayOverTime };
        } else if (firstArrival) {
            incoming = InitializeRegionTimeStates(arrival.region)[arrival.region];
        } else {
            firstArrival = false;
            continue;
        }
        firstArrival = false;
        auto it = result.regionTimes.find(arrival.region);
        if (it == result.regionTimes.end()) {
            result.regionTimes[arrival.region] = incoming;
        } else {
            it->second = MergeTimeStates(it->second, incoming);
        }
    }

    ExpandReachable(result, result.appliedEvents, gate);
    result.exits.clear();
    CollectChecksAndExits(result, gate, true);
}

}
}
