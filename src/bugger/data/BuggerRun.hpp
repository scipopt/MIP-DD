/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*    BUGGER                                                                 */
/*                                                                           */
/* Copyright (C) 2024             Zuse Institute Berlin                      */
/*                                                                           */
/* This program is free software: you can redistribute it and/or modify      */
/* it under the terms of the GNU Lesser General Public License as published  */
/* by the Free Software Foundation, either version 3 of the License, or      */
/* (at your option) any later version.                                       */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU Lesser General Public License for more details.                       */
/*                                                                           */
/* You should have received a copy of the GNU Lesser General Public License  */
/* along with this program.  If not, see <https://www.gnu.org/licenses/>.    */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef __BUGGER_DATA_BUGGERRUN_HPP__
#define __BUGGER_DATA_BUGGERRUN_HPP__

#include "bugger/io/MpsParser.hpp"
#include "bugger/io/MpsWriter.hpp"
#include "bugger/io/SolParser.hpp"
#include "bugger/misc/OptionsParser.hpp"
#include "bugger/misc/MultiPrecision.hpp"
#include "bugger/modules/SettingModul.hpp"


namespace bugger {

   class BuggerRun {

   private:

      const Message& msg;
      const Num<double>& num;
      BuggerParameters& parameters;
      const std::shared_ptr<SolverFactory>& factory;
      const Vec<std::unique_ptr<BuggerModul>>& modules;
      Vec<ModulStatus> results;

   public:

      explicit BuggerRun(const Message& _msg, const Num<double>& _num, BuggerParameters& _parameters, const std::shared_ptr<SolverFactory>& _factory, const Vec<std::unique_ptr<BuggerModul>>& _modules)
            : msg(_msg), num(_num), parameters(_parameters), factory(_factory), modules(_modules), results(_modules.size()) { }

      bool
      is_time_exceeded(const Timer& timer) const {

         return parameters.tlim != std::numeric_limits<double>::max() && timer.getTime() >= parameters.tlim;
      }

      void
      apply(const OptionsInfo& optionsInfo, SettingModul* const setting) {

         msg.info("\nMIP Solver:\n");
         factory->create_solver(msg)->print_header();
         msg.info("\n");
         if( setting->isEnabled() )
         {
            auto target_settings = factory->create_solver(msg)->parseSettings(optionsInfo.target_settings_file);
            if( !target_settings )
            {
               msg.error("error loading targets {}\n", optionsInfo.target_settings_file);
               return;
            }
            setting->target_settings = target_settings.get();
         }
         auto instance = factory->create_solver(msg)->readInstance(optionsInfo.settings_file, optionsInfo.problem_file);
         if( !instance.first )
         {
            msg.error("error loading settings {}\n", optionsInfo.settings_file);
            return;
         }
         if( !instance.second )
         {
            msg.info("Parser of the solver failed. Using internal parser...");
            instance.second = MpsParser<double>::loadProblem(optionsInfo.problem_file);
            if( !instance.second )
            {
               msg.error("error loading problem {}\n", optionsInfo.problem_file);
               return;
            }
         }
         auto settings = instance.first.get();
         auto problem = instance.second.get();
         Solution<double> solution;
         if( !optionsInfo.solution_file.empty( ) )
         {
            if( boost::iequals(optionsInfo.solution_file, "infeasible") )
               solution.status = SolutionStatus::kInfeasible;
            else if( boost::iequals(optionsInfo.solution_file, "unbounded") )
               solution.status = SolutionStatus::kUnbounded;
            else if( !boost::iequals(optionsInfo.solution_file, "unknown") )
            {
               bool success = SolParser<double>::read(optionsInfo.solution_file, problem.getVariableNames( ), solution);
               if( !success )
               {
                  msg.error("error loading solution {}\n", optionsInfo.solution_file);
                  return;
               }
            }
         }

         check_feasibility_of_solution(problem, solution);
         long long last_solving_effort = -1;
         std::pair<char, SolverStatus> last_result = { SolverInterface::OKAY, SolverStatus::kUnknown };
         int last_round = -1;
         int last_module = -1;
         if( parameters.mode == 1 )
         {
            if( parameters.expenditure < 0 )
               parameters.expenditure = 0;
         }
         else
         {
            auto solver = factory->create_solver(msg);
            solver->doSetUp(settings, problem, solution);
            last_result = solver->solve(Vec<int>{ });
            long long solving_effort = solver->getSolvingEffort( );
            msg.info("Original solve returned code {} with status {} and solving effort {}.\n\n", (int)last_result.first, last_result.second, solving_effort);
            if( parameters.mode == 0 )
               return;
            if( parameters.expenditure > 0 )
               last_solving_effort = solving_effort;
            else if( parameters.expenditure < 0 && ( parameters.nbatches <= 0 || solving_effort <= 0 || ( parameters.expenditure = parameters.nbatches * solving_effort) / solving_effort != parameters.nbatches ) )
            {
               msg.info("Batch adaption disabled.\n");
               parameters.expenditure = 0;
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
               auto solver = factory->create_solver(msg);
               solver->doSetUp(settings, problem, solution);
               if( !solver->writeInstance(filename + std::to_string(round), setting->isEnabled()) )
                  MpsWriter<double>::writeProb(filename + std::to_string(round) + ".mps", problem);

               if( is_time_exceeded(timer) )
                  break;

               // adapt batch number
               if( parameters.expenditure > 0 && last_solving_effort >= 0 )
               {
                  parameters.nbatches = last_solving_effort >= 1 ? ( parameters.expenditure - 1) / last_solving_effort + 1 : 0;
                  last_solving_effort = -1;
               }

               msg.info("Round {} Stage {} Batch {}\n", round+1, stage+1, parameters.nbatches);

               for( int module = 0; module <= stage && stage < parameters.maxstages; ++module )
               {
                  results[ module ] = modules[ module ]->run(settings, problem, solution, timer);

                  if( results[ module ] == bugger::ModulStatus::kSuccessful )
                  {
                     long long solving_effort = modules[ module ]->getLastSolvingEffort( );
                     if( solving_effort >= 0 )
                        last_solving_effort = solving_effort;
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
         printStats(time, last_result, last_round, last_module);
      }

   private:

      void
      check_feasibility_of_solution(const Problem<double>& problem, const Solution<double>& solution) {

         if( solution.status != SolutionStatus::kFeasible )
            return;
         const Vec<double>& ub = problem.getUpperBounds();
         const Vec<double>& lb = problem.getLowerBounds();
         double maxviol = 0.0;
         int maxindex = -1;
         bool maxrow = false;
         bool maxupper = false;
         bool maxintegral = false;
         double viol;

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

            if( problem.getColFlags()[col].test( ColFlag::kIntegral ) && solution.primal[col] != rint(solution.primal[col]) )
            {
               msg.detailed( "\tColumn {:<3} violates integrality property ({:<3} != {:<3})\n", problem.getVariableNames()[col], solution.primal[col], rint(solution.primal[col]) );
               viol = abs(solution.primal[col] - rint(solution.primal[col]));
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

         const Vec<double>& rhs = problem.getConstraintMatrix().getRightHandSides();
         const Vec<double>& lhs = problem.getConstraintMatrix().getLeftHandSides();

         for( int row = 0; row < problem.getNRows(); row++ )
         {
            if( problem.getRowFlags()[row].test( RowFlag::kRedundant ) )
               continue;

            double rowValue = 0;
            auto entries = problem.getConstraintMatrix().getRowCoefficients( row );
            for( int j = 0; j < entries.getLength(); j++ )
            {
               int col = entries.getIndices()[j];
               if( problem.getColFlags()[col].test( ColFlag::kInactive ) )
                  continue;
               double x = entries.getValues()[j];
               double primal = solution.primal[col];
               rowValue += x * primal;
            }

            if( !problem.getRowFlags()[row].test( RowFlag::kLhsInf ) && rowValue < lhs[row] )
            {
               msg.detailed( "\tRow {:<3} violates left side ({:<3} < {:<3})\n", problem.getConstraintNames()[row], rowValue, lhs[row] );
               viol = lhs[row] - rowValue;
               if( viol > maxviol )
               {
                  maxviol = viol;
                  maxindex = row;
                  maxrow = true;
                  maxupper = false;
                  maxintegral = false;
               }
            }

            if( !problem.getRowFlags()[row].test( RowFlag::kRhsInf ) && rowValue > rhs[row] )
            {
               msg.detailed( "\tRow {:<3} violates right side ({:<3} > {:<3})\n", problem.getConstraintNames()[row], rowValue, rhs[row] );
               viol = rowValue - rhs[row];
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

         if( maxindex >= 0 )
            msg.info("Solution is infeasible.\nMaximum violation {:<3} of {} {:<3} {}.", maxviol, maxrow ? "row" : "column", (maxrow ? problem.getConstraintNames() : problem.getVariableNames())[maxindex], maxintegral ? "integral" : (maxrow ? (maxupper ? "right" : "left") : (maxupper ? "upper" : "lower")));
         else
            msg.info("Solution is feasible.\nNo violations detected.");
         msg.info("\n\n");
      }

      bugger::ModulStatus
      evaluateResults( ) {

         int largestValue = static_cast<int>( bugger::ModulStatus::kDidNotRun );

         for( int module = 0; module < parameters.maxstages; ++module )
            largestValue = std::max(largestValue, static_cast<int>( results[ module ] ));

         return static_cast<bugger::ModulStatus>( largestValue );
      }

      void
      printStats(const double& time, const std::pair<char, SolverStatus>& last_result, int last_round, int last_module) {

         msg.info("\n {:>18} {:>12} {:>12} {:>18} {:>12} {:>18} \n", "modules",
                  "nb calls", "changes", "success calls(%)", "solves", "execution time(s)");
         int nsolves = 0;
         for( const auto &module: modules )
         {
            module->printStats(msg);
            nsolves += module->getNSolves();
         }
         if( last_round == -1 )
         {
            assert(parameters.mode != 1 || ( last_result.first == SolverInterface::OKAY && last_result.second == SolverStatus::kUnknown ));
            msg.info("\nNo reductions found by the bugger!");
         }
         else
         {
            assert(last_module != -1);
            msg.info("\nFinal solve returned code {} with status {} in round {} by module {}.", (int)last_result.first, last_result.second, last_round + 1, modules[ last_module ]->getName( ));
         }
         msg.info( "\nbugging took {:.3f} seconds with {} solver invocations", time, nsolves );
         if( parameters.mode != 1 )
            msg.info(" (excluding original solve)");
         msg.info("\n");
      }
   };

}

#endif //BUGGER_BUGGERRUN_HPP
