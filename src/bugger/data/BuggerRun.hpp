/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*                            MIP-DD                                         */
/*                                                                           */
/* Copyright (C) 2024             Zuse Institute Berlin                      */
/*                                                                           */
/*  Licensed under the Apache License, Version 2.0 (the "License");          */
/*  you may not use this file except in compliance with the License.         */
/*  You may obtain a copy of the License at                                  */
/*                                                                           */
/*      http://www.apache.org/licenses/LICENSE-2.0                           */
/*                                                                           */
/*  Unless required by applicable law or agreed to in writing, software      */
/*  distributed under the License is distributed on an "AS IS" BASIS,        */
/*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. */
/*  See the License for the specific language governing permissions and      */
/*  limitations under the License.                                           */
/*                                                                           */
/*  You should have received a copy of the Apache-2.0 license                */
/*  along with MIP-DD; see the file LICENSE. If not visit scipopt.org.       */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef __BUGGER_DATA_BUGGERRUN_HPP__
#define __BUGGER_DATA_BUGGERRUN_HPP__

#include "bugger/io/MpsParser.hpp"
#include "bugger/io/MpsWriter.hpp"
#include "bugger/io/SolParser.hpp"
#include "bugger/misc/OptionsParser.hpp"
#include "bugger/modules/SettingModul.hpp"


namespace bugger
{
   template <typename REAL>
   class BuggerRun
   {
   private:

      const Message& msg;
      const Num<REAL>& num;
      BuggerParameters& parameters;
      const std::shared_ptr<SolverFactory<REAL>>& factory;
      const Vec<std::unique_ptr<BuggerModul<REAL>>>& modules;
      Vec<ModulStatus> results;

   public:

      explicit BuggerRun(const Message& _msg, const Num<REAL>& _num, BuggerParameters& _parameters, const std::shared_ptr<SolverFactory<REAL>>& _factory, const Vec<std::unique_ptr<BuggerModul<REAL>>>& _modules)
            : msg(_msg), num(_num), parameters(_parameters), factory(_factory), modules(_modules), results(_modules.size()) { }

      bool
      is_time_exceeded(const Timer& timer) const
      {
         return timer.getTime() >= parameters.tlim;
      }

      void
      apply(const OptionsInfo& optionsInfo, SettingModul<REAL>* const setting)
      {
         msg.info("\nMIP Solver:\n");
         factory->create_solver(msg)->print_header();
         msg.info("\n");
         if( setting->isEnabled() )
         {
            auto target_settings = factory->create_solver(msg)->parseSettings(optionsInfo.target_settings_file);
            if( !target_settings )
            {
               msg.error("Error loading targets {}\n", optionsInfo.target_settings_file);
               return;
            }
            setting->target_settings = target_settings.get();
         }
         SolutionStatus status { };
         if( boost::iequals(optionsInfo.solution_file, "infeasible") )
            status = SolutionStatus::kInfeasible;
         else if( boost::iequals(optionsInfo.solution_file, "unbounded") )
            status = SolutionStatus::kUnbounded;
         else if( !optionsInfo.solution_file.empty() && !boost::iequals(optionsInfo.solution_file, "unknown") )
            status = SolutionStatus::kFeasible;
         auto instance = factory->create_solver(msg)->readInstance(optionsInfo.settings_file, optionsInfo.problem_file, status == SolutionStatus::kFeasible ? optionsInfo.solution_file : "");
         if( !std::get<0>(instance) )
         {
            msg.error("Error loading settings {}\n", optionsInfo.settings_file);
            return;
         }
         auto settings = std::get<0>(instance).get();
         if( !std::get<1>(instance) )
         {
            msg.info("Problem parser of the solver failed. Trying general parser...");
            std::get<1>(instance) = MpsParser<REAL>::loadProblem(optionsInfo.problem_file);
            if( !std::get<1>(instance) )
            {
               msg.error("Error loading problem {}\n", optionsInfo.problem_file);
               return;
            }
         }
         auto problem = std::get<1>(instance).get();
         if( !std::get<2>(instance) )
         {
            msg.info("Solution parser of the solver failed. Trying general parser...");
            std::get<2>(instance) = SolParser<REAL>::read(optionsInfo.solution_file, problem.getVariableNames( ));
            if( !std::get<2>(instance) )
            {
               msg.error("Error loading solution {}\n", optionsInfo.solution_file);
               return;
            }
         }
         auto solution = std::get<2>(instance).get();
         solution.status = status;
         check_feasibility_of_solution(problem, solution);
         long long last_effort = -1;
         std::pair<char, SolverStatus> last_result = { SolverRetcode::OKAY, SolverStatus::kUnknown };
         int last_round = -1;
         int last_module = -1;
         auto solver = factory->create_solver(msg);
         solver->doSetUp(settings, problem, solution);
         if( parameters.mode == 1 )
         {
            if( parameters.expenditure < 0 )
               parameters.expenditure = 0;
         }
         else
         {
            last_result = solver->solve(Vec<int>{ });
            last_effort = solver->getSolvingEffort( );
            msg.info("Original solve returned code {} with status {} and effort {}.\n", (int)last_result.first, last_result.second, last_effort);
            if( parameters.mode == 0 )
               return;
            if( parameters.expenditure < 0 && ( parameters.nbatches <= 0 || last_effort <= 0 || (parameters.expenditure = parameters.nbatches * last_effort) / last_effort != parameters.nbatches ) )
            {
               msg.info("Batch adaption disabled.\n");
               parameters.expenditure = 0;
            }
            msg.info("\n");
         }
         bool writesolution = false;
         for( const auto& module: modules )
         {
            if( module->getName() == "fixing" )
            {
               writesolution = module->isEnabled();
               break;
            }
         }
         int ending = optionsInfo.problem_file.rfind('.');
         if( optionsInfo.problem_file.substr(ending + 1) == "gz" ||
             optionsInfo.problem_file.substr(ending + 1) == "bz2" )
            ending = optionsInfo.problem_file.rfind('.', ending-1);
         std::string filename = optionsInfo.problem_file.substr(0, ending) + "_";

         double time = 0.0;
         {
            Timer timer(time);

            for( int round = parameters.initround, stage = parameters.initstage, success = parameters.initstage; round < parameters.maxrounds && stage < parameters.maxstages; ++round )
            {
               //TODO: Clean matrix in each round
               //TODO: Simplify solver handling
               //TODO: Free solver afterwards
               solver = factory->create_solver(msg);
               solver->doSetUp(settings, problem, solution);
               if( !solver->writeInstance(filename + std::to_string(round), setting->isEnabled(), writesolution) )
                  MpsWriter<REAL>::writeProb(filename + std::to_string(round) + ".mps", problem);

               if( is_time_exceeded(timer) )
                  break;

               // adapt batch number
               if( parameters.expenditure > 0 && last_effort >= 0 )
                  parameters.nbatches = last_effort >= 1 ? ( parameters.expenditure - 1) / last_effort + 1 : 0;

               msg.info("Round {} Stage {} Batch {}\n", round+1, stage+1, parameters.nbatches);

               for( int module = 0; module <= stage && stage < parameters.maxstages; ++module )
               {
                  results[ module ] = modules[ module ]->run(settings, problem, solution, timer);

                  if( results[ module ] == bugger::ModulStatus::kSuccessful )
                  {
                     long long effort = modules[ module ]->getLastSolvingEffort( );
                     if( effort >= 0 )
                        last_effort = effort;
                     last_result = modules[ module ]->getLastResult( );
                     last_round = round;
                     last_module = module;
                     success = module;
                  }
                  else if( success == module )
                  {
                     module = stage;
                     ++stage;
                     success = stage;
                  }
               }
            }

            assert( is_time_exceeded(timer) || evaluateResults( ) != bugger::ModulStatus::kSuccessful );
         }
         printStats(time, last_result, last_round, last_module, last_effort);
      }

   private:

      REAL
      get_linear_activity(const SparseVectorView<REAL>& data, const Solution<REAL>& solution) const
      {
         StableSum<REAL> sum;
         for( int i = 0; i < data.getLength( ); ++i )
            sum.add(data.getValues( )[ i ] * solution.primal[ data.getIndices( )[ i ] ]);
         return sum.get( );
      }

      void
      check_feasibility_of_solution(const Problem<REAL>& problem, const Solution<REAL>& solution)
      {
         if( solution.status != SolutionStatus::kFeasible )
            return;

         const auto& lb = problem.getLowerBounds();
         const auto& ub = problem.getUpperBounds();
         REAL viol;
         REAL maxviol { };
         int maxindex = -1;
         bool maxrow = false;
         bool maxupper = false;
         bool maxintegral = false;

         msg.info("\nCheck:\n");
         for( int col = 0; col < problem.getNCols(); col++ )
         {
            if( problem.getColFlags()[col].test( ColFlag::kInactive ) )
               continue;

            if( !problem.getColFlags()[col].test( ColFlag::kLbInf ) && solution.primal[col] < lb[col] )
            {
               msg.detailed( "\tColumn {:<3} violates lower bound ({:<3} < {:<3})\n", problem.getVariableNames()[col], solution.primal[col], lb[col] );
               viol = lb[col] - solution.primal[col];
               if( viol > maxviol )
               {
                  maxviol = viol;
                  maxindex = col;
                  maxrow = false;
                  maxupper = false;
                  maxintegral = false;
               }
            }

            if( !problem.getColFlags()[col].test( ColFlag::kUbInf ) && solution.primal[col] > ub[col] )
            {
               msg.detailed( "\tColumn {:<3} violates upper bound ({:<3} > {:<3})\n", problem.getVariableNames()[col], solution.primal[col], ub[col] );
               viol = solution.primal[col] - ub[col];
               if( viol > maxviol )
               {
                  maxviol = viol;
                  maxindex = col;
                  maxrow = false;
                  maxupper = true;
                  maxintegral = false;
               }
            }

            if( problem.getColFlags()[col].test( ColFlag::kIntegral ) && solution.primal[col] != round(solution.primal[col]) )
            {
               msg.detailed( "\tColumn {:<3} violates integrality property ({:<3} != {:<3})\n", problem.getVariableNames()[col], solution.primal[col], round(solution.primal[col]) );
               viol = abs(solution.primal[col] - round(solution.primal[col]));
               if( viol > maxviol )
               {
                  maxviol = viol;
                  maxindex = col;
                  maxrow = false;
                  maxupper = false;
                  maxintegral = true;
               }
            }
         }

         const auto& lhs = problem.getConstraintMatrix().getLeftHandSides();
         const auto& rhs = problem.getConstraintMatrix().getRightHandSides();

         for( int row = 0; row < problem.getNRows(); row++ )
         {
            if( problem.getRowFlags()[row].test( RowFlag::kRedundant ) )
               continue;

            REAL activity { get_linear_activity(problem.getConstraintMatrix().getRowCoefficients(row), solution) };

            if( !problem.getRowFlags()[row].test( RowFlag::kLhsInf ) && activity < lhs[row] )
            {
               msg.detailed( "\tRow {:<3} violates left side ({:<3} < {:<3})\n", problem.getConstraintNames()[row], activity, lhs[row] );
               viol = lhs[row] - activity;
               if( viol > maxviol )
               {
                  maxviol = viol;
                  maxindex = row;
                  maxrow = true;
                  maxupper = false;
                  maxintegral = false;
               }
            }

            if( !problem.getRowFlags()[row].test( RowFlag::kRhsInf ) && activity > rhs[row] )
            {
               msg.detailed( "\tRow {:<3} violates right side ({:<3} > {:<3})\n", problem.getConstraintNames()[row], activity, rhs[row] );
               viol = activity - rhs[row];
               if( viol > maxviol )
               {
                  maxviol = viol;
                  maxindex = row;
                  maxrow = true;
                  maxupper = true;
                  maxintegral = false;
               }
            }
         }

         msg.info("Solution is {}.\n", num.isEpsGT(maxviol, 0.0) ? "infeasible" : num.isZetaGT(maxviol, 0.0) ? "tolerable" : "feasible");
         if( maxindex >= 0 )
            msg.info("Maximum violation {:<3} of {} {:<3} {}.\n", maxviol, maxrow ? "row" : "column", (maxrow ? problem.getConstraintNames() : problem.getVariableNames())[maxindex], maxintegral ? "integral" : (maxrow ? (maxupper ? "right" : "left") : (maxupper ? "upper" : "lower")));
         else
            msg.info("No violations detected.\n");
         msg.info("\n");
      }

      bugger::ModulStatus
      evaluateResults( )
      {
         int largestValue = static_cast<int>( bugger::ModulStatus::kDidNotRun );

         for( int module = 0; module < parameters.maxstages; ++module )
            largestValue = max(largestValue, static_cast<int>( results[ module ] ));

         return static_cast<bugger::ModulStatus>( largestValue );
      }

      void
      printStats(const double& time, const std::pair<char, SolverStatus>& last_result, int last_round, int last_module, long long last_effort)
      {
         msg.info("\n {:>18} {:>12} {:>12} {:>18} {:>12} {:>18} \n",
                  "modules", "nb calls", "changes", "success calls(%)", "solves", "execution time(s)");
         int nsolves = 0;
         for( const auto& module: modules )
         {
            module->printStats(msg);
            nsolves += module->getNSolves();
         }
         if( last_round == -1 )
         {
            assert(last_module == -1);
            if( parameters.mode == 1 )
            {
               assert(last_result.first == SolverRetcode::OKAY);
               assert(last_result.second == SolverStatus::kUnknown);
               assert(last_effort == -1);
            }
            msg.info("\nNo reductions found by the bugger!");
         }
         else
         {
            assert(last_module != -1);
            msg.info("\nFinal solve returned code {} with status {} and effort {} in round {} by module {}.", (int)last_result.first, last_result.second, last_effort, last_round + 1, modules[ last_module ]->getName( ));
         }
         msg.info( "\nbugging took {:.3f} seconds with {} solver invocations", time, nsolves );
         if( parameters.mode != 1 )
            msg.info(" (excluding original solve)");
         msg.info("\n");
      }
   };

}

#endif //BUGGER_BUGGERRUN_HPP
