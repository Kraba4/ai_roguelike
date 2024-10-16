#include "aiLibrary.h"
#include <flecs.h>
#include "ecsTypes.h"
#include "raylib.h"
#include <cfloat>
#include <cmath>
#include <iostream>

void add_slime_sm(flecs::entity entity);
flecs::entity create_slime(flecs::world &ecs, int x, int y, float hitpoints = 100.f, int numSpawn = 1);

template<typename T>
T sqr(T a){ return a*a; }

template<typename T, typename U>
static float dist_sq(const T &lhs, const U &rhs) { return float(sqr(lhs.x - rhs.x) + sqr(lhs.y - rhs.y)); }

template<typename T, typename U>
static float dist(const T &lhs, const U &rhs) { return sqrtf(dist_sq(lhs, rhs)); }

template<typename T, typename U>
static int move_towards(const T &from, const U &to)
{
  int deltaX = to.x - from.x;
  int deltaY = to.y - from.y;
  if (abs(deltaX) > abs(deltaY))
    return deltaX > 0 ? EA_MOVE_RIGHT : EA_MOVE_LEFT;
  return deltaY < 0 ? EA_MOVE_UP : EA_MOVE_DOWN;
}

static int inverse_move(int move)
{
  return move == EA_MOVE_LEFT ? EA_MOVE_RIGHT :
         move == EA_MOVE_RIGHT ? EA_MOVE_LEFT :
         move == EA_MOVE_UP ? EA_MOVE_DOWN :
         move == EA_MOVE_DOWN ? EA_MOVE_UP : move;
}


template<typename Callable>
static void on_closest_enemy_pos(flecs::world &ecs, flecs::entity entity, Callable c)
{
  static auto enemiesQuery = ecs.query<const Position, const Team>();
  entity.insert([&](const Position &pos, const Team &t, Action &a)
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
    if (ecs.is_valid(closestEnemy))
      c(a, pos, closestEnemy);
  });
}

class MoveToEnemyState : public State
{
public:
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override
  {
    on_closest_enemy_pos(ecs, entity, [&](Action &a, const Position &pos, flecs::entity closestEnemy)
    {
      a.action = move_towards(pos, *closestEnemy.get<Position>());
    });
  }
};


class FleeFromEnemyState : public State
{
public:
  FleeFromEnemyState() {}
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override
  {
    on_closest_enemy_pos(ecs, entity, [&](Action &a, const Position &pos, flecs::entity closestEnemy)
    {
      a.action = inverse_move(move_towards(pos, *closestEnemy.get<Position>()));
    });
  }
};

class PatrolState : public State
{
  float patrolDist;
public:
  PatrolState(float dist) : patrolDist(dist) {}
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override
  {
    entity.insert([&](const Position &pos, const PatrolPos &ppos, Action &a)
    {
      if (dist(pos, ppos) > patrolDist)
        a.action = move_towards(pos, ppos); // do a recovery walk
      else
      {
        // do a random walk
        a.action = GetRandomValue(EA_MOVE_START, EA_MOVE_END - 1);
      }
    });
  }
};

class NopState : public State
{
public:
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override {}
};

class SpawnState : public State
{
public:
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override {
    entity.insert([](Spawner &spawner, const Position &pos) {
      spawner.numSpawn -= 1;
    });
    entity.insert([&](const Position &pos, const Hitpoints &hitpoints) {
      add_slime_sm(create_slime(ecs, pos.x + 1, pos.y + 1, hitpoints.hitpoints, 0));
    });
  }
};

class AttackEnemyState : public State
{
public:
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override {
    on_closest_enemy_pos(ecs, entity, [&](Action &a, const Position &pos, flecs::entity closestEnemy)
    {
      a.action = EA_ATTACK;
      entity.insert([&](RangeAttack &range_attack) {
        range_attack.target = closestEnemy;
      });
    });
  }
};

class MoveToHealerTargetState : public State
{
public:
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override
  {
    entity.insert([](Action& a, const Healer& healer, const Position& pos) {
      a.action = move_towards(pos, *healer.target.get<Position>());
    });
  }
};

class HealTargetState : public State
{
public:
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override
  {
    std::cout << "\nheal act\n";
    entity.insert([](Action& a) {
      a.action = EA_HEAL;
    });
  }
};

class HierarchicalState : public State
{
  StateMachine* sm;
public:
  HierarchicalState(StateMachine* sm) : sm(sm) {};
  ~HierarchicalState() { delete sm; };
  void enter() const override {
    sm->setCurStateIdx(0);
  }
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override
  {
    sm->act(0.f, ecs, entity);
  }
};

class MoveToPositionState : public State
{
  Position position;
public:
  MoveToPositionState(const Position& position) : position(position) {};
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override
  {
    entity.insert([&](Action& a, const Position& pos) {
      a.action = move_towards(pos, position);
    });
  }
};

class TakeResourceState : public State
{
public:
  TakeResourceState() {};
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override
  {
    entity.insert([&](Action& a, const Position& pos) {
      a.action = EA_NOP;
    });
  }
};

class CraftState : public State
{
public:
  CraftState() {};
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override
  {
    entity.insert([&](Action& a, const Position& pos) {
      a.action = EA_NOP;
    });
  }
};

class EnemyAvailableTransition : public StateTransition
{
  float triggerDist;
public:
  EnemyAvailableTransition(float in_dist) : triggerDist(in_dist) {}
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    static auto enemiesQuery = ecs.query<const Position, const Team>();
    bool enemiesFound = false;
    entity.get([&](const Position &pos, const Team &t)
    {
      enemiesQuery.each([&](flecs::entity enemy, const Position &epos, const Team &et)
      {
        if (t.team == et.team)
          return;
        float curDist = dist(epos, pos);
        enemiesFound |= curDist <= triggerDist;
      });
    });
    return enemiesFound;
  }
};

class HitpointsLessThanTransition : public StateTransition
{
  float threshold;
public:
  HitpointsLessThanTransition(float in_thres) : threshold(in_thres) {}
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    bool hitpointsThresholdReached = false;
    entity.get([&](const Hitpoints &hp)
    {
      hitpointsThresholdReached |= hp.hitpoints < threshold;
    });
    return hitpointsThresholdReached;
  }
};

class EnemyReachableTransition : public StateTransition
{
public:
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    return false;
  }
};

class NegateTransition : public StateTransition
{
  const StateTransition *transition; // we own it
public:
  NegateTransition(const StateTransition *in_trans) : transition(in_trans) {}
  ~NegateTransition() override { delete transition; }

  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    return !transition->isAvailable(ecs, entity);
  }
};

class AndTransition : public StateTransition
{
  const StateTransition *lhs; // we own it
  const StateTransition *rhs; // we own it
public:
  AndTransition(const StateTransition *in_lhs, const StateTransition *in_rhs) : lhs(in_lhs), rhs(in_rhs) {}
  ~AndTransition() override
  {
    delete lhs;
    delete rhs;
  }

  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    return lhs->isAvailable(ecs, entity) && rhs->isAvailable(ecs, entity);
  }
};

class CanSpawnTransition : public StateTransition
{
public:
  CanSpawnTransition(){}
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    bool canSpawn;
    entity.get([&](const Spawner &spawner)
    {
      canSpawn = spawner.numSpawn > 0;
    });
    return canSpawn;
  }
};

class HealerTargetHitpointsLessThanTransition : public StateTransition
{
  float threshold;
public:
  HealerTargetHitpointsLessThanTransition(float in_threshold) : threshold(in_threshold) {}
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    bool hitpointsThresholdReached = false;
    entity.get([&](const Healer& healer){
       hitpointsThresholdReached |= healer.target.get<Hitpoints>()->hitpoints < threshold;
    });
    return hitpointsThresholdReached;
  }
};


class CooldownTransition : public StateTransition
{
public:
  CooldownTransition() {}
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    bool cooldownReady = false;
    entity.get([&](const Healer& healer){
       cooldownReady |= healer.currentCooldown == 0;
    });
    return cooldownReady;
  }
};


class TargetAvaibleTransition : public StateTransition
{
  float distance;
public:
  TargetAvaibleTransition(float distance) : distance(distance) {}
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    bool targetNear = false;
    entity.get([&](const Position &pos, const Healer &healer)
    {
        targetNear |= dist(pos, *healer.target.get<Position>()) < distance;
    });
    return targetNear;
  }
};

class PositionReachedTransition : public StateTransition
{
  Position position;
public:
  PositionReachedTransition(const Position& position) : position(position) {}
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    bool targetNear = false;
    entity.get([&](const Position &pos)
    {
        targetNear |= dist(pos, position) <= 1.f;
    });
    return targetNear;
  }
};

class AlwaysTransition : public StateTransition
{
public:
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    return true;
  }
};

// states
State *create_attack_enemy_state()
{
  return new AttackEnemyState();
}
State *create_move_to_enemy_state()
{
  return new MoveToEnemyState();
}

State *create_flee_from_enemy_state()
{
  return new FleeFromEnemyState();
}


State *create_patrol_state(float patrol_dist)
{
  return new PatrolState(patrol_dist);
}

State *create_nop_state()
{
  return new NopState();
}

State *create_spawn_state()
{
  return new SpawnState();
}

State *create_move_to_healer_target_state() {
  return new MoveToHealerTargetState();
}

State *create_heal_target_state() {
  return new HealTargetState();
}

State *create_hierarchical_state(StateMachine* sm) {
  return new HierarchicalState(sm);
}

State *create_move_to_position_state(const Position& position) {
  return new MoveToPositionState(position);
}

State *create_take_resource_state() {
  return new TakeResourceState();
}

State *create_craft_state() {
  return new CraftState();
}
// transitions
StateTransition *create_enemy_available_transition(float dist)
{
  return new EnemyAvailableTransition(dist);
}

StateTransition *create_enemy_reachable_transition()
{
  return new EnemyReachableTransition();
}

StateTransition *create_hitpoints_less_than_transition(float thres)
{
  return new HitpointsLessThanTransition(thres);
}

StateTransition *create_negate_transition(StateTransition *in)
{
  return new NegateTransition(in);
}
StateTransition *create_and_transition(StateTransition *lhs, StateTransition *rhs)
{
  return new AndTransition(lhs, rhs);
}

StateTransition *create_can_spawn_transition()
{
  return new CanSpawnTransition();
}

StateTransition *create_target_hitpoints_less_than_transition(float thres) {
  return new HealerTargetHitpointsLessThanTransition(thres);
}

StateTransition *create_cooldown_transition() {
  return new CooldownTransition();
}

StateTransition *create_target_available_transition(float dist) {
  return new TargetAvaibleTransition(dist);
}

StateTransition *create_position_reached_transition(Position position) {
  return new PositionReachedTransition(position);
}

StateTransition *create_always_transition() {
  return new AlwaysTransition();
}