#include "ara.h"
#include "raylib.h"
#include <cstdint>
#include <iostream>
#include <cmath>

void ARA::improve_path() {
    while (!m_opened.empty() && m_g[coord_to_idx(m_to)] > m_opened.top().fvalue) {
        PositionWithFvalue bestToExpand = m_opened.top();
        m_opened.pop();

        Position curPos = bestToExpand.pos;
        m_closed[coord_to_idx(curPos)] = m_closed_marker;
        m_sum_expanded += 1;
        m_now_expanded += 1;
        auto checkNeighbour = [&](Position p)
        {
            // out of bounds
            if (p.x < 0 || p.y < 0 || p.x >= int(m_width) || p.y >= int(m_height))
                return;
            int idx = coord_to_idx(p);
            // not empty
            if (m_input[idx] == '#')
                return;
            float edgeWeight = m_input[idx] == 'o' ? 10.f : 1.f;
            float gScore = m_g[coord_to_idx(curPos)] + 1.f * edgeWeight; // we're exactly 1 unit away
            if (m_g[idx] > gScore)
            {
                m_g[idx] = gScore;
                if (m_closed[idx] != m_closed_marker) {
                    m_opened.push({p, fvalue(p, m_weight)});
                } else {
                    m_incons.insert(coord_to_idx(p));
                }
                m_prev[idx] = curPos;
                m_currentFvalueMin = std::min(m_currentFvalueMin, fvalue(p, 1));
            }
        };
        checkNeighbour({curPos.x + 1, curPos.y + 0});
        checkNeighbour({curPos.x - 1, curPos.y + 0});
        checkNeighbour({curPos.x + 0, curPos.y + 1});
        checkNeighbour({curPos.x + 0, curPos.y - 1});
    }
}

void ARA::try_improve_path() 
{
    improve_path();
    
    // Print logs
    m_e = std::min(m_weight, m_g[coord_to_idx(m_to)] / m_currentFvalueMin);
    std::cout << "ARA e' = " << m_e << "  weight = " << m_weight << "\n";
    std::cout << "num opened = " << m_opened.size() << " num incons = " << m_incons.size() << "\n";
    std::cout << "[path cost = " << calcPathCost(get_path()) << " sum_expanded = " << m_sum_expanded << " now_expanded = " << m_now_expanded << "]\n";
    m_now_expanded = 0;

    // decrease weight
    m_weight -= m_weight_decrease;
    m_weight = std::max(m_weight, 1.f);

    // Update the priorities for all s in OPEN according to fvalue(s)
    std::vector<Position> openedPositions;
    openedPositions.reserve(m_opened.size());
    while(!m_opened.empty()) {
        openedPositions.push_back(m_opened.top().pos);
        m_opened.pop();
    }
    for (const Position& pos: openedPositions) {
        m_opened.push({pos, fvalue(pos, m_weight)});
    }

    // Move states from INCONS into OPEN (correct weight)
    for (const int idx : m_incons) {
        Position pos = idx_to_pos(idx);
        m_opened.push({pos, fvalue(pos, m_weight)});
    }
    m_incons.clear();

    // CLOSED = empty
    m_closed_marker += 1;
}

std::vector<Position> ARA::get_path()
{
    Position curPos = m_to;
    std::vector<Position> res = {curPos};
    while (m_prev[coord_to_idx(curPos)] != Position{-1, -1})
    {
        curPos = m_prev[coord_to_idx(curPos)];
        res.insert(res.begin(), curPos);
    }
    return res;
}

void ARA::draw_expanded() {
    for (int i = 0; i < m_closed.size(); ++i) {
        if (m_closed[i] == m_closed_marker - 1) { // closed in last improve
            Position expanded_pos = idx_to_pos(i);
            const Rectangle rect = {float(expanded_pos.x), float(expanded_pos.y), 1.f, 1.f};
            DrawRectangleRec(rect, Color{uint8_t(m_g[i]), uint8_t(m_g[i]), 0, 100});
        }
    }
}