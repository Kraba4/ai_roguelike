#pragma once

#include "stateMachine.h"
#include "ecsTypes.h"

// states
State *create_attack_enemy_state();
State *create_move_to_enemy_state();
State *create_flee_from_enemy_state();
State *create_patrol_state(float patrol_dist);
State *create_nop_state();
State *create_spawn_state();
State *create_move_to_healer_target_state();
State *create_heal_target_state();
State *create_hierarchical_state(StateMachine* sm);
State *create_move_to_position_state(const Position& position);
State *create_take_resource_state();
State *create_craft_state();

// transitions
StateTransition *create_enemy_available_transition(float dist);
StateTransition *create_enemy_reachable_transition();
StateTransition *create_hitpoints_less_than_transition(float thres);
StateTransition *create_negate_transition(StateTransition *in);
StateTransition *create_and_transition(StateTransition *lhs, StateTransition *rhs);
StateTransition *create_can_spawn_transition();
StateTransition *create_target_hitpoints_less_than_transition(float thres);
StateTransition *create_cooldown_transition();
StateTransition *create_target_available_transition(float dist);
StateTransition *create_position_reached_transition(Position position);
StateTransition *create_always_transition();