/*
 * LaCAM algorithm
 */
#pragma once

#include "dist_table.hpp"
#include "graph.hpp"
#include "instance.hpp"
#include "utils.hpp"

#include <iostream>
#include <algorithm>
#include <set>

// low-level search node
struct Constraint {
  std::vector<int> who;
  Vertices where;
  const int depth;
  Constraint();
  Constraint(Constraint* parent, int i, Vertex* v);  // who and where
  ~Constraint();
};

// high-level search node
struct Node {
  const Config C;
  Node* parent;

  // for low-level search
  std::vector<float> priorities;
  std::vector<int> order;
  std::queue<Constraint*> search_tree;

  Node(Config _C, DistTable& D, Node* _parent = nullptr);
  ~Node();
};
using Nodes = std::vector<Node*>;

// PIBT agent
// struct Agent {
//   const int id;
//   Vertex* v_now;   // current location
//   Vertex* v_next;  // next location
//   Agent(int _id) : id(_id), v_now(nullptr), v_next(nullptr) {}
// };
// using Agents = std::vector<Agent*>;

struct Agent; 
using Agents = std::vector<Agent*>; 
// next location candidates, for saving memory allocation
using Candidates = std::vector<std::array<Vertex*, 5> >;

// PIBT agent
struct Agent {
  int id;
  Vertex* v_now;        // current location
  Vertex* v_next;       // next location
  Vertex* v_next_best;  // best next location
  float tie_breaker;  // epsilon, tie-breaker
  bool is_constrained; //should we recurse through all the actions?
  Agents* group;        // group this belongs to
  int priority;
  // trying new vars
  Candidates C_next;                // next location candidates
  int penalty;
};

struct Planner {
  const Instance* ins;
  const Deadline* deadline;
  Deadline* opti_deadline;
  std::mt19937* MT;
  const int verbose;
  bool opti;


  // new ints
  int timestep_penalty;
  int best_penalty;
  int group_no;
  int num_grouped_agents;

  // solver utils
  const int N;  // number of agents
  const int V_size;
  DistTable D;
  Candidates C_next;                // next location candidates
  std::vector<float> tie_breakers;  // random values, used in PIBT
  Agents A;
  Agents A_copy;
  Agents occupied_now;   // for quick collision checking
  Agents occupied_next;  // for quick collision checking

  using Groups = std::vector<Agents*>;

  Groups groups;

  Planner(const Instance* _ins, const Deadline* _deadline, std::mt19937* _MT,
          int _verbose = 0);
  int calculate_penalty(Agent* ai);
  void print_penalty(const std::string& filename, int penalty);
  void refresh_lists(Agents A);
  bool addToGroup(Agent* ai, Agent* aj, bool del_group);
  std::pair<bool, int> OptiPIBT(Agents A, Agent* aj, int accumulated_penalty);
  Solution solve();
  bool get_new_config(Node* S, Constraint* M);
  bool funcPIBT(Agent* ai);
};

// main function
Solution solve(const Instance& ins, const int verbose = 0,
               const Deadline* deadline = nullptr, Deadline* opti_deadline  = nullptr, 
               bool bool_opti = false, std::mt19937* MT = nullptr);
