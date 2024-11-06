#include "aiLibrary.h"
#include "ecsTypes.h"
#include "aiUtils.h"
#include "math.h"
#include "raylib.h"
#include "blackboard.h"
#include <algorithm>
#include <iostream>

struct CompoundNode : public BehNode
{
  std::vector<BehNode*> nodes;

  virtual ~CompoundNode()
  {
    for (BehNode *node : nodes)
      delete node;
    nodes.clear();
  }

  CompoundNode &pushNode(BehNode *node)
  {
    nodes.push_back(node);
    return *this;
  }
};

struct Sequence : public CompoundNode
{
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    for (BehNode *node : nodes)
    {
      BehResult res = node->update(ecs, entity, bb);
      if (res != BEH_SUCCESS)
        return res;
    }
    return BEH_SUCCESS;
  }
};

struct Selector : public CompoundNode
{
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    for (BehNode *node : nodes)
    {
      BehResult res = node->update(ecs, entity, bb);
      if (res != BEH_FAIL)
        return res;
    }
    return BEH_FAIL;
  }
};

struct UtilitySelector : public BehNode
{
  std::vector<std::pair<BehNode*, utility_function>> utilityNodes;
  std::vector<float> utilityMultipliers;

  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    if (utilityMultipliers.empty()) {
      utilityMultipliers.reserve(utilityNodes.size());
      for (int i = 0; i < utilityNodes.size(); ++i) {
        utilityMultipliers.push_back(1.0f);
      }
    } else {
      for (int i = 0; i < utilityMultipliers.size(); ++i) {
        utilityMultipliers[i] = std::max(1.0f, utilityMultipliers[i] - 0.1f);
      }
    }

    std::vector<std::pair<float, size_t>> utilityScores;
    for (size_t i = 0; i < utilityNodes.size(); ++i)
    {
      const float utilityScore = utilityNodes[i].second(bb) * utilityMultipliers[i];
      utilityScores.push_back(std::make_pair(utilityScore, i));
    }
    std::sort(utilityScores.begin(), utilityScores.end(), [](auto &lhs, auto &rhs)
    {
      return lhs.first > rhs.first;
    });
    for (const std::pair<float, size_t> &node : utilityScores)
    {
      size_t nodeIdx = node.second;
      BehResult res = utilityNodes[nodeIdx].first->update(ecs, entity, bb);
      if (res != BEH_FAIL) {
        utilityMultipliers[nodeIdx] = 1.5f;
        return res;
      }
    }
    return BEH_FAIL;
  }
};

struct MoveToEntity : public BehNode
{
  size_t entityBb = size_t(-1); // wraps to 0xff...
  float distance;
  MoveToEntity(flecs::entity entity, float in_dist, const char *bb_name) : distance(in_dist)
  {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }

  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_RUNNING;
    entity.insert([&](Action &a, const Position &pos)
    {
      flecs::entity targetEntity = bb.get<flecs::entity>(entityBb);
      if (!ecs.is_valid(targetEntity) || !ecs.is_alive(targetEntity))
      {
        res = BEH_FAIL;
        return;
      }
      targetEntity.get([&](const Position &target_pos)
      {
        if (dist(pos,target_pos) > distance)
        {
          a.action = move_towards(pos, target_pos);
          res = BEH_RUNNING;
        }
        else
          res = BEH_SUCCESS;
      });
    });
    return res;
  }
};

struct IsLowHp : public BehNode
{
  float threshold = 0.f;
  IsLowHp(float thres) : threshold(thres) {}

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &) override
  {
    BehResult res = BEH_SUCCESS;
    entity.get([&](const Hitpoints &hp)
    {
      res = hp.hitpoints < threshold ? BEH_SUCCESS : BEH_FAIL;
    });
    return res;
  }
};

struct IsEntityNear : public BehNode
{
  size_t targetEntityBb = size_t(-1);
  float distance = 0.f;

  IsEntityNear( flecs::entity entity, const char *bb_target_name, float in_dist) : distance(in_dist) {
     targetEntityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_target_name);
  }

  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_FAIL;
    flecs::entity target = bb.get<flecs::entity>(targetEntityBb);
    if (!ecs.is_valid(target) || !ecs.is_alive(target)) {
      return res;
    }
    const Position* curPosition = entity.get<Position>();
    const Position* targetPosition = target.get<Position>();

    if (dist(*curPosition, *targetPosition) <= distance) {
      res = BEH_SUCCESS;
    }
    return res;
  }
};

struct FindEnemy : public BehNode
{
  size_t entityBb = size_t(-1);
  float distance = 0;
  FindEnemy(flecs::entity entity, float in_dist, const char *bb_name) : distance(in_dist)
  {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_FAIL;
    static auto enemiesQuery = ecs.query<const Position, const Team>();
    entity.insert([&](const Position &pos, const Team &t)
    {
      flecs::entity closestEnemy;
      float closestDist = FLT_MAX;
      Position closestPos;
      enemiesQuery.each([&](flecs::entity enemy, const Position &epos, const Team &et)
      {
        if (t.team == et.team)
          return;
        float curDist = dist(epos, pos);
        if (curDist < closestDist)
        {
          closestDist = curDist;
          closestPos = epos;
          closestEnemy = enemy;
        }
      });
      if (ecs.is_valid(closestEnemy) && closestDist <= distance)
      {
        bb.set<flecs::entity>(entityBb, closestEnemy);
        res = BEH_SUCCESS;
      }
    });
    return res;
  }
};

struct Flee : public BehNode
{
  size_t entityBb = size_t(-1);
  Flee(flecs::entity entity, const char *bb_name)
  {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }

  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_RUNNING;
    entity.insert([&](Action &a, const Position &pos)
    {
      flecs::entity targetEntity = bb.get<flecs::entity>(entityBb);
      if (!ecs.is_valid(targetEntity) || !ecs.is_alive(targetEntity))
      {
        res = BEH_FAIL;
        return;
      }
      targetEntity.get([&](const Position &target_pos)
      {
        a.action = inverse_move(move_towards(pos, target_pos));
      });
    });
    return res;
  }
};

struct Patrol : public BehNode
{
  size_t pposBb = size_t(-1);
  float patrolDist = 1.f;
  Patrol(flecs::entity entity, float patrol_dist, const char *bb_name)
    : patrolDist(patrol_dist)
  {
    pposBb = reg_entity_blackboard_var<Position>(entity, bb_name);
    entity.insert([&](Blackboard &bb, const Position &pos)
    {
      bb.set<Position>(pposBb, pos);
    });
  }

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_RUNNING;
    entity.insert([&](Action &a, const Position &pos)
    {
      Position patrolPos = bb.get<Position>(pposBb);
      if (dist(pos, patrolPos) > patrolDist)
        a.action = move_towards(pos, patrolPos);
      else
        a.action = GetRandomValue(EA_MOVE_START, EA_MOVE_END - 1); // do a random walk
    });
    return res;
  }
};

struct RandomMove : public BehNode
{
  BehResult update(flecs::world &, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_RUNNING;
    entity.insert([&](Action &a, const Position &pos)
    {
      a.action = GetRandomValue(EA_MOVE_START, EA_MOVE_END - 1); // do a random walk
    });
    return res;
  }
};

struct PatchUp : public BehNode
{
  float hpThreshold = 100.f;
  PatchUp(float threshold) : hpThreshold(threshold) {}

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &) override
  {
    BehResult res = BEH_SUCCESS;
    entity.insert([&](Action &a, Hitpoints &hp)
    {
      if (hp.hitpoints >= hpThreshold)
        return;
      res = BEH_RUNNING;
      a.action = EA_HEAL_SELF;
    });
    return res;
  }
};

struct PatchUpTarget : public BehNode
{
  size_t entityBb = size_t(-1);
  float hpThreshold = 100.f;
  PatchUpTarget(flecs::entity entity, float threshold, const char *bb_name) : hpThreshold(threshold) {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_SUCCESS;
    flecs::entity target = bb.get<flecs::entity>(entityBb);
    entity.insert([&](Action &a)
    {
      target.insert([&](Hitpoints &target_hp) {
        if (target_hp.hitpoints >= hpThreshold)
          return;
        res = BEH_RUNNING;
        a.action = EA_HEAL_TARGET;
      });
    });
    return res;
  }
};

struct IsEntityLowHP : public BehNode
{
  size_t entityBb = size_t(-1);
  float hpThreshold;
  IsEntityLowHP(flecs::entity entity, float threshold, const char *bb_name) : hpThreshold(threshold) {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_FAIL;
    flecs::entity target = bb.get<flecs::entity>(entityBb);
    if (!target.is_valid()) {
      return res;
    }
    target.insert([&](const Hitpoints& hp) {
      if (hp.hitpoints < hpThreshold) {
        res = BEH_SUCCESS;
      }
    });
    return res;
  }
};
struct Not : public BehNode
{
  BehNode* node;

  Not(BehNode* node) : node(node) {}

  virtual ~Not()
  {
    delete node;
  }

  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = node->update(ecs, entity, bb);
    if (res == BEH_RUNNING) {
      return res;
    }
    return res == BEH_SUCCESS ? BEH_FAIL : BEH_SUCCESS;
  }
};

BehNode *sequence(const std::vector<BehNode*> &nodes)
{
  Sequence *seq = new Sequence;
  for (BehNode *node : nodes)
    seq->pushNode(node);
  return seq;
}

BehNode *selector(const std::vector<BehNode*> &nodes)
{
  Selector *sel = new Selector;
  for (BehNode *node : nodes)
    sel->pushNode(node);
  return sel;
}

BehNode *utility_selector(const std::vector<std::pair<BehNode*, utility_function>> &nodes)
{
  UtilitySelector *usel = new UtilitySelector;
  usel->utilityNodes = std::move(nodes);
  return usel;
}

BehNode *move_to_entity(flecs::entity entity, float in_dist, const char *bb_name)
{
  return new MoveToEntity(entity, in_dist, bb_name);
}

BehNode *is_low_hp(float thres)
{
  return new IsLowHp(thres);
}

BehNode *find_enemy(flecs::entity entity, float dist, const char *bb_name)
{
  return new FindEnemy(entity, dist, bb_name);
}

BehNode *flee(flecs::entity entity, const char *bb_name)
{
  return new Flee(entity, bb_name);
}

BehNode *patrol(flecs::entity entity, float patrol_dist, const char *bb_name)
{
  return new Patrol(entity, patrol_dist, bb_name);
}

BehNode *patch_up(float thres)
{
  return new PatchUp(thres);
}

BehNode *is_entity_near(flecs::entity entity, float in_dist, const char *bb_name) {
  return new IsEntityNear(entity, bb_name, in_dist);
}
BehNode *patch_up_target(flecs::entity entity, float thres, const char *bb_name) 
{
   return new PatchUpTarget(entity, thres, bb_name);
}

BehNode *my_not(BehNode *node) {
  return new Not(node);
}

BehNode *random_move() {
  return new RandomMove();
}

BehNode *is_entity_low_hp(flecs::entity entity, float thres, const char *bb_name)
{
  return new IsEntityLowHP(entity, thres, bb_name);
}