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

#ifndef BUGGER_BUGGERRUN_HPP
#define BUGGER_BUGGERRUN_HPP

#include <utility>
#include <memory>
#include <fstream>
#include <algorithm>
#include <boost/program_options.hpp>

#include "bugger/data/BuggerParameters.hpp"
#include "bugger/io/MpsParser.hpp"
#include "bugger/io/MpsWriter.hpp"
#include "bugger/io/SolParser.hpp"
#include "bugger/misc/VersionLogger.hpp"
#include "bugger/misc/OptionsParser.hpp"
#include "bugger/misc/Vec.hpp"
#include "bugger/misc/MultiPrecision.hpp"
#include "bugger/interfaces/ScipInterface.hpp"
#include "bugger/modules/VarroundModul.hpp"
#include "bugger/modules/VariableModul.hpp"
#include "bugger/modules/SideModul.hpp"
#include "bugger/modules/SettingModul.hpp"
#include "bugger/modules/ObjectiveModul.hpp"
#include "bugger/modules/FixingModul.hpp"
#include "bugger/modules/ConstraintModul.hpp"
#include "bugger/modules/ConsroundModul.hpp"
#include "bugger/modules/CoefficientModul.hpp"
#include "bugger/modules/BuggerModul.hpp"


namespace bugger {

   class BuggerRun {

   private:

      BuggerParameters parameters { };
      Vec<std::unique_ptr<bugger::BuggerModul>> &modules;
      Vec<bugger::ModulStatus> results;
      Message msg { };
      std::shared_ptr<SolverFactory> factory;

   public:

      explicit BuggerRun( Vec<std::unique_ptr<BuggerModul>> &_modules )
            : modules(_modules), factory(load_solver_factory()) { }

      bool
      is_time_exceeded( const Timer &timer ) const
      {
         return parameters.tlim != std::numeric_limits<double>::max() &&
                timer.getTime() >= parameters.tlim;
      }

      void apply( const Timer &timer, const OptionsInfo &optionsInfo ) {

         overwrite_parameters_by_cmd_options(optionsInfo);
         msg.info("\nMIP Solver:\n");
         factory->create_solver(msg)->print_header();
         msg.info("\n");
         SolverSettings targets { };
         if( !optionsInfo.target_settings_file.empty( ) )
         {
            auto target_settings = factory->create_solver(msg)->parseSettings(optionsInfo.target_settings_file);
            if( !target_settings )
            {
               msg.error("error loading targets {}\n", optionsInfo.target_settings_file);
               return;
            }
            targets = target_settings.get();
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
         if( parameters.mode != 1 )
         {
            printOriginalSolveStatus(settings, problem, solution, factory);
            if( parameters.mode == 0 )
               return;
         }

         Num<double> num{};
         num.setFeasTol( parameters.feastol );
         num.setEpsilon( parameters.epsilon );
         num.setZeta( parameters.zeta );

         using uptr = std::unique_ptr<bugger::BuggerModul>;
         addModul(uptr(new ConstraintModul(msg, num, parameters, factory)));
         addModul(uptr(new VariableModul(msg, num, parameters, factory)));
         addModul(uptr(new CoefficientModul(msg, num, parameters, factory)));
         addModul(uptr(new FixingModul(msg, num, parameters, factory)));
         addModul(uptr(new SettingModul(msg, num, parameters, factory, targets)));
         addModul(uptr(new SideModul(msg, num, parameters, factory)));
         addModul(uptr(new ObjectiveModul(msg, num, parameters, factory)));
         addModul(uptr(new VarroundModul(msg, num, parameters, factory)));
         addModul(uptr(new ConsRoundModul(msg, num, parameters, factory)));

         // disable module setting
         auto &setting = *modules[4];
         if( optionsInfo.target_settings_file.empty( ) )
            setting.setEnabled(false);

         if( parameters.maxrounds < 0 )
            parameters.maxrounds = INT_MAX;
         if( parameters.initround < 0 || parameters.initround >= parameters.maxrounds )
            parameters.initround = parameters.maxrounds-1;
         if( parameters.maxstages < 0 || parameters.maxstages > modules.size( ) )
            parameters.maxstages = modules.size( );
         if( parameters.initstage < 0 || parameters.initstage >= parameters.maxstages )
            parameters.initstage = parameters.maxstages-1;

         int ending = optionsInfo.problem_file.rfind('.');
         if( optionsInfo.problem_file.substr(ending+1) == "gz" || optionsInfo.problem_file.substr(ending+1) == "bz2" )
            ending = optionsInfo.problem_file.rfind('.', ending-1);
         std::string filename = optionsInfo.problem_file.substr(0, ending) + "_";
         results.resize(modules.size( ));

         for( int round = parameters.initround, stage = parameters.initstage, success = parameters.initstage; round < parameters.maxrounds && stage < parameters.maxstages; ++round )
         {
            //TODO: Clean matrix in each round
            //TODO: Simplify solver handling
            //TODO: Free solver afterwards
            auto solver = factory->create_solver(msg);
            solver->doSetUp(settings, problem, solution);
            if( !solver->writeInstance(filename + std::to_string(round), setting.isEnabled()) )
               MpsWriter<double>::writeProb(filename + std::to_string(round) + ".mps", problem);

            if( is_time_exceeded(timer) )
               break;

            msg.info("Round {} Stage {}\n", round+1, stage+1);

            for( int module = 0; module <= stage && stage < parameters.maxstages; ++module )
            {
               results[ module ] = modules[ module ]->run(problem, settings, solution, timer);

               if( results[ module ] == bugger::ModulStatus::kSuccessful )
                  success = module;
               else if( success == module )
               {
                  module = stage;
                  ++stage;
                  success = stage;
               }
            }
         }

         assert( is_time_exceeded(timer) || evaluateResults( ) != bugger::ModulStatus::kSuccessful );
         printStats( timer.getTime() );
      }

      void overwrite_parameters_by_cmd_options(const OptionsInfo &optionsInfo) {
         if( !isnan(optionsInfo.tlim) && optionsInfo.tlim >= 0)
         {
            parameters.tlim = optionsInfo.tlim;
            msg.info("overwriting parameter tlim by cmd to {}\n", parameters.tlim);

         }
         if( optionsInfo.mode <= 1 && optionsInfo.mode >= -1)
         {
            parameters.mode = optionsInfo.mode;
            msg.info("overwriting parameter mode by cmd to {}\n", parameters.mode);
         }
         if( !optionsInfo.debug_filename.empty() )
         {
            parameters.debug_filename = optionsInfo.debug_filename;
            msg.info("overwriting parameter debug-filename by cmd to {}\n", parameters.debug_filename);
         }
      }

      void
      addModul( std::unique_ptr<BuggerModul> module ) {
         modules.emplace_back(std::move(module));
      }

      ParameterSet getParameters( ) {
         ParameterSet paramSet;
         parameters.addParameters(paramSet);
         for( const auto &module: modules )
            module->addParameters(paramSet);
         factory->addParameters(paramSet);
         return paramSet;
      }

   private:

      void check_feasibility_of_solution( const Problem<double> &problem , const Solution<double> &solution ) {
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
               msg.detailed( "\tColumn {:<3} violates lower bound ({:<3} < {:<3}).\n", problem.getVariableNames()[col], solution.primal[col], lb[col] );
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
               msg.detailed( "\tColumn {:<3} violates upper bound ({:<3} > {:<3}).\n", problem.getVariableNames()[col], solution.primal[col], ub[col] );
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
               msg.detailed( "\tColumn {:<3} violates integrality property ({:<3} != {:<3}).\n", problem.getVariableNames()[col], solution.primal[col], rint(solution.primal[col]) );
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
               msg.detailed( "\tRow {:<3} violates left side ({:<3} < {:<3}).\n", problem.getConstraintNames()[row], rowValue, lhs[row] );
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
               msg.detailed( "\tRow {:<3} violates right side ({:<3} > {:<3}).\n", problem.getConstraintNames()[row], rowValue, rhs[row] );
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

      void printOriginalSolveStatus( const SolverSettings &settings, const Problem<double>& problem, Solution<double>& solution, const std::shared_ptr<SolverFactory>& factory ) {
         auto solver = factory->create_solver(msg);
         solver->doSetUp(settings, problem, solution);
         Vec<int> empty_passcodes{};
         const std::pair<char, SolverStatus> &pair = solver->solve(empty_passcodes);
         msg.info("Original solve returned code {} with status {}.\n\n", (int) pair.first, pair.second);
      }

      std::shared_ptr<SolverFactory>
      load_solver_factory(){
#ifdef BUGGER_HAVE_SCIP
         return std::shared_ptr<SolverFactory>( new ScipFactory( ) );
#else
         msg.error("No solver specified -- aborting ....");
         return nullptr;
#endif
      }

      bugger::ModulStatus
      evaluateResults( ) {
         int largestValue = static_cast<int>( bugger::ModulStatus::kDidNotRun );

         for( int module = 0; module < parameters.maxstages; ++module )
            largestValue = std::max(largestValue, static_cast<int>( results[ module ] ));

         return static_cast<bugger::ModulStatus>( largestValue );
      }

      void printStats( const double &time ) {
         msg.info("\n {:>18} {:>12} {:>12} {:>18} {:>18} \n", "modules",
                  "nb calls", "changes", "success calls(%)", "execution time(s)");
         for( const auto &module: modules )
            module->printStats(msg);
         fmt::print( "\nbugging took {:.3} seconds\n", time );

      }
   };

}

#endif //BUGGER_BUGGERRUN_HPP
