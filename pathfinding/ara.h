#pragma once
#include "math.h"
#include <vector>
#include <functional>
#include <stdio.h>
#include <limits>
#include <queue>
#include <unordered_set>

struct PositionWithFvalue {
    Position pos;
    float fvalue;
};

class Compare
{
public:
    bool operator() (const PositionWithFvalue& a, const PositionWithFvalue& b)
    {
        return a.fvalue > b.fvalue;
    }
};

class ARA {
    int coord_to_idx(Position pos) { return pos.x + pos.y * m_width; }
    Position idx_to_pos(int idx) { return {idx % m_width, idx / m_width}; }
    void improve_path();
    float fvalue(Position pos, float weight) {
        return m_g[coord_to_idx(pos)] + weight * m_h(pos, m_to);
    }
    // for debug
    float calcPathCost(const std::vector<Position>& path) {
        float path_cost = 0;
        for (int i = 1; i < path.size(); ++i) {
            float edgeWeight = m_input[coord_to_idx(path[i])] == 'o' ? 10.f : 1.f;
            path_cost += edgeWeight;
        }
        return path_cost;
    }
    std::vector<float> m_g;
    std::function<float(Position, Position)> m_h;  
    const char *m_input;
    int m_width;
    int m_height;
    Position m_from;
    Position m_to;
    float m_weight;
    float m_weight_decrease;
    std::priority_queue<PositionWithFvalue, std::vector<PositionWithFvalue>, Compare> m_opened;
    std::vector<int> m_closed;
    int m_closed_marker;
    std::vector<Position> m_prev;
    std::unordered_set<int> m_incons;
    float m_currentFvalueMin;
    int m_sum_expanded;
    int m_now_expanded;
    float m_e;
public:
    ARA(const char *input, int width, int height, 
        Position from, Position to,
        float initial_weight, float weight_decrease,
        std::function<float(Position, Position)> heuristic) 
        : m_input(input), m_width(width), m_height(height), m_from(from), m_to(to), m_weight(initial_weight), m_weight_decrease(weight_decrease), m_h(heuristic) 
    {
        int input_size = width * height;
        m_g.resize(input_size, std::numeric_limits<float>::max());
        m_g[coord_to_idx(m_from)] = 0;
        m_opened.push({from, fvalue(from, m_weight)});
        m_closed.resize(input_size, 0);
        m_closed_marker = 1;
        m_prev.resize(input_size, {-1,-1});
        m_currentFvalueMin = std::numeric_limits<float>::max();
        m_sum_expanded = 0;
        m_now_expanded = 0;
        m_e = std::numeric_limits<float>::max();
    }
    void try_improve_path();
    std::vector<Position> get_path();
    void draw_expanded();
};