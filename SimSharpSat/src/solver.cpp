/*
 * solver.cpp
 *
 *  Created on: Aug 23, 2012
 *      Author: marc
 */
#include "solver.h"
#include <deque>

#include <algorithm>

using std::endl;
using std::cout;

StopWatch::StopWatch()
{
  interval_length_.tv_sec = 60;
  gettimeofday(&last_interval_start_, NULL);
  start_time_ = stop_time_ = last_interval_start_;
}

timeval StopWatch::getElapsedTime()
{
  timeval r;
  timeval other_time = stop_time_;
  if (stop_time_.tv_sec == start_time_.tv_sec && stop_time_.tv_usec == start_time_.tv_usec)
    gettimeofday(&other_time, NULL);
  long int ad = 0;
  long int bd = 0;

  if (other_time.tv_usec < start_time_.tv_usec)
  {
    ad = 1;
    bd = 1000000;
  }
  r.tv_sec = other_time.tv_sec - ad - start_time_.tv_sec;
  r.tv_usec = other_time.tv_usec + bd - start_time_.tv_usec;
  return r;
}

void Solver::print(vector<LiteralID> &vec)
{
  cout << "c ";
  for (auto l : vec)
    cout << l.toInt() << " ";
  cout << endl;
}

void Solver::print(vector<unsigned> &vec)
{
  cout << "c ";
  for (auto l : vec)
    cout << l << " ";
  cout << endl;
}

// Chang's modification: print clauses
void Solver::printClauses() {
  std::cout << "cnf: ";
  int state = 1; // 0: skipping header; 1: parsing a clause
  int clsId = 0;
  for (unsigned i = 0; i < literal_pool_.size(); ) {
    if (state == 0) {
      i += ClauseHeader::overheadInLits();
      state = 1;
    }
    else if (state == 1) {
      if (literal_pool_[i] == NOT_A_LIT) {
        ++i;
        state = 0;
      }
      else {
        assert(literal_pool_[i] != NOT_A_LIT);
        cout << "no." << ++clsId << "* ";
        while (literal_pool_[i] != NOT_A_LIT) {
          cout << literal_pool_[i].toInt();
          if (!isActive(literal_pool_[i]))
            std::cout << "(" << int(literal_values_[literal_pool_[i]]) << ")";
          cout << ",";
          ++i;
        }
        cout << "; ";
        assert(literal_pool_[i] == NOT_A_LIT);
        state = 0;
        ++i;
      }
    }
  }
  // for (int var = 0; var < int(variables_.size()); ++var) {
  //   for (int phase = 0; phase <= 1; ++phase) {
  //     LiteralID litId0(var, phase);
  //     auto lit0 = literals_[litId0];
  //     auto& bl = lit0.binary_links_;
  //     if (bl.size() > 1) {
  //       for (auto bt = bl.begin(); *bt != SENTINEL_LIT; bt++) {
  //         if (abs(litId0.toInt()) <= abs(bt->toInt())) {
  //           std::cout << litId0.toInt();
  //           if (!isActive(litId0))
  //             std::cout << "(" << int(literal_values_[litId0]) << ")";
  //           std::cout << ",";
  //           std::cout << bt->toInt();
  //           if (!isActive(*bt))
  //             std::cout << "(" << int(literal_values_[*bt]) << ")";
  //           std::cout << "; ";
  //         }
  //       }
  //     }
  //   }
  // }
  for (auto& lit: unit_clauses_) {
    if (lit == SENTINEL_LIT)
      continue;
    std::cout << lit.toInt();
    if (!isActive(lit))
      std::cout << "(" << int(literal_values_[lit]) << ")";
    std::cout << "; ";
  }
  std::cout << std::endl;
}

// Chang's modification: print undecided clausess
void Solver::printUndecidedClauses(Component& top) {
  std::cout << "undecided clauses: ";
  for (auto itClsId = top.clsBegin(); *itClsId != clsSENTINEL; ++itClsId) {
    std::cout << "no." << *itClsId << "* ";
    for (auto itLit = literal_pool_.begin() + clsId2ClsOfs[*itClsId]; *itLit != NOT_A_LIT; ++itLit) {
      std::cout << itLit->toInt();
      if (!isActive(*itLit))
        std::cout << "(" << int(literal_values_[*itLit]) << ")";
      std::cout << ",";
    }
    std::cout << "; ";
  }
  std::cout << std::endl;
}

bool Solver::simplePreProcess()
{
  if (!config_.perform_pre_processing)
    return true;

  assert(literal_stack_.size() == 0);
  unsigned start_ofs = 0;

  //begin process unit clauses
  for (auto lit : unit_clauses_)
  {
    if (isUnitClause(lit.neg()))
    {
      return false;
    }
    setLiteralIfFree(lit);
  }
  //end process unit clauses

  bool succeeded = BCP(start_ofs);

  if (succeeded)
    succeeded &= prepFailedLiteralTest();

  // Chang's comment: it is very useful sometimes, but I don't know how to combine it with the circuit structure
  // if (succeeded)
  //   HardWireAndCompact();

  return succeeded;
}

// Chang's modification: disable
bool Solver::prepFailedLiteralTest()
{
  unsigned last_size;
  do
  {
    last_size = literal_stack_.size();
    for (unsigned v = 1; v < variables_.size(); v++)
      if (isActive(v))
      {
        unsigned sz = literal_stack_.size();
        // cout << "set true " << v << endl;
        setLiteralIfFree(LiteralID(v, true));
        bool res = BCP(sz);
        while (literal_stack_.size() > sz)
        {
          // unSet(literal_stack_.back());
          // Chang's modification: updated undecided vars
          auto lit = literal_stack_.back();
          unSet(lit);
          literal_stack_.pop_back();
        }

        if (!res)
        {
          sz = literal_stack_.size();
          // cout << "set false " << v << endl;
          setLiteralIfFree(LiteralID(v, false));
          if (!BCP(sz))
            return false;
        }
        else
        {

          sz = literal_stack_.size();
          // cout << "set false " << v << endl;
          setLiteralIfFree(LiteralID(v, false));
          bool resb = BCP(sz);
          while (literal_stack_.size() > sz)
          {
            // unSet(literal_stack_.back());
            // Chang's modification: updated undecided vars
            auto lit = literal_stack_.back();
            unSet(lit);
            literal_stack_.pop_back();
          }
          if (!resb)
          {
            sz = literal_stack_.size();
            // cout << "set true" << v << endl;
            setLiteralIfFree(LiteralID(v, true));
            if (!BCP(sz))
              return false;
          }
        }
      }
  } while (literal_stack_.size() > last_size);

  return true;
}

void Solver::HardWireAndCompact()
{
  compactClauses();
  compactVariables();
  literal_stack_.clear();

  for (auto l = LiteralID(1, false); l != literals_.end_lit(); l.inc())
  {
    literal(l).activity_score_ = literal(l).binary_links_.size() - 1;
    literal(l).activity_score_ += occurrence_lists_[l].size();
  }

  statistics_.num_unit_clauses_ = unit_clauses_.size();

  statistics_.num_original_binary_clauses_ = statistics_.num_binary_clauses_;
  statistics_.num_original_unit_clauses_ = statistics_.num_unit_clauses_ =
      unit_clauses_.size();
  initStack(num_variables());
  original_lit_pool_size_ = literal_pool_.size();
}

void Solver::solve(const string &file_name)
{
  stopwatch_.setTimeBound(config_.time_bound_seconds);
  srand(config_.randomseed);
  stopwatch_.start();
  statistics_.input_file_ = file_name;

  bool notfoundUNSAT = createfromFile(file_name, config_.maxPI, config_.nIsolatedPIs);

  //Found Empirically
  if (statistics_.num_original_binary_clauses_ > 0.75 * statistics_.num_original_clauses_)
  {
    config_.maxdecterminate = false;
  }

  if (config_.perform_pcc)
  {
    comp_manager_.getrandomseedforclhash();
  }

  initStack(num_variables());

  if (!config_.quiet)
  {
    cout << "c Solving " << file_name << endl;
    statistics_.printShortFormulaInfo();
  }
  if (independent_support_.size() == 0)
  {
    if (!config_.quiet)
    {
      cout << "c Sampling set not present! So doing total model counting." << endl;
    }
    config_.perform_projectedmodelcounting = false;
  }
  else if (!config_.quiet)
  {
    if (!config_.perform_projectedmodelcounting)
    {
      cout << "c Warning! Sampling set is present but projected model counting"
           << " is turned off by the user so solver is not doing projected model counting." << endl;
    }
    else
    {
      cout << "c Sampling set is present, performing projected model counting " << endl;
    }
    cout << "c Sampling set size: " << independent_support_.size() << endl;
    cout << "c Sampling set: ";
    for (auto it = independent_support_.begin(); it != independent_support_.end(); ++it)
    {
      cout << ' ' << *it;
    }
    cout << endl;
  }
  if (!config_.quiet)
  {
    cout << "c " << endl;
    cout << "c Preprocessing .." << endl;
  }

  if (notfoundUNSAT) {
    notfoundUNSAT = simplePreProcess();
    // Chang's modification: update clause offset
    updateClauseOffset();
  }

  if (!config_.quiet) {
    cout << "c DONE" << endl;
  }

  // Chang's modificatio
  mpz_class include_freePI_solution_count_ = 0;
  if (notfoundUNSAT) {

    if (!config_.quiet) {
      statistics_.printShortFormulaInfo();
    }

    last_ccl_deletion_time_ = last_ccl_cleanup_time_ =
      statistics_.getTime();

    violated_clause.reserve(num_variables());

    comp_manager_.initialize(literals_, literal_pool_, num_variables());

    statistics_.exit_state_ = countSAT();

    if (statistics_.exit_state_ == CHANGEHASH) {
      cout << "ERROR: We need to change the hash range (-1)" << endl;
      exit(1);
    }
    if (config_.perform_projectedmodelcounting) {
      assert(0);
    } else {
      statistics_.set_final_solution_count(stack_.top().getTotalModelCount());
      mpz_mul_2exp(
        include_freePI_solution_count_.get_mpz_t (),
        statistics_.final_solution_count().get_mpz_t (),
        config_.nIsolatedPIs);
    }
    statistics_.num_long_conflict_clauses_ = num_conflict_clauses();
  } else {
    statistics_.exit_state_ = SUCCESS;
    statistics_.set_final_solution_count(0.0);
    cout << endl
         << "c FOUND UNSAT DURING PREPROCESSING " << endl;
  }

  stopwatch_.stop();
  statistics_.time_elapsed_ = stopwatch_.getElapsedSeconds();

  comp_manager_.gatherStatistics();
  string writefile;
  if (config_.perform_projectedmodelcounting) {
    writefile = "out.pmc";
  } else {
    writefile = "out.mc";
  }
//  statistics_.writeToFile(writefile, config_.perform_projectedmodelcounting);
  statistics_.printShort(config_.perform_projectedmodelcounting);
  cout << "mc including free PIs = " << include_freePI_solution_count_ << endl;
}


// Chang's modification: use Hanyu's version
SOLVER_StateT Solver::countSAT() {
  const double HASH_BOUND = pow(2, (log2(config_.delta) + 64 * config_.hashrange * 0.9843) / 2);
  retStateT state = RESOLVED;
  dynPat.resize(num_variables() + 1);
  for (auto & pat: dynPat)
    pat.resize(MAX_LEN_OF_PAT, 0);
  vector<LiteralID> var2PosLit;
  var2PosLit.resize(num_variables() + 1);
  for (int i = 0; i <= (int)num_variables(); ++i)
    var2PosLit[i] = LiteralID(i, 1);
  vector<int> undecPIs;

  // //begin process unit clauses
  // for (auto lit : unit_clauses_)
  // {
  //   if (isUnitClause(lit.neg())) {
  //     stack_.top().includeSolution(0);
  //     return SUCCESS;
  //   }
  //   setLiteralIfFree(lit);
  // }
  // //end process unit clauses

  while (true) {
    // in this case we terminate and change hash
    if (statistics_.num_cache_look_ups_ + 1 > HASH_BOUND)
      return CHANGEHASH;

    if (state == RESOLVED) {
      // in this case we reached the end and now we need to back track
      if (!comp_manager_.findNextRemainingComponentOf(stack_.top())) {
        state = BACKTRACK;
        continue;
      }

      // Chang's comment: can be optimized
      getUndecPI(undecPIs);
      int nPI = undecPIs.size();
      auto & topComp = comp_manager_.currentRemainingComponentOf(stack_.top());
      if (nPI >= 5 && nPI <= MAX_ENUM_BIT) {
        if ((nPI * nPI * 0.5) < topComp.numLongClauses()) {
          simulatePro(undecPIs, var2PosLit, topComp);
          stack_.top().includeSolution(simRes);
          if (!simRes) {
            stack_.top().set_unprocessed_components_end(stack_.top().remaining_components_ofs());
            state = BACKTRACK;
          }
          else
            stack_.top().nextUnprocessedComponent();
          continue;
        }
      }

      decideLiteral();
      while (true) {
        if (!bcp()) // found conflict
          state = resolveConflict();
        else // not found conflict
          break;
        if (state == BACKTRACK)
          break;
      }
    }

    else { // must be the BACKTRACK state
      // assert(state == BACKTRACK);
      state = backtrack();
      // might go to exit
      if (state == EXIT) {
        // cout << "t0 = " << t0 << endl << "t1 = " << t1 << endl;
        return SUCCESS;
      }
      if (state != RESTART) {
        while (true) {
          if (state == PROCESS_COMPONENT)
            break;
          // must be the second branch
          assert(stack_.top().isSecondBranch());
          if (!bcp()) { // found conflict
            state = resolveConflictOnBranch2();
            // must return BACKTRACK
            // assert(state == BACKTRACK); 
            state = backtrack();
            if (state == EXIT) {
              // cout << "t0 = " << t0 << endl << "t1 = " << t1 << endl;
              return SUCCESS;
            }
          }
          else // not found conflict
            break;
        }
      }
      state = RESOLVED;
    }
  }

  return SUCCESS;
}


void Solver::decideLiteral() {
  // establish another decision stack level
  stack_.push_back(
    StackLevel(stack_.top().currentRemainingComponent(),
               literal_stack_.size(),
               comp_manager_.component_stack_size()));

  auto it = comp_manager_.superComponentOf(stack_.top()).varsBegin();
  unsigned max_score_var = *it;
  float max_score = scoreOf(*(it));
  float score;
  if (config_.perform_projectedmodelcounting)
    assert(0);
  else if (config_.use_edr)
    assert(0);
  else {
    for (auto it =
             comp_manager_.superComponentOf(stack_.top()).varsBegin();
         *it != varsSENTINEL; it++) {
      score = scoreOf(*it);
      if (score > max_score) {
        max_score = score;
        max_score_var = *it;
      }
    }
    if (config_.use_csvsads) {
      float cachescore = comp_manager_.cacheScoreOf(max_score_var);
      for (auto it = comp_manager_.superComponentOf(stack_.top()).varsBegin();
           *it != varsSENTINEL; it++) {
        score = scoreOf(*it);
        if (score > max_score * config_.csvsads_param) {
          if (comp_manager_.cacheScoreOf(*it) > cachescore) {
            max_score_var = *it;
            cachescore = comp_manager_.cacheScoreOf(*it);
          }
        }
      }
      max_score = score;
    }
  }
  // this assert should always hold,
  // if not then there is a bug in the logic of countSAT();
  assert(max_score_var != 0);
  bool polarity = true;
  switch (config_.polarity_config) {
    case polar_false:
      polarity = false;
      break;
    case polar_true:
      polarity = true;
      break;
    case polar_default:
      polarity = literal(LiteralID(max_score_var, true)).activity_score_ > literal(LiteralID(max_score_var, false)).activity_score_;
      break;
    case polaritycache:
      polarity = literal(LiteralID(max_score_var, true)).activity_score_ >
        literal(LiteralID(max_score_var, false)).activity_score_;
      if (literal(LiteralID(max_score_var, true)).activity_score_ >
            2 * literal(LiteralID(max_score_var, false)).activity_score_) {
      polarity = true;
      } else if (literal(LiteralID(max_score_var, false)).activity_score_ >
                  2 * literal(LiteralID(max_score_var, true)).activity_score_) {
        polarity = false;
      } else if (var(max_score_var).set) {
        int random = rand() % 3;
        switch (random) {
          case 0:
            polarity = literal(LiteralID(max_score_var, true)).activity_score_ >
              literal(LiteralID(max_score_var, false)).activity_score_;
            break;
          case 1:
            polarity = var(max_score_var).polarity;
            break;
          case 2:
            polarity = var(max_score_var).polarity;
            polarity = !polarity;
            break;
        }
      }
      break;
  }
  LiteralID theLit(max_score_var, polarity);
  stack_.top().setbranchvariable(max_score_var);

  // cout << "decide literal: " << theLit.toInt() << ", level = " << stack_.get_decision_level() << endl;
    
  setLiteralIfFree(theLit);
  statistics_.num_decisions_++;
  if (config_.maxdecterminate) {
    if (statistics_.num_decisions_ > config_.maxdec &&
        statistics_.num_conflicts_ < config_.minconflicts_) {
      cout << "c Terminating solver because the number of decisions exceeds the given value of " << config_.maxdec << " and conflicts is less than " << config_.minconflicts_ << endl;
      exit(1);
    }
  }
  if (statistics_.num_decisions_ % 128 == 0) {
    if (config_.use_csvsads) {
      comp_manager_.increasecachescores();
    }
    decayActivities();
  }
  assert(
      stack_.top().remaining_components_ofs() <= comp_manager_.component_stack_size());
  if (stack_.get_decision_level() > (int)(statistics_.max_decision_level_)) {
    statistics_.max_decision_level_ = stack_.get_decision_level();
    if (statistics_.max_decision_level_ % 25 == 0) {
      cout << "c Max decision level :" << statistics_.max_decision_level_ << endl;
    }
  }
}

retStateT Solver::backtrack() {
  assert(
      stack_.top().remaining_components_ofs() <= comp_manager_.component_stack_size());

  if (config_.use_lso && statistics_.num_decisions_ >= config_.lsoafterdecisions) {
    config_.use_lso = false;
    if (!config_.quiet) {
      cout << "c Doing Restart" << endl;
    }
    do {
      if (stack_.top().branch_found_unsat() || stack_.top().anotherCompProcessible()) {
        comp_manager_.removeAllCachePollutionsOf(stack_.top());
      }
      reactivateTOS();
      stack_.pop_back();
    } while (stack_.get_decision_level() > 0);
    statistics_.num_decisions_ = 0;
    return RESTART;
  }
  if (!isindependent && config_.perform_projectedmodelcounting) {
    assert(0);
  } else {
    do {
      if (stack_.top().branch_found_unsat()) {
        comp_manager_.removeAllCachePollutionsOf(stack_.top());
      } else if (stack_.top().anotherCompProcessible()) {
        return PROCESS_COMPONENT;
      }

      if (!stack_.top().isSecondBranch()) {
        if (stack_.get_decision_level() == 1) {
          cout << "c We have solved halfed" << endl;
          config_.maxdecterminate = false;
          config_.use_lso = false;
        }
        LiteralID aLit = TOS_decLit();
        assert(stack_.get_decision_level() > 0);
        if (stack_.get_decision_level() == 1) {
          cout << "c We have solved halfed" << endl;
          config_.use_lso = false;
        }

        stack_.top().changeBranch();
        reactivateTOS();
        setLiteralIfFree(aLit.neg(), NOT_A_CLAUSE);
        return RESOLVED;
      }
      // OTHERWISE:  backtrack further
      comp_manager_.cacheModelCountOf(stack_.top().super_component(),
                                      stack_.top().getTotalModelCount());

      // Chang's modification: diable csvsads
      if (config_.use_csvsads) {
        statistics_.numcachedec_++;
        if (statistics_.numcachedec_ % 128 == 0) {
          comp_manager_.increasecachescores();
        }
        comp_manager_.decreasecachescore(comp_manager_.superComponentOf(stack_.top()));
      }

      if (stack_.get_decision_level() <= 0) {
        break;
      }
      reactivateTOS();

      assert(stack_.size() >= 2);
      (stack_.end() - 2)->includeSolution(stack_.top().getTotalModelCount());
      stack_.pop_back();
      // step to the next component not yet processed
      stack_.top().nextUnprocessedComponent();

      assert(
          stack_.top().remaining_components_ofs() < comp_manager_.component_stack_size() + 1);
    } while (stack_.get_decision_level() >= 0);
  }
  return EXIT;
}

retStateT Solver::resolveConflict() {
  recordLastUIPCauses();
  if (statistics_.num_clauses_learned_ - last_ccl_deletion_time_ >
        statistics_.clause_deletion_interval()) {
    deleteConflictClauses();
    last_ccl_deletion_time_ = statistics_.num_clauses_learned_;
  }
  if (statistics_.num_clauses_learned_ - last_ccl_cleanup_time_ > 100000) {
    compactConflictLiteralPool();
    last_ccl_cleanup_time_ = statistics_.num_clauses_learned_;
  }
  statistics_.num_conflicts_++;
  assert(
      stack_.top().remaining_components_ofs() <= comp_manager_.component_stack_size());
  assert(uip_clauses_.size() == 1);

  // mark the current branch UNSAT
  stack_.top().mark_branch_unsat();
  //BEGIN Backtracking
  // maybe the other branch had some solutions
  if (stack_.top().isSecondBranch()) {
    if (stack_.get_decision_level() == 1) {
      cout << "c We have solved halfed" << endl;
      config_.use_lso = false;
    }
    return BACKTRACK;
  }

  Antecedent ant(NOT_A_CLAUSE);
  // this has to be checked since using implicit BCP and checking literals there not exhaustively
  // we cannot guarantee that uip_clauses_.back().front() == TOS_decLit().neg()
  // this is because we might have checked a literal during implict BCP which has been a failed literal due only to assignments made at lower decision levels
  if (uip_clauses_.back().front() == TOS_decLit().neg()) {
    assert(TOS_decLit().neg() == uip_clauses_.back()[0]);
    var(TOS_decLit().neg()).ante = addUIPConflictClause(
        uip_clauses_.back());
    ant = var(TOS_decLit()).ante;
  }

  assert(stack_.get_decision_level() > 0);
  assert(stack_.top().branch_found_unsat());

  // we do not have to remove pollutions here,
  // since conflicts only arise directly before
  // remaining components are stored
  // hence
  assert(
      stack_.top().remaining_components_ofs() == comp_manager_.component_stack_size());

  stack_.top().changeBranch();
  LiteralID lit = TOS_decLit();
  reactivateTOS();
  setLiteralIfFree(lit.neg(), ant);
  //END Backtracking
  return RESOLVED;
}


// Chang's modification: resolve conflict for the second branch
retStateT Solver::resolveConflictOnBranch2() {
  recordLastUIPCauses();
  if (statistics_.num_clauses_learned_ - last_ccl_deletion_time_ >
        statistics_.clause_deletion_interval()) {
    deleteConflictClauses();
    last_ccl_deletion_time_ = statistics_.num_clauses_learned_;
  }
  if (statistics_.num_clauses_learned_ - last_ccl_cleanup_time_ > 100000) {
    compactConflictLiteralPool();
    last_ccl_cleanup_time_ = statistics_.num_clauses_learned_;
  }
  statistics_.num_conflicts_++;
  assert(
      stack_.top().remaining_components_ofs() <= comp_manager_.component_stack_size());
  assert(uip_clauses_.size() == 1);

  // mark the current branch UNSAT
  stack_.top().mark_branch_unsat();
  //BEGIN Backtracking
  assert(stack_.top().isSecondBranch());
  // {
    if (stack_.get_decision_level() == 1) {
      cout << "c We have solved halfed" << endl;
      config_.use_lso = false;
    }
    return BACKTRACK;
  // }
}


retStateT Solver::resolveConflictForSim() {
  if (simRes == 0)
    stack_.top().mark_branch_unsat();
  stack_.top().includeSolution(simRes);
  //BEGIN Backtracking
  // maybe the other branch had some solutions
  if (stack_.top().isSecondBranch())
    return BACKTRACK;

  Antecedent ant(NOT_A_CLAUSE);
  assert(stack_.get_decision_level() > 0);
  assert(stack_.top().remaining_components_ofs() == comp_manager_.component_stack_size());
  stack_.top().changeBranch();
  LiteralID lit = TOS_decLit();
  reactivateTOS();
  setLiteralIfFree(lit.neg(), ant);
  //END Backtracking
  return RESOLVED;
}


retStateT Solver::resolveConflictForSimOnBranch2() {
  if (simRes == 0)
    stack_.top().mark_branch_unsat();
  stack_.top().includeSolution(simRes);
  //BEGIN Backtracking
  // maybe the other branch had some solutions
  assert(stack_.top().isSecondBranch());
  return BACKTRACK;
}


// Chang's modification: update clause offset
void Solver::updateClauseOffset() {
  clsId2ClsOfs.reserve(statistics_.num_long_clauses_ + 1);
  clsId2ClsOfs.emplace_back(0);
  var2ClsOfs.resize(num_variables() + 1, 0);
  clsOfs2Var.resize(literal_pool_.size(), 0);
  for (auto it_lit = literal_pool_.begin(); it_lit != literal_pool_.end(); it_lit++) {
    if (*it_lit == SENTINEL_LIT) {
      if (it_lit + 1 == literal_pool_.end())
        break;
      it_lit += ClauseHeader::overheadInLits();
      auto offset = 1 + it_lit - literal_pool_.begin();
      clsId2ClsOfs.push_back(offset);

      // variable order in each clause: a, b, z, SENTINEL (for z = a & b)
      // assume that clauses are given in the topological order
      // in each clause, the corresponding gate has the maximum variable id
      assert(*(1 + it_lit) != SENTINEL_LIT && *(2 + it_lit) != SENTINEL_LIT && *(3 + it_lit) != SENTINEL_LIT && *(4 + it_lit) == SENTINEL_LIT);
      VariableIndex var = max(max((1 + it_lit)->var(), (2 + it_lit)->var()), (3 + it_lit)->var());
      // cout << "clause = " << ((1 + it_lit)->toInt()) << "," << ((2 + it_lit)->toInt()) << "," << ((3 + it_lit)->toInt()) << ", offset = " << offset << ", var = " << var << endl;
      var2ClsOfs[var] = offset;
      clsOfs2Var[offset] = var;
    }
  }
}


// void Solver::simulateOnce(Component& topComp, BitVec& ALL_ZERO, BitVec& ALL_ONE) {
//   unordered_set<VariableIndex> checkedGates; // Chang's comment: use reserve() for acceleration
//   for (auto itClsId = topComp.clsBegin(); *itClsId != clsSENTINEL; ++itClsId) {
//     auto clsOfs = clsId2ClsOfs[*itClsId];
//     auto var = clsOfs2Var[clsOfs];
//     if (!isActive(LiteralID(var, 1))) {
//       auto fi0 = fanin0[var], fi1 = fanin1[var];
//       if (isActive(LiteralID(fi0, 1)) && isActive(LiteralID(fi1, 1)))
//         checkedGates.emplace(var);
//       continue;
//     }
//     // simulate a gate
//     auto fi0 = fanin0[var], fi1 = fanin1[var];
//     auto lit0 = LiteralID(fi0, 1); 
//     auto lit1 = LiteralID(fi1, 1);
//     auto op0 = isActive(lit0)? (faninC0[var]? ~dynPat[fi0]: dynPat[fi0]): ((isSatisfied(lit0) == faninC0[var])? ALL_ZERO: ALL_ONE);
//     auto op1 = isActive(lit1)? (faninC1[var]? ~dynPat[fi1]: dynPat[fi1]): ((isSatisfied(lit1) == faninC1[var])? ALL_ZERO: ALL_ONE);
//     dynPat[var] = op0 & op1;
//   }

//   auto consistence = ALL_ONE;
//   for (auto gate: checkedGates) {
//     auto fi0 = fanin0[gate], fi1 = fanin1[gate];
//     auto lit0 = LiteralID(fi0, 1);
//     auto lit1 = LiteralID(fi1, 1);
//     assert(!isActive(LiteralID(gate, 1)));
//     const auto & valInv = isSatisfied(LiteralID(gate, 1))? ALL_ZERO: ALL_ONE;
//     auto op0 = isActive(lit0)? (faninC0[gate]? ~dynPat[fi0]: dynPat[fi0]): ((isSatisfied(lit0) == faninC0[gate])? ALL_ZERO: ALL_ONE);
//     auto op1 = isActive(lit1)? (faninC1[gate]? ~dynPat[fi1]: dynPat[fi1]): ((isSatisfied(lit1) == faninC1[gate])? ALL_ZERO: ALL_ONE);
//     consistence &= (valInv ^ (op0 & op1));
//   }

//   simRes += consistence.count();
// }


void Solver::simulatePro(vector<int> & undecPIs, vector<LiteralID>& lit, Component& topComp) {
  int nUndecPI = undecPIs.size();
  // auto & topComp = comp_manager_.currentRemainingComponentOf(stack_.top());
  int nBit = 1 << nUndecPI;
  BitVec ALL_ZERO(nBit, 0);
  BitVec ALL_ONE(nBit, 0); ALL_ONE.set();
  simRes = 0;
  // statistics_.num_simulation_++;
  // statistics_.sum_pi += nUndecPI;
  // statistics_.sum_clause += topComp.numLongClauses();

  // initialize PI patterns
  for (int piId = 0; piId < nUndecPI; ++piId)
    dynPat[undecPIs[piId]] = DYN_ENUM_PATTERN[nUndecPI][piId];

  // simulate
  auto consistence = ALL_ONE;
  for (auto itClsId = topComp.clsBegin(); *itClsId != clsSENTINEL; ++itClsId) {
    auto clsOfs = clsId2ClsOfs[*itClsId];
    auto var = clsOfs2Var[clsOfs];
    if (!isActive(lit[var])) {
      // check consistency
      auto fi0 = fanin0[var], fi1 = fanin1[var];
      // assert(isActive(LiteralID(fi0, 1)) && isActive(LiteralID(fi1, 1)));
      const auto & valNeg = isSatisfied(lit[var])? ALL_ZERO: ALL_ONE;
      const auto & op0 = faninC0[var]? ~dynPat[fi0]: dynPat[fi0];
      const auto & op1 = faninC1[var]? ~dynPat[fi1]: dynPat[fi1];
      consistence &= (valNeg ^ (op0 & op1));
    }
    else {
      // simulate a gate
      auto fi0 = fanin0[var], fi1 = fanin1[var];
      const auto & op0 = isActive(lit[fi0])? (faninC0[var]? ~dynPat[fi0]: dynPat[fi0]): ((isSatisfied(lit[fi0]) == faninC0[var])? ALL_ZERO: ALL_ONE);
      const auto & op1 = isActive(lit[fi1])? (faninC1[var]? ~dynPat[fi1]: dynPat[fi1]): ((isSatisfied(lit[fi1]) == faninC1[var])? ALL_ZERO: ALL_ONE);
      dynPat[var] = op0 & op1;
    }
  }

  simRes += consistence.count();
}


bool Solver::bcp() {
  // the asserted literal has been set, so we start
  // bcp on that literal
  unsigned start_ofs = literal_stack_.size() - 1;

  // Chang's comment: may be accelerated
  //BEGIN process unit clauses
  for (auto lit : unit_clauses_) {
    setLiteralIfFree(lit);
  }
  //END process unit clauses

  bool bSucceeded = BCP(start_ofs);

  // Chang's modification: do not use implicit BCP, it works on adders and multipliers, but I don't know why
  // if (config_.perform_failed_lit_test && bSucceeded) {
  //   bSucceeded = implicitBCP();
  // }
  
  // printUndecidedClauses(comp_manager_.superComponentOf(stack_.top()));
  return bSucceeded;
}


bool Solver::BCP(unsigned start_at_stack_ofs) {
  for (unsigned int i = start_at_stack_ofs; i < literal_stack_.size(); i++) {
    LiteralID unLit = literal_stack_[i].neg();
    //BEGIN Propagate Bin Clauses
    for (auto bt = literal(unLit).binary_links_.begin();
         *bt != SENTINEL_LIT; bt++) {
      if (isResolved(*bt)) {
        setConflictState(unLit, *bt);
        return false;
      }
      setLiteralIfFree(*bt, Antecedent(unLit));
    }
    //END Propagate Bin Clauses
    for (auto itcl = literal(unLit).watch_list_.rbegin();
         *itcl != SENTINEL_CL; itcl++) {
      bool isLitA = (*beginOf(*itcl) == unLit);
      auto p_watchLit = beginOf(*itcl) + 1 - isLitA;
      auto p_otherLit = beginOf(*itcl) + isLitA;

      if (isSatisfied(*p_otherLit)) {
        continue;
      }
      auto itL = beginOf(*itcl) + 2;
      while (isResolved(*itL)) {
        itL++;
      }
      // either we found a free or satisfied lit
      if (*itL != SENTINEL_LIT) {
        literal(*itL).addWatchLinkTo(*itcl);
        swap(*itL, *p_watchLit);
        *itcl = literal(unLit).watch_list_.back();
        literal(unLit).watch_list_.pop_back();
      } else {
        // or p_unLit stays resolved
        // and we have hence no free literal left
        // for p_otherLit remain poss: Active or Resolved
        if (setLiteralIfFree(*p_otherLit, Antecedent(*itcl))) { // implication
          if (isLitA) {
            swap(*p_otherLit, *p_watchLit);
          }
        } else {
          setConflictState(*itcl);
          return false;
        }
      }
    }
  }
  return true;
}

// Chang's modification: disable
// this is IBCP 30.08
bool Solver::implicitBCP() {
  static vector<LiteralID> test_lits(num_variables());
  static LiteralIndexedVector<unsigned char> viewed_lits(num_variables() + 1,
                                                         0);

  unsigned stack_ofs = stack_.top().literal_stack_ofs();
  unsigned num_curr_lits = 0;
  while (stack_ofs < literal_stack_.size()) {
    test_lits.clear();
    for (auto it = literal_stack_.begin() + stack_ofs;
         it != literal_stack_.end(); it++) {
      for (auto cl_ofs : occurrence_lists_[it->neg()]) {
        if (!isSatisfied(cl_ofs)) {
          for (auto lt = beginOf(cl_ofs); *lt != SENTINEL_LIT; lt++) {
            if (isActive(*lt) && !viewed_lits[lt->neg()]) {
              test_lits.push_back(lt->neg());
              viewed_lits[lt->neg()] = true;
            }
          }
        }
      }
    }
    num_curr_lits = literal_stack_.size() - stack_ofs;
    stack_ofs = literal_stack_.size();
    for (auto jt = test_lits.begin(); jt != test_lits.end(); jt++) {
      viewed_lits[*jt] = false;
    }

    vector<float> scores;
    scores.clear();
    for (auto jt = test_lits.begin(); jt != test_lits.end(); jt++) {
      scores.push_back(literal(*jt).activity_score_);
    }
    sort(scores.begin(), scores.end());
    num_curr_lits = 10 + num_curr_lits / 20;
    float threshold = 0.0;
    if (scores.size() > num_curr_lits) {
      threshold = scores[scores.size() - num_curr_lits];
    }

    statistics_.num_failed_literal_tests_ += test_lits.size();

    for (auto lit : test_lits) {
      if (isActive(lit) && threshold <= literal(lit).activity_score_) {
        unsigned sz = literal_stack_.size();
        // we increase the decLev artificially
        // s.t. after the tentative BCP call, we can learn a conflict clause
        // relative to the assignment of *jt
        stack_.startFailedLitTest();
        setLiteralIfFree(lit);

        assert(!hasAntecedent(lit));

        bool bSucceeded = BCP(sz);
        if (!bSucceeded) {
          recordAllUIPCauses();
        }

        stack_.stopFailedLitTest();

        while (literal_stack_.size() > sz) {
          // unSet(literal_stack_.back());
          // Chang's modification: updated undecided vars
          auto lit = literal_stack_.back();
          unSet(lit);
          literal_stack_.pop_back();
        }

        if (!bSucceeded) {
          statistics_.num_failed_literals_detected_++;
          sz = literal_stack_.size();
          for (auto it = uip_clauses_.rbegin();
               it != uip_clauses_.rend(); it++) {
            // DEBUG
            if (it->size() == 0) {
              cout << "c EMPTY CLAUSE FOUND" << endl;
            }
            // END DEBUG
            setLiteralIfFree(it->front(),
                             addUIPConflictClause(*it));
          }
          if (!BCP(sz)) {
            return false;
          }
        }
      }
    }
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// BEGIN module conflictAnalyzer
///////////////////////////////////////////////////////////////////////////////////////////////

void Solver::minimizeAndStoreUIPClause(
  LiteralID uipLit,
  vector<LiteralID> &tmp_clause, bool seen[]) {

  static deque<LiteralID> clause;
  clause.clear();
  assertion_level_ = 0;
  for (auto lit : tmp_clause) {
    if (existsUnitClauseOf(lit.var())) {
      continue;
    }
    bool resolve_out = false;
    if (hasAntecedent(lit)) {
      resolve_out = true;
      if (getAntecedent(lit).isAClause()) {
        for (auto it = beginOf(getAntecedent(lit).asCl()) + 1;
             *it != SENTINEL_CL; it++) {
          if (!seen[it->var()]) {
            resolve_out = false;
            break;
          }
        }
      } else if (!seen[getAntecedent(lit).asLit().var()]) {
        resolve_out = false;
      }
    }

    if (!resolve_out) {
      // uipLit should be the sole literal of this Decision Level
      if (var(lit).decision_level >= assertion_level_) {
        assertion_level_ = var(lit).decision_level;
        clause.push_front(lit);
      } else {
        clause.push_back(lit);
      }
    }
  }

  if (uipLit.var()) {
    assert(var(uipLit).decision_level == stack_.get_decision_level());
  }

  //assert(uipLit.var() != 0);
  if (uipLit.var() != 0) {
    clause.push_front(uipLit);
  }
  uip_clauses_.push_back(vector<LiteralID>(clause.begin(), clause.end()));
}

void Solver::recordLastUIPCauses() {
  // note:
  // variables of lower dl: if seen we dont work with them anymore
  // variables of this dl: if seen we incorporate their
  // antecedent and set to unseen
  bool seen[num_variables() + 1];
  memset(seen, false, sizeof(bool) * (num_variables() + 1));

  static vector<LiteralID> tmp_clause;
  tmp_clause.clear();

  assertion_level_ = 0;
  uip_clauses_.clear();

  unsigned lit_stack_ofs = literal_stack_.size();
  int DL = stack_.get_decision_level();
  unsigned lits_at_current_dl = 0;

  for (auto l : violated_clause) {
    if (var(l).decision_level == 0 || existsUnitClauseOf(l.var())) {
      continue;
    }
    if (var(l).decision_level < DL) {
      tmp_clause.push_back(l);
    } else {
      lits_at_current_dl++;
    }
    literal(l).increaseActivity();
    seen[l.var()] = true;
  }

  LiteralID curr_lit;
  while (lits_at_current_dl) {
    assert(lit_stack_ofs != 0);
    curr_lit = literal_stack_[--lit_stack_ofs];

    if (!seen[curr_lit.var()]) {
      continue;
    }

    seen[curr_lit.var()] = false;

    if (lits_at_current_dl-- == 1) {
      // perform UIP stuff
      if (!hasAntecedent(curr_lit)) {
        // this should be the decision literal when in first branch
        // or it is a literal decided to explore in failed literal testing
        break;
      }
    }

    assert(hasAntecedent(curr_lit));
    if (getAntecedent(curr_lit).isAClause()) {
      updateActivities(getAntecedent(curr_lit).asCl());
      assert(curr_lit == *beginOf(getAntecedent(curr_lit).asCl()));

      for (auto it = beginOf(getAntecedent(curr_lit).asCl()) + 1;
           *it != SENTINEL_CL; it++) {
        if (seen[it->var()] || (var(*it).decision_level == 0) || existsUnitClauseOf(it->var())) {
          continue;
        }
        if (var(*it).decision_level < DL) {
          tmp_clause.push_back(*it);
        } else {
          lits_at_current_dl++;
        }
        seen[it->var()] = true;
      }
    } else {
      LiteralID alit = getAntecedent(curr_lit).asLit();
      literal(alit).increaseActivity();
      literal(curr_lit).increaseActivity();
      if (!seen[alit.var()] && !(var(alit).decision_level == 0) &&
            !existsUnitClauseOf(alit.var())) {
        if (var(alit).decision_level < DL) {
          tmp_clause.push_back(alit);
        } else {
          lits_at_current_dl++;
        }
        seen[alit.var()] = true;
      }
    }
    curr_lit = NOT_A_LIT;
  }
  minimizeAndStoreUIPClause(curr_lit.neg(), tmp_clause, seen);

  //	if (var(curr_lit).decision_level > assertion_level_)
  //		assertion_level_ = var(curr_lit).decision_level;
}

void Solver::recordAllUIPCauses() {
  // note:
  // variables of lower dl: if seen we dont work with them anymore
  // variables of this dl: if seen we incorporate their
  // antecedent and set to unseen
  bool seen[num_variables() + 1];
  memset(seen, false, sizeof(bool) * (num_variables() + 1));

  static vector<LiteralID> tmp_clause;
  tmp_clause.clear();

  assertion_level_ = 0;
  uip_clauses_.clear();

  unsigned lit_stack_ofs = literal_stack_.size();
  int DL = stack_.get_decision_level();
  unsigned lits_at_current_dl = 0;

  for (auto l : violated_clause) {
    if (var(l).decision_level == 0 || existsUnitClauseOf(l.var())) {
      continue;
    }
    if (var(l).decision_level < DL) {
      tmp_clause.push_back(l);
    } else {
      lits_at_current_dl++;
    }
    literal(l).increaseActivity();
    seen[l.var()] = true;
  }
  unsigned n = 0;
  LiteralID curr_lit;
  while (lits_at_current_dl) {
    assert(lit_stack_ofs != 0);
    curr_lit = literal_stack_[--lit_stack_ofs];

    if (!seen[curr_lit.var()]) {
      continue;
    }

    seen[curr_lit.var()] = false;

    if (lits_at_current_dl-- == 1) {
      n++;
      if (!hasAntecedent(curr_lit)) {
        // this should be the decision literal when in first branch
        // or it is a literal decided to explore in failed literal testing
        //assert(stack_.TOS_decLit() == curr_lit);
        break;
      }
      // perform UIP stuff
      minimizeAndStoreUIPClause(curr_lit.neg(), tmp_clause, seen);
    }

    assert(hasAntecedent(curr_lit));

    if (getAntecedent(curr_lit).isAClause()) {
      updateActivities(getAntecedent(curr_lit).asCl());
      assert(curr_lit == *beginOf(getAntecedent(curr_lit).asCl()));

      for (auto it = beginOf(getAntecedent(curr_lit).asCl()) + 1;
           *it != SENTINEL_CL; it++) {
        if (seen[it->var()] || (var(*it).decision_level == 0) ||
              existsUnitClauseOf(it->var())) {
          continue;
        }
        if (var(*it).decision_level < DL) {
          tmp_clause.push_back(*it);
        } else {
          lits_at_current_dl++;
        }
        seen[it->var()] = true;
      }
    } else {
      LiteralID alit = getAntecedent(curr_lit).asLit();
      literal(alit).increaseActivity();
      literal(curr_lit).increaseActivity();
      if (!seen[alit.var()] && !(var(alit).decision_level == 0) &&
            !existsUnitClauseOf(alit.var())) {
        if (var(alit).decision_level < DL) {
          tmp_clause.push_back(alit);
        } else {
          lits_at_current_dl++;
        }
        seen[alit.var()] = true;
      }
    }
  }
  if (!hasAntecedent(curr_lit)) {
    minimizeAndStoreUIPClause(curr_lit.neg(), tmp_clause, seen);
  }
  //	if (var(curr_lit).decision_level > assertion_level_)
  //		assertion_level_ = var(curr_lit).decision_level;
}

void Solver::printOnlineStats() {
  if (config_.quiet) {
    return;
  }

  cout << endl;
  cout << "time elapsed: " << stopwatch_.getElapsedSeconds() << "s" << endl;
  if (config_.verbose) {
    cout << "conflict clauses (all / bin / unit) \t";
    cout << num_conflict_clauses();
    cout << "/" << statistics_.num_binary_conflict_clauses_ << "/"
         << unit_clauses_.size() << endl;
    cout << "failed literals found by implicit BCP \t "
         << statistics_.num_failed_literals_detected_ << endl;

    cout << "implicit BCP miss rate \t "
         << statistics_.implicitBCP_miss_rate() * 100 << "%";
    cout << endl;

    comp_manager_.gatherStatistics();

    cout << "cache size " << statistics_.cache_MB_memory_usage() << "MB" << endl;
    cout << "components (stored / hits) \t\t"
         << statistics_.cached_component_count() << "/"
         << statistics_.cache_hits() << endl;
    cout << "avg. variable count (stored / hits) \t"
         << statistics_.getAvgComponentSize() << "/"
         << statistics_.getAvgCacheHitSize();
    cout << endl;
    cout << "cache miss rate " << statistics_.cache_miss_rate() * 100 << "%"
         << endl;
  }
}
