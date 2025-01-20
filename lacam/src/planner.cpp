#include "../include/planner.hpp"
#include <chrono> // Include chrono for timing

Constraint::Constraint() : who(std::vector<int>()), where(Vertices()), depth(0)
{
}

Constraint::Constraint(Constraint* parent, int i, Vertex* v)
    : who(parent->who), where(parent->where), depth(parent->depth + 1)
{
  who.push_back(i);
  where.push_back(v);
}

Constraint::~Constraint(){};

Node::Node(Config _C, DistTable& D, Node* _parent)
    : C(_C),
      parent(_parent),
      priorities(C.size(), 0),
      order(C.size(), 0),
      search_tree(std::queue<Constraint*>())
{
  search_tree.push(new Constraint());
  const auto N = C.size();

  // set priorities
  if (parent == nullptr) {
    // initialize
    for (size_t i = 0; i < N; ++i) priorities[i] = (float)D.get(i, C[i]) / N;
  } else {
    // dynamic priorities, akin to PIBT
    for (size_t i = 0; i < N; ++i) {
      if (D.get(i, C[i]) != 0) {
        priorities[i] = parent->priorities[i] + 1;
      } else {
        priorities[i] = parent->priorities[i] - (int)parent->priorities[i];
      }
    }
  }

  // set order
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(),
            [&](int i, int j) { return priorities[i] > priorities[j]; });
}

Node::~Node()
{
  while (!search_tree.empty()) {
    delete search_tree.front();
    search_tree.pop();
  }
}

Planner::Planner(const Instance* _ins, const Deadline* _deadline,
                 std::mt19937* _MT, int _verbose)
    : ins(_ins),
      deadline(_deadline),
      MT(_MT),
      verbose(_verbose),
      N(ins->N),
      V_size(ins->G.size()),
      D(DistTable(ins)),
      C_next(Candidates(N, std::array<Vertex*, 5>())),
      tie_breakers(std::vector<float>(V_size, 0)),
      A(Agents(N, nullptr)),
      occupied_now(Agents(V_size, nullptr)),
      occupied_next(Agents(V_size, nullptr))
{
}

// NEW FUNCTIONS

int Planner::calculate_penalty(Agent* ai) {
  
  const auto i = ai->id;
  // calculate ideal dist for penalty purposes
  int ideal_dist = D.get(ai->id, ai->C_next[i][0]);  // Distance to goal if taking ideal move
  int actual_dist = D.get(ai->id, ai->v_next); // Distance to goal based on suggested move
  return actual_dist - ideal_dist;
}

void Planner::print_penalty(const std::string& filename, int penalty) {
    // std::string output_dir = "code/output/";
    std::string full_filename = filename;
    
    std::ofstream outFile(full_filename, std::ios::app);  // Open in append mode
    if (!outFile) {
        std::cerr << "Error opening file: " << full_filename << "\n";
        return;
    }

    // Write start positions
    outFile << penalty << "\n";
    outFile.close();
}

void Planner::refresh_lists(Agents A){
  // clear occupied next (local list) before next optipibt iter
  for (auto a : A) {
    // clear
    if (a->v_next != nullptr){
      if (occupied_next[a->v_next->id] == a){
        occupied_next[a->v_next->id] = nullptr;
      }
      a->v_next = nullptr;
    }
  }
}

bool Planner::addToGroup(Agent* ai, Agent* aj){
  // if not in opti mode,
  // if any agent is constrained, don't add to group
  if (!opti || ai->is_constrained || aj->is_constrained){
    return false;
  }
  Agents* ng;
  if (ai->group != nullptr && aj->group == nullptr){
    ng = ai->group;
    // add aj to group- assume ai is already in g
    ng->push_back(aj);
    aj->group = ng;
    num_grouped_agents +=1;
    return true;
  } else if (aj->group != nullptr && ai->group == nullptr){
    ng = aj->group;
    ng->push_back(ai);
    ai->group = ng;
    num_grouped_agents +=1;
    return true;
  } else if (aj->group == nullptr && ai->group == nullptr){
    // neither have group
    ng = new Agents();
    ng->push_back(ai);
    ng->push_back(aj);
    ai->group = ng;
    aj->group = ng;
    groups.push_back(ng);
    num_grouped_agents +=2;
  } else if ((aj->group != nullptr && ai->group != nullptr) && aj->group != ai->group){
    // merge groups
    Agents* group1 = ai->group;
    Agents* group2 = aj->group;

    // Create a new group that contains all agents from both groups
    Agents* new_group = new Agents();
    std::set<Agent*> unique_agents;

    // Insert all agents from group1 into the set
    for (const auto& agent : *group1) {
        unique_agents.insert(agent);
    }

    // Insert all agents from group2 into the set, avoiding duplicates
    for (const auto& agent : *group2) {
        unique_agents.insert(agent);
    }

    // Reserve space in new_group to avoid unnecessary reallocations
    new_group->reserve(unique_agents.size());

    // Copy unique agents back to new_group
    for (const auto& agent : unique_agents) {
        new_group->push_back(agent);
    }

    // Update group pointers for all agents in both groups
    for (auto& agent : *group1) {
      agent->group = new_group;
    }
    for (auto& agent : *group2) {
      agent->group = new_group;
    }

    // Add the new group to the groups list
    groups.push_back(new_group);
  }
  return false;
}

std::pair<bool, int> Planner::OptiPIBT(Agents A, Agent* aj, int accumulated_penalty){

  if (is_expired(opti_deadline)){
    return std::make_pair(false, 100000);
  }

  std::sort(A.begin(), A.end(), [](Agent* a, Agent* b) {
        return a->priority < b->priority; // Ascending order
    });
  Agent* ai;
  // pick highest priority agent 
  if (aj == nullptr){
    ai = A[0];
  }
  else{
    ai = aj;
  }

  // get subset of agents
  Agents agents_subset;
  // Copy all pointers except for agent ai
  std::copy_if(A.begin(), A.end(), std::back_inserter(agents_subset), 
                [ai](Agent* agent) { return agent != ai; });  

  const auto i = ai->id;
  const auto K = ai->v_now->neighbor.size();

  // get candidates for next locations
  for (size_t k = 0; k < K; ++k) {
    auto u = ai->v_now->neighbor[k];
    C_next[i][k] = u;
    if (MT != nullptr)
      tie_breakers[u->id] = get_random_float(MT);  // set tie-breaker get_random_float(MT)
  }
  C_next[i][K] = ai->v_now;

  // sort, note: K + 1 is sufficient
  std::sort(C_next[i].begin(), C_next[i].begin() + K + 1,
            [&](Vertex* const v, Vertex* const u) {
              return D.get(i, v) + tie_breakers[v->id] <
                     D.get(i, u) + tie_breakers[u->id];
            });

  // calculate ideal dist for penalty purposes
  int ideal_dist = D.get(ai->id, C_next[i][0]);  // Distance to goal if taking ideal move
  int actual_dist;
  int round_bestp = 100000;
  bool group_exists = false;

  // counting number of skipped actions
  int n_avail_acts = 0;
  int n_skipped_acts = 0;

  
  for (size_t k = 0; k < K + 1; ++k) {
    auto u = C_next[i][k];   //include best action for completeness
    refresh_lists(A); // clear the results from the last PIBT call
    n_avail_acts += 1;
    actual_dist = D.get(ai->id, u);
    int action_penalty =  (actual_dist - ideal_dist); //0 if equal
    if (action_penalty + accumulated_penalty >= best_penalty){
      n_skipped_acts += 1;
      continue; //bad action
    }
    if (occupied_next[u->id] != nullptr && occupied_next[u->id] != ai){
      // The agent at vertex 'id' in occupied_next is not nullptr and not in A
      // and it's not the current agent
      // means it's been reserved by a previous fixed agent or by a constraint
      group_exists = false;
      for (size_t i = group_no + 1; i < groups.size(); ++i) {
        if (groups[i] == ai->group) {
            group_exists = true;
            break;
        }
      }
      if (addToGroup(occupied_next[u->id], ai) && !group_exists){
        // need to add this new group to the groups list because something new has been added
        groups.push_back(ai->group);
      }
      n_skipped_acts += 1;
      continue;
    }

    // if action is causing swap conflict, continue
    auto ak = occupied_now[u->id];
    if (ak != nullptr && ak != ai){
      // The agent at vertex 'id' in occupied_now is not nullptr and not in A
      // and it's not the current agent
      // means it's been reserved by a previous fixed agent
      // // check if we have a swap conflict
      if (ak->v_next == ai->v_now){
        group_exists = false;
        for (size_t i = group_no + 1; i < groups.size(); ++i) {
            if (groups[i] == ai->group) {
                group_exists = true;
                break;
            }
        }
        if (addToGroup(ak, ai) && !group_exists){
          // need to add this new group to the groups list because something new has been added
          groups.push_back(ai->group);
        }
        // the node we are trying to move to is currently occupied by an agent that 
        // wants to move to us or stay at u. since we are lower priority, we sacrifice this action.
        n_skipped_acts += 1;
        continue;
      }
    }
    // option 2: try recursing through ak and skip action if it doesn't improve
    occupied_next[u->id] = ai;
    ai->v_next = u; 
    // there is an unplanned agent here, let's see if we can comfortably move it
    if (ak != nullptr && ak->v_next == nullptr){
      group_exists = false;
      for (size_t i = group_no + 1; i < groups.size(); ++i) {
        if (groups[i] == ai->group) {
            group_exists = true;
            break;
        }
      }
      if (addToGroup(ak, ai) && !group_exists){
        // need to add this new group to the groups list because something new has been added
        groups.push_back(ai->group);
      }
      auto [failed, bas] = OptiPIBT(agents_subset, ak, accumulated_penalty + action_penalty);
      if (failed){
        n_skipped_acts += 1;
        // we tried moving to this action and moving other agents accordingly, but agent ak is stuck
        // ai->v_next = nullptr;
        continue;
      }
      round_bestp = std::min(round_bestp, action_penalty + bas);
      continue; // regardless, try another action now, this one has been explored already (if it's best, itll get saved at the bottom)
    }
   
    
    // run OptiPIBT recursive call if not last agent
    int best_after_subset = 0;
    bool f = true;
    if (!agents_subset.empty()) {
      // agents_subset is not empty
      std::pair<bool, int> result = OptiPIBT(agents_subset, nullptr, accumulated_penalty + action_penalty);
      if (result.first) continue;
      best_after_subset = result.second;
    } 
    round_bestp = std::min(round_bestp, action_penalty + best_after_subset);
    // this should give it some new v_next values
    if (accumulated_penalty + action_penalty + best_after_subset < best_penalty){
      best_penalty = accumulated_penalty + action_penalty + best_after_subset;
      for (auto agent : A_copy) {
        // Set the agent's v_next_best to v_next (which should be correct atm)
        agent->v_next_best = agent->v_next; 
      }
      if (best_penalty == 0){
        return std::make_pair(false, round_bestp);
      }
    }
  }
  // either all moves have failed or next best has been found
  if (n_skipped_acts == n_avail_acts){
    // failed to secure node
    // occupied_next[ai->v_now->id] = ai;
    // ai->v_next = ai->v_now;
    return std::make_pair(true, 100000);
  }
  return std::make_pair(false, round_bestp);
}

Solution Planner::solve()
{
  info(1, verbose, "elapsed:", elapsed_ms(deadline), "ms\tstart search");

  // setup agents
  for (auto i = 0; i < N; ++i){
    Agent* a = new Agent{i,                          // id
                         nullptr,                    // current location
                         nullptr,                    // next location (local)
                         nullptr,                    // next best location
                         get_random_float(MT)};      // tie-breaker
                         false;                      // constraints remembrance
    A[i] = a;
  } 

  // setup search queues
  std::stack<Node*> OPEN;
  std::unordered_map<Config, Node*, ConfigHasher> CLOSED;
  std::vector<Constraint*> GC;  // garbage collection of constraints

  // insert initial node
  auto S = new Node(ins->starts, D);
  OPEN.push(S);
  CLOSED[S->C] = S;

  // depth first search
  int loop_cnt = 0;
  std::vector<Config> solution;

  //  
  int soln_count = 0;
  while (!OPEN.empty() && !is_expired(deadline)) {
    loop_cnt += 1;

    // do not pop here!
    S = OPEN.top();

    // check goal condition
    if (is_same_config(S->C, ins->goals)) {
      // backtrack
      while (S != nullptr) {
        solution.push_back(S->C);
        S = S->parent;
      }
      std::reverse(solution.begin(), solution.end());
      break;
    }

    // low-level search end
    if (S->search_tree.empty()) {
      OPEN.pop();
      continue;
    }

    // create successors at the low-level search
    auto M = S->search_tree.front();
    GC.push_back(M);
    S->search_tree.pop();
    if (M->depth < N) {
      auto i = S->order[M->depth];
      auto C = S->C[i]->neighbor;
      C.push_back(S->C[i]);
      if (MT != nullptr) std::shuffle(C.begin(), C.end(), *MT);  // randomize
      for (auto u : C) S->search_tree.push(new Constraint(M, i, u));
    }

    // create successors at the high-level search
    if (!get_new_config(S, M)) continue;

    soln_count ++;    

    // create new configuration
    auto C = Config(N, nullptr);
    for (auto a : A) C[a->id] = a->v_next;

    // check explored list
    auto iter = CLOSED.find(C);
    if (iter != CLOSED.end()) {
      OPEN.push(iter->second);
      continue;
    }

    // insert new search node
    auto S_new = new Node(C, D, S);
    OPEN.push(S_new);
    CLOSED[S_new->C] = S_new;
  }

  info(1, verbose, "elapsed:", elapsed_ms(deadline), "ms\t",
       solution.empty() ? (OPEN.empty() ? "no solution" : "failed")
                        : "solution found",
       "\tloop_itr:", loop_cnt, "\texplored:", CLOSED.size());
  // memory management
  for (auto a : A) delete a;
  for (auto M : GC) delete M;
  for (auto p : CLOSED) delete p.second;

  return solution;
}

bool Planner::get_new_config(Node* S, Constraint* M)
{
  // setup cache
  for (auto a : A) {
    // clear previous cache
    if (a->v_now != nullptr && occupied_now[a->v_now->id] == a) {
      occupied_now[a->v_now->id] = nullptr;
    }
    if (a->v_next != nullptr) {
      occupied_next[a->v_next->id] = nullptr;
      a->v_next = nullptr;
    }

    if (a->v_next_best != nullptr) {
      // occupied_next[a->v_next_best->id] = nullptr;
      a->v_next_best = nullptr;
    }

    a->group = nullptr;
    a->is_constrained = false;

    // set occupied now
    a->v_now = S->C[a->id];
    occupied_now[a->v_now->id] = a;
  }

  // add constraints
  for (auto k = 0; k < M->depth; ++k) {
    const auto i = M->who[k];        // agent
    const auto l = M->where[k]->id;  // loc

    // check vertex collision
    if (occupied_next[l] != nullptr) return false;
    // check swap collision
    auto l_pre = S->C[i]->id;
    if (occupied_next[l_pre] != nullptr && occupied_now[l] != nullptr &&
        occupied_next[l_pre]->id == occupied_now[l]->id)
      return false;

    // set occupied_next
    A[i]->v_next = M->where[k];
    occupied_next[l] = A[i];
    A[i]->is_constrained = true;
  }

  // perform PIBT
  timestep_penalty = 0;
  int pi = 0;
  num_grouped_agents = 0;
  for (auto k : S->order) {
    auto a = A[k];
    a->priority = pi;
    if (a->v_next == nullptr && !funcPIBT(a)) return false;  // planning failure
    pi ++;
  }

  // int pen1 = 0;
  // for (auto a: A){
  //   pen1 += calculate_penalty(a);
  // }
  // print_penalty("costs.txt", pen1);

  // if opti, refine with opti-pibt
  if (opti && timestep_penalty != 0) {
    group_no = 0;
    size_t j = groups.size();
    size_t i = 0;
    opti_deadline->reset();

    // double overall_deadline = opti_deadline->time_limit_ms;

    //   
    while (j-1 != i && !is_expired(opti_deadline)) {
        // Record the start time of the iteration
        auto start_time = std::chrono::high_resolution_clock::now();

        // std::cout << "i = " << i << std::endl;
        group_no = i;
        Agents* g = groups[i];
        A_copy = *g;
        best_penalty = 100000;
        int num_agents = 0;
        int tsp = 0;

        for (auto a : A_copy) {
          tsp+= calculate_penalty(a);
          num_agents += 0;
          a->v_next_best = a->v_next; // Reserve PIBT answer for cutoff reasons
        }

        // opti_deadline->time_limit_ms = overall_deadline*(num_agents/num_grouped_agents);

        if (tsp == 0){
          continue;
        }

        OptiPIBT(A_copy, nullptr, 0);
        refresh_lists(A_copy);

        // If new groups are added, they will be processed in the next iteration
        for (auto a : A_copy) {
            occupied_next[a->v_next_best->id] = a; // Reserve
            a->v_next = a->v_next_best;
        }

        j = groups.size();
        i++;

        // Record the end time of the iteration
        // Calculate and print the elapsed time for this iteration
        // if (is_expired(opti_deadline)){
          // std::cout << "expired = " << true << std::endl;
          // auto elapsed_time_ms = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
          // std::cout << "Time taken for iteration " << i << ": " << elapsed_time_ms << " ns" << std::endl;
          // break;
        // }
    }
  }
  // int pen2 = 0;
  // for (auto a: A){
  //   pen2 += calculate_penalty(a);
  // }
  // print_penalty("costs.txt", pen2);
  groups.clear();
  return true;
}

bool Planner::funcPIBT(Agent* ai)
{
  const auto i = ai->id;
  const auto K = ai->v_now->neighbor.size();

  // get candidates for next locations
  for (size_t k = 0; k < K; ++k) {
    auto u = ai->v_now->neighbor[k];
    C_next[i][k] = u;
    if (MT != nullptr)
      tie_breakers[u->id] = get_random_float(MT);  // set tie-breaker
  }
  C_next[i][K] = ai->v_now;

  // sort, note: K + 1 is sufficient
  std::sort(C_next[i].begin(), C_next[i].begin() + K + 1,
            [&](Vertex* const v, Vertex* const u) {
              return D.get(i, v) + tie_breakers[v->id] <
                     D.get(i, u) + tie_breakers[u->id];
            });
  
  ai->C_next = C_next;

  // calculate ideal dist for penalty purposes
  int ideal_dist = D.get(ai->id, C_next[i][0]);  // Distance to goal if taking ideal move
  int actual_dist;
  int diff;

  for (size_t k = 0; k < K + 1; ++k) {
    auto u = C_next[i][k];

    // avoid vertex conflicts
     if (occupied_next[u->id] != nullptr){
      addToGroup(ai, occupied_next[u->id]);
      continue;
    } 

    auto& ak = occupied_now[u->id];

    // avoid swap conflicts with constraints (and with inherited agents)
    // this condition takes care of aj swap
    if (ak != nullptr && ak->v_next == ai->v_now){
      addToGroup(ai, ak);
      continue;
    } 

    // reserve next location
    occupied_next[u->id] = ai;
    ai->v_next = u;

    // empty or stay
    if (ak == nullptr || u == ai->v_now) return true;  //why does this second condition exist?

    // priority inheritance
    if (ak->v_next == nullptr){
      addToGroup(ai, ak);
      if (!funcPIBT(ak)) continue;
    } 

    // PENALTY FOR LOGGING PURPOSES
    actual_dist = D.get(ai->id, ai->v_next);
    diff = actual_dist - ideal_dist;
    timestep_penalty += diff; //0 if equal

    // success to plan next one step
    return true;
  }

  // failed to secure node
  occupied_next[ai->v_now->id] = ai;
  ai->v_next = ai->v_now;

  // find penalty
  actual_dist = D.get(ai->id, ai->v_next);
  diff = actual_dist - ideal_dist;
  timestep_penalty += diff; //0 if equal
  return false;
}

Solution solve(const Instance& ins, const int verbose, const Deadline* deadline,
               Deadline* opti_deadline, bool bool_opti,
               std::mt19937* MT)
{
  info(1, verbose, "elapsed:", elapsed_ms(deadline), "ms\tpre-processing");
  auto planner = Planner(&ins, deadline, MT, verbose);
  planner.opti_deadline = opti_deadline;
  planner.opti = bool_opti;
  std::cout << "opti is " << bool_opti << std::endl;
  std::cout << "deadline is " << opti_deadline->time_limit_ms << std::endl;
  return planner.solve();
}
