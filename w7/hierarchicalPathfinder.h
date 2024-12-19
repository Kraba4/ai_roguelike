#pragma once
#include <flecs.h>
#include "math.h"
#include <vector>
#include <unordered_map>
#include "dungeonUtils.h"
#include "pathfinder.h"

class HierarchicalPathFinder {
    // portals for path start and end point
    bool m_initialized = false;
    size_t m_start_portal_idx;
    size_t m_end_portal_idx;
    IVec2 m_start = {-1, -1};
    IVec2 m_end = {-1, -1};
    std::vector<int> m_cached_path;
    std::unordered_map<int, std::vector<int>> m_cached_tiles_dists;

    struct InitialToExpand {
        IVec2 pos;
        int costAddition;
    };
    std::vector<int> calc_distances_inside_tile(std::vector<InitialToExpand> froms, DungeonData& dungeon_data, IVec2 tile_pos, int tile_size);
    void connect_portal(size_t portal_idx, IVec2 portal_pos, DungeonPortals& portals, DungeonData& dungeon_data);
    void disconnect_portal(size_t portal_idx, DungeonPortals& portals, IVec2 portal_pos, size_t dd_width);
    std::vector<int> find_path_a_star(DungeonPortals& portals, size_t from_portal_idx, size_t to_portal_idx);
    std::vector<int> reconstruct_path(const std::vector<int>& prev, size_t to_portal_idx); 
    float heuristic(size_t first_portal_idx, size_t second_portal_idx, DungeonPortals& portals);
    std::vector<IVec2> get_portal_ceils(DungeonPortals& dp, int portal_idx, int dd_width);
public:
    void find_path(DungeonPortals &portals, DungeonData &dd, IVec2 from, IVec2 to);
    std::vector<IVec2> get_detailed_path(IVec2 from, DungeonPortals& dp, DungeonData& dd);
    const std::vector<int>& get_path() const;
    IVec2 getStart() { return m_start;}
    IVec2 getEnd() { return m_end;}
};