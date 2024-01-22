/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*    BUGGER                                                                 */
/*                                                                           */
/* Copyright (C) 2023             Konrad-Zuse-Zentrum                        */
/*                     fuer Informationstechnik Berlin                       */
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

#include <boost/program_options.hpp>
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
#include "bugger/interfaces/ScipInterface.hpp"
#include "bugger/misc/VersionLogger.hpp"
#include "bugger/misc/OptionsParser.hpp"
#include "bugger/misc/Vec.hpp"
#include "bugger/misc/MultiPrecision.hpp"
#include "bugger/data/BuggerOptions.hpp"
#include <utility>
#include <memory>
#include <fstream>
#include <algorithm>

namespace bugger {

   class BuggerRun {

   private:
      bugger::BuggerOptions options;
      const std::string& settings_filename;
      const std::string& target_settings_filename;
      bugger::Problem<double> &problem;
      bugger::Solution<double> &solution;
      bugger::Vec<std::unique_ptr<bugger::BuggerModul>> &modules;
      bugger::Vec<bugger::ModulStatus> results;
      bugger::Message msg { };

   public:

      BuggerRun(const std::string& _settings_filename, const std::string& _target_settings_filename, bugger::Problem<double> &_problem, bugger::Solution<double> &_solution,
                bugger::Vec<std::unique_ptr<bugger::BuggerModul>> &_modules)
            : options({ }), settings_filename(_settings_filename), target_settings_filename(_target_settings_filename), problem(_problem), solution(_solution),
              modules(_modules) { }

      bool
      is_time_exceeded( const Timer& timer ) const
      {
         return options.tlim != std::numeric_limits<double>::max() &&
                timer.getTime() >= options.tlim;
      }

      void apply(bugger::Timer &timer, std::string filename) {

         check_feasibility_of_solution();

         auto solver_factory = load_solver_factory();

         SolverSettings solver_settings = parseSettings(settings_filename, solver_factory);

         auto solverstatus = getOriginalSolveStatus(solver_settings, solver_factory);

         msg.info("original instance solve-status is {}\n", solverstatus);

         using uptr = std::unique_ptr<bugger::BuggerModul>;

         Num<double> num{};
         num.setFeasTol( options.feastol );
         num.setEpsilon( options.epsilon );
         num.setZeta( options.zeta );

         bool settings_modul_activated = !target_settings_filename.empty( );
         if( settings_modul_activated )
            addModul(uptr(new SettingModul( msg, num,parseSettings(target_settings_filename, solver_factory), solver_factory)));
         addModul(uptr(new ConstraintModul( msg, num, solver_factory)));
         addModul(uptr(new VariableModul( msg, num, solver_factory)));
         addModul(uptr(new SideModul( msg, num, solver_factory)));
         addModul(uptr(new ObjectiveModul( msg, num, solver_factory)));
         addModul(uptr(new CoefficientModul( msg, num, solver_factory)));
         addModul(uptr(new FixingModul( msg, num, solver_factory)));
         addModul(uptr(new VarroundModul( msg, num, solver_factory)));
         addModul(uptr(new ConsRoundModul( msg, num, solver_factory)));

         if( options.maxrounds < 0 )
            options.maxrounds = INT_MAX;

         results.resize(modules.size( ));

         if( options.maxstages < 0 || options.maxstages > modules.size( ) )
            options.maxstages = (int) modules.size( );

         int ending = 4;
         if( filename.substr(filename.length( ) - 3) == ".gz" )
            ending = 7;
         if( filename.substr(filename.length( ) - 3) == ".bz2" )
            ending = 7;

         for( int round = options.initround, stage = options.initstage, success = 0; round < options.maxrounds && stage < options.maxstages; ++round )
         {
            //TODO: one can think about shrinking the matrix in each round
            solver_factory->create_solver( )->writeInstance(filename.substr(0, filename.length( ) - ending) + "_" + std::to_string(round), solver_settings, problem, settings_modul_activated);

            if( is_time_exceeded(timer) )
               break;

            msg.info("Round {} Stage {}\n", round+1, stage+1);

            for( int module = 0; module <= stage && stage < options.maxstages; ++module )
            {
               results[ module ] = modules[ module ]->run(problem, solver_settings, solution, options, timer);

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
         printStats( );
      }
      
      void
      addModul(std::unique_ptr<bugger::BuggerModul> module) {
         modules.emplace_back(std::move(module));
      }

      ParameterSet getParameters( ) {
         ParameterSet paramSet;
         options.addParameters(paramSet);
         for( const auto &module: modules )
            module->addParameters(paramSet);
         return paramSet;
      }

   private:

      void check_feasibility_of_solution( ) {
         if( solution.status != SolutionStatus::kFeasible )
            return;
         const Vec<double>& ub = problem.getUpperBounds();
         const Vec<double>& lb = problem.getLowerBounds();
         double maxviol = 0.0;
         int maxindex = -1;
         bool maxrow = false;
         bool maxupper = false;
         double viol;

         msg.info("\nCheck:\n");
         for( int col = 0; col < problem.getNCols(); col++ )
         {
            if( problem.getColFlags()[col].test( ColFlag::kInactive ) )
               continue;

            if ( !problem.getColFlags()[col].test( ColFlag::kLbInf ) && solution.primal[col] < lb[col] )
            {
               msg.detailed( "\tColumn {:<3} violates lower bound ({:<3} < {:<3}).\n", problem.getVariableNames()[col], solution.primal[col], lb[col] );
               viol = lb[col] - solution.primal[col];
               if( viol > maxviol )
               {
                  maxviol = viol;
                  maxindex = col;
                  maxrow = false;
                  maxupper = false;
               }
            }

            if ( !problem.getColFlags()[col].test( ColFlag::kUbInf ) && solution.primal[col] > ub[col] )
            {
               msg.detailed( "\tColumn {:<3} violates upper bound ({:<3} > {:<3}).\n", problem.getVariableNames()[col], solution.primal[col], ub[col] );
               viol = solution.primal[col] - ub[col];
               if( viol > maxviol )
               {
                  maxviol = viol;
                  maxindex = col;
                  maxrow = false;
                  maxupper = true;
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
               }
            }
         }

         if( maxindex >= 0 )
            msg.info("Solution is infeasible.\nMaximum violation {:<3} of {} {:<3} {}\n", maxviol, maxrow ? "row" : "column", (maxrow ? problem.getConstraintNames() : problem.getVariableNames())[maxindex], maxrow ? (maxupper ? "right" : "left") : (maxupper ? "upper" : "lower"));
         else
            msg.info("Solution is feasible.\nNo violations detected");
         msg.info("\n");
      }

      SolverStatus getOriginalSolveStatus(const SolverSettings &settings, const std::shared_ptr<SolverFactory>& factory) {
         auto solver = factory->create_solver();
         solver->doSetUp(problem, settings, solution);
         Vec<int> empty_passcodes{};
         const std::pair<char, SolverStatus> &pair = solver->solve(empty_passcodes);
         return pair.second;
      }

      SolverSettings parseSettings( const std::string& filename, const std::shared_ptr<SolverFactory>& factory) {
         auto solver = factory->create_solver();
         return solver->parseSettings(filename);
      }

      std::shared_ptr<SolverFactory>
      load_solver_factory(){
#ifdef BUGGER_HAVE_SCIP
         return std::shared_ptr<SolverFactory>(new ScipFactory( ) );
#else
         msg.error("No solver specified -- aborting ....");
         return nullptr;
#endif
      }

      bugger::ModulStatus
      evaluateResults( ) {
         int largestValue = static_cast<int>( bugger::ModulStatus::kDidNotRun );

         for( int module = 0; module < options.maxstages; ++module )
            largestValue = std::max(largestValue, static_cast<int>( results[ module ] ));

         return static_cast<bugger::ModulStatus>( largestValue );
      }

      void printStats( ) {
         msg.info("\n {:>18} {:>12} {:>12} {:>18} {:>18} \n", "modules",
                  "nb calls", "changes", "success calls(%)", "execution time(s)");
         for( const auto &module: modules )
            module->printStats(msg);

      }
   };

}

#endif //BUGGER_BUGGERRUN_HPP
