#include "hierarchicalPathfinder.h"
#include "ecsTypes.h"
// #include "pathfinder.h"
// #include "dungeonUtils.h"
#include <queue>
#include <iostream>

size_t coord_to_idx(int x, int y, size_t w)
{
  return size_t(y) * w + size_t(x);
}

struct CandidateForExpand
{
    IVec2 pos;
    int cost;
};

class Compare
{
public:
    bool operator() (const CandidateForExpand& a, const CandidateForExpand& b)
    {
        return a.cost > b.cost;
    }
};

std::vector<int> HierarchicalPathFinder::calc_distances_inside_tile(std::vector<InitialToExpand> froms, DungeonData& dungeon_data, IVec2 tile_pos, int tile_size) {
    int tile_pos_x = tile_pos.x;
    int tile_pos_y = tile_pos.y;

    IVec2 lim_min{int((tile_pos_x + 0) * tile_size), int((tile_pos_y + 0) * tile_size)};
    IVec2 lim_max{int((tile_pos_x + 1) * tile_size), int((tile_pos_y + 1) * tile_size)};
    IVec2 tile_offset = IVec2{tile_pos_x * tile_size, tile_pos_y * tile_size};

    std::vector<int> dists(tile_size * tile_size, std::numeric_limits<int>::max()); // inside current tile
    std::priority_queue<CandidateForExpand, std::vector<CandidateForExpand>, Compare> candidates;

    for (const InitialToExpand& from : froms) {
        if (from.pos.x < lim_min.x || from.pos.y < lim_min.y || from.pos.x >= lim_max.x || from.pos.y >= lim_max.y)
            continue;
        if (dungeon_data.tiles[coord_to_idx(from.pos.x,from.pos.y, dungeon_data.width)] == dungeon::wall)
            continue;
        IVec2 pos_inside_tile = from.pos - tile_offset;
        size_t idx_inside_tile = coord_to_idx(pos_inside_tile.x, pos_inside_tile.y, tile_size);
        dists[idx_inside_tile] = 1 + from.costAddition;
        candidates.push({from.pos, 1 + from.costAddition});
    }
    while (!candidates.empty()) {
        CandidateForExpand expanded = candidates.top();
        candidates.pop();

        auto checkNeighbour = [&](IVec2 p)
        {
            // out of bounds
            if (p.x < lim_min.x || p.y < lim_min.y || p.x >= lim_max.x || p.y >= lim_max.y)
                return;
        
            // not empty
            size_t idx = coord_to_idx(p.x, p.y, dungeon_data.width);
            if (dungeon_data.tiles[idx] == dungeon::wall)
                return;

            IVec2 p_inside_tile = p - tile_offset;
            size_t idx_inside_tile = coord_to_idx(p_inside_tile.x, p_inside_tile.y, tile_size);
            if (expanded.cost + 1 < dists[idx_inside_tile]) {
                dists[idx_inside_tile] = expanded.cost + 1;
                candidates.push({p, expanded.cost + 1});
            }
        };
        checkNeighbour({expanded.pos.x + 1, expanded.pos.y + 0});
        checkNeighbour({expanded.pos.x - 1, expanded.pos.y + 0});
        checkNeighbour({expanded.pos.x + 0, expanded.pos.y + 1});
        checkNeighbour({expanded.pos.x + 0, expanded.pos.y - 1});
    }
    return dists;
}
void HierarchicalPathFinder::connect_portal(size_t new_portal_idx, IVec2 portal_pos, DungeonPortals& portals, DungeonData& dungeon_data) 
{
    const int tile_size = portals.tileSplit;
    const int tile_pos_x = portal_pos.x / tile_size;
    const int tile_pos_y = portal_pos.y / tile_size;
    std::vector<int> dists_to_new_portal = calc_distances_inside_tile({{portal_pos, 0}}, dungeon_data, IVec2{tile_pos_x, tile_pos_y}, tile_size);

    const size_t width = dungeon_data.width / tile_size;
    const IVec2 lim_min{int((tile_pos_x + 0) * tile_size), int((tile_pos_y + 0) * tile_size)};
    const IVec2 lim_max{int((tile_pos_x + 1) * tile_size), int((tile_pos_y + 1) * tile_size)};
    const size_t tile_index = tile_pos_x + tile_pos_y * width;
    const IVec2 tile_offset = IVec2{tile_pos_x * tile_size, tile_pos_y * tile_size};
    
    const std::vector<size_t> &same_tile_other_portals_indices = portals.tilePortalsIndices[tile_index];
    for (const size_t other_idx : same_tile_other_portals_indices) {
        PathPortal& other_portal = portals.portals[other_idx];

        int min_dist_to_new_portal = std::numeric_limits<int>::max();
        bool hasPath = true;

        for (int fromY = std::max(other_portal.startY, size_t(lim_min.y));
                fromY <= std::min(other_portal.endY, size_t(lim_max.y - 1)) && hasPath; ++fromY)
        {
            for (int fromX = std::max(other_portal.startX, size_t(lim_min.x));
                    fromX <= std::min(other_portal.endX, size_t(lim_max.x - 1)) && hasPath; ++fromX)
            {
                IVec2 pos_inside_tile = IVec2{fromX, fromY} - tile_offset;
                int idx_inside_tile = coord_to_idx(pos_inside_tile.x, pos_inside_tile.y, tile_size);

                if (dists_to_new_portal[idx_inside_tile] == std::numeric_limits<int>::max()) {
                    hasPath = false;
                    break;
                }

                if (dists_to_new_portal[idx_inside_tile] < min_dist_to_new_portal) {
                    min_dist_to_new_portal = dists_to_new_portal[idx_inside_tile];
                }
            }
        }
        if (hasPath) {
            other_portal.conns.push_back({new_portal_idx, (float)min_dist_to_new_portal});
            portals.portals[new_portal_idx].conns.push_back({other_idx, (float)min_dist_to_new_portal});
        }
    }

    portals.tilePortalsIndices[tile_index].push_back(new_portal_idx);
}

void HierarchicalPathFinder::disconnect_portal(size_t portal_idx, DungeonPortals& portals, IVec2 portal_pos, size_t dd_width) {
    int tile_size = portals.tileSplit;
    const size_t width = dd_width / tile_size;
    const int tile_pos_x = portal_pos.x / tile_size;
    const int tile_pos_y = portal_pos.y / tile_size;
    const size_t tile_index = tile_pos_x + tile_pos_y * width;

    std::vector<PortalConnection>& removable_conns = portals.portals[portal_idx].conns;
    for (PortalConnection& removable_connection : removable_conns) {
        std::vector<PortalConnection>& other_conns = portals.portals[removable_connection.connIdx].conns;

        for (int i = 0; i < other_conns.size(); ++i) {
            if (other_conns[i].connIdx == portal_idx) {
                other_conns.erase(other_conns.begin() + i);
                break;
            }
        }
    }
    removable_conns.clear();
    for (int i = 0; i < portals.tilePortalsIndices[tile_index].size(); ++i) {
        if (portals.tilePortalsIndices[tile_index][i] == portal_idx) {
            portals.tilePortalsIndices[tile_index].erase(portals.tilePortalsIndices[tile_index].begin() + i);
        }
    }
}

float HierarchicalPathFinder::heuristic(size_t first_portal_idx, size_t second_portal_idx, DungeonPortals& portals) {
    PathPortal& first = portals.portals[first_portal_idx];
    PathPortal& second = portals.portals[second_portal_idx];

    int x_dist = std::min(std::abs((int)first.startX - (int)second.endX), std::abs((int)second.startX - (int)first.endX));
    int y_dist = std::min(std::abs((int)first.startY - (int)second.endY), std::abs((int)second.startY - (int)first.endY));

    return x_dist + y_dist;
}
std::vector<int> HierarchicalPathFinder::reconstruct_path(const std::vector<int>& prev, size_t to_portal_idx) {
  int curIdx = to_portal_idx;
  std::vector<int> res = {curIdx};
  while (prev[curIdx] != -1)
  {
    curIdx = prev[curIdx];
    res.insert(res.begin(), curIdx);
  }
  return res;
}
std::vector<int> HierarchicalPathFinder::find_path_a_star(DungeonPortals& portals, size_t from_portal_idx, size_t to_portal_idx)
{
  std::vector<float> g(portals.portals.size(), std::numeric_limits<float>::max());
  std::vector<float> f(portals.portals.size(), std::numeric_limits<float>::max());
  std::vector<int> prev(portals.portals.size(), -1);

  std::vector<size_t> openList = {from_portal_idx};
  std::vector<size_t> closedList;

  g[from_portal_idx] = 0;
  f[from_portal_idx] = heuristic(from_portal_idx, to_portal_idx, portals);

  while (!openList.empty())
  {
    size_t bestIdx = 0;
    float bestScore = f[openList[0]];
    for (size_t i = 1; i < openList.size(); ++i)
    {
      float score = f[openList[i]];
      if (score < bestScore)
      {
        bestIdx = i;
        bestScore = score;
      }
    }
    size_t expanded_idx = openList[bestIdx];
    if (expanded_idx == to_portal_idx) {
        return reconstruct_path(prev, to_portal_idx);
    }
    PathPortal& curPortal = portals.portals[expanded_idx];
    openList.erase(openList.begin() + bestIdx);

    auto checkNeighbour = [&](size_t neighbour_idx, int cost)
    {
      float gScore = g[expanded_idx] + cost;
      if (gScore < g[neighbour_idx])
      {
        prev[neighbour_idx] = expanded_idx;
        g[neighbour_idx] = gScore;
        f[neighbour_idx] = gScore + heuristic(neighbour_idx, to_portal_idx, portals);
        openList.emplace_back(neighbour_idx);
      }
    };
    for (const PortalConnection& connection : curPortal.conns) {
        checkNeighbour(connection.connIdx, connection.score);
    }
  }
  // empty path
  return std::vector<int>();
}

 std::vector<IVec2> HierarchicalPathFinder::get_portal_ceils(DungeonPortals& dp, int portal_idx, int dd_width) {
    PathPortal& portal = dp.portals[portal_idx];

    const int tile_size = dp.tileSplit;
    const size_t width = dd_width / tile_size;

    std::vector<IVec2> portal_ceils;

    for (int fromY = portal.startY; fromY <= portal.endY; ++fromY)
    {
        for (int fromX = portal.startX; fromX <= portal.endX; ++fromX)
        {
            portal_ceils.push_back(IVec2{fromX, fromY});
        }
    }
    return portal_ceils;
 }
void HierarchicalPathFinder::find_path(DungeonPortals &dp, DungeonData &dd, IVec2 from, IVec2 to) 
{
    if (!m_initialized) {
        // reserve indices for start and end points
        m_start_portal_idx = dp.portals.size();
        dp.portals.push_back({});

        m_end_portal_idx = dp.portals.size();
        dp.portals.push_back({});

        m_start = {-1, -1};
        m_end   = {-1, -1};
        m_initialized = true;
    }
    bool need_update = false;

    if (from != m_start) {
        disconnect_portal(m_start_portal_idx, dp, m_start, dd.width);

        m_start = from;
        dp.portals[m_start_portal_idx].startX = from.x;
        dp.portals[m_start_portal_idx].startY = from.y;
        dp.portals[m_start_portal_idx].endX = from.x;
        dp.portals[m_start_portal_idx].endY = from.y;
        connect_portal(m_start_portal_idx, from, dp, dd);
        need_update = true;
    }
    if (to != m_end) {
        disconnect_portal(m_end_portal_idx, dp, m_end, dd.width);

        m_end = to;
        dp.portals[m_end_portal_idx].startX = to.x;
        dp.portals[m_end_portal_idx].startY = to.y;
        dp.portals[m_end_portal_idx].endX = to.x;
        dp.portals[m_end_portal_idx].endY = to.y;
        connect_portal(m_end_portal_idx, to, dp, dd);
        need_update = true;
    }
    if (need_update && m_start != IVec2{-1, -1} && m_end != IVec2{-1, -1}) {
        m_cached_path = find_path_a_star(dp, m_start_portal_idx, m_end_portal_idx);

        m_cached_tiles_dists.clear();
        std::unordered_map<int, std::vector<InitialToExpand>> expansion_map;
        /** 
          rough_path_cost нужен для того чтобы при выборе поклеточного пути учитывалось насколько близко портал к точке назначения
          как будто это эвристика, но прикинутая через грубый путь по графам
          но из-за того что грубый путь не точный, то может возникнуть ситуация (наверное), что бот будет выходить из портала
          и возвращаться обратно в него и так он на одном месте застрянет (если представить что мышка это бот)
          ну и я это пытался решить с помощью distination_tile_idx и т.д закоменченное что
          но там тоже при определенных генерациях возникают страшные кейсы
          ну и я подумал зачем я это вообще делаю, ведь бот это не мышка и он может запомнить порталы по которым он ходил и все будет норм,
          а то что он протупит это последствие прикидки пути через порталы
          p.s. я мог вообще невнятный комментарий щас написать, уже 6 часов утра, я не соображаю
        */
        int rough_path_cost = 0;
        // int distination_tile_idx = -1; //coord_to_idx(int(m_end.x / dp.tileSplit), int(m_end.y / dp.tileSplit), (dd.width / dp.tileSplit));
        // int next_destination_tile_idx;
        for (int i = m_cached_path.size() - 1; i >= 0; --i) {
            int portal_idx = m_cached_path[i];
            std::vector<IVec2> portal_ceils = get_portal_ceils(dp, portal_idx, dd.width);
            
            for (IVec2 ceil : portal_ceils) {
                IVec2 tile_pos = IVec2{(int)(ceil.x / dp.tileSplit), (int)(ceil.y / dp.tileSplit)};
                int tile_idx = tile_pos.x + tile_pos.y * (dd.width / dp.tileSplit);
                if (expansion_map.find(tile_idx) == expansion_map.end()) {
                    expansion_map[tile_idx] = {};
                }
                // if (distination_tile_idx != tile_idx || portal_idx == m_start_portal_idx) {
                expansion_map[tile_idx].push_back({ceil, rough_path_cost});
                    // next_destination_tile_idx = tile_idx;
                // } 
            }
            // distination_tile_idx = next_destination_tile_idx;
            if (i > 0) {
                float transit_next_cost;
                for (const auto& connection : dp.portals[portal_idx].conns) {
                    if (connection.connIdx == m_cached_path[i - 1]) {
                        transit_next_cost = connection.score;
                        break;
                    }
                }
                rough_path_cost += transit_next_cost;
            }
        }
        for (auto& pair : expansion_map) {
            int tile_idx = pair.first;
            int width = (dd.width / dp.tileSplit);
            IVec2 tile_pos = IVec2{tile_idx % width, tile_idx / width};
            m_cached_tiles_dists[tile_idx] = calc_distances_inside_tile(pair.second, dd, tile_pos, dp.tileSplit);
        }
    }
}

const std::vector<int>& HierarchicalPathFinder::get_path() const {
    return m_cached_path;
}

std::vector<IVec2> HierarchicalPathFinder::get_detailed_path(IVec2 from, DungeonPortals& dp, DungeonData& dd) {
    IVec2 tile_pos = IVec2{(int)(from.x / dp.tileSplit), (int)(from.y / dp.tileSplit)};
    int tile_idx = tile_pos.x + tile_pos.y * (dd.width / dp.tileSplit);
    int tile_size = dp.tileSplit;

    if (m_cached_tiles_dists.find(tile_idx) == m_cached_tiles_dists.end()) {
        return {};
    }

    const std::vector<int>& dists_inside_tile = m_cached_tiles_dists[tile_idx];

    IVec2 tile_offset = IVec2{tile_pos.x * tile_size, tile_pos.y * tile_size};
    IVec2 negate_tile_offset = IVec2{-tile_pos.x * tile_size, -tile_pos.y * tile_size};

    IVec2 current_pos_inside_tile = from - tile_offset;
    int current_dist = dists_inside_tile[coord_to_idx(current_pos_inside_tile.x, current_pos_inside_tile.y, tile_size)];
    static std::vector<IVec2> dirs = {IVec2{1,0}, {-1, 0}, {0, 1}, {0, -1}};

    if (current_dist == std::numeric_limits<int>::max()) {
        return {};
    }
    std::vector<IVec2> detailed_path;
    detailed_path.push_back(current_pos_inside_tile - negate_tile_offset);
    bool success_step = true;
    while (success_step) {
        success_step = false;
        for (int i = 0; i < dirs.size(); ++i) {
            IVec2 p = {current_pos_inside_tile.x + dirs[i].x, current_pos_inside_tile.y  + dirs[i].y};
            if (p.x < 0 || p.y < 0 || p.x >= tile_size || p.y >= tile_size)
                continue;
            size_t idx = coord_to_idx(p.x, p.y, tile_size);
            if (dists_inside_tile[idx] != current_dist - 1)
                continue;
            
            current_pos_inside_tile = p; 
            current_dist = current_dist - 1;
            success_step = true;
            detailed_path.push_back(current_pos_inside_tile - negate_tile_offset);
            break;
        }
    }
    return detailed_path;
}