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
      bool solution_exists;
      bugger::Vec<std::unique_ptr<bugger::BuggerModul>> &modules;
      bugger::Vec<bugger::ModulStatus> results;
      bugger::Message msg { };

   public:

      BuggerRun(const std::string& _settings_filename, const std::string& _target_settings_filename, bugger::Problem<double> &_problem, bugger::Solution<double> &_solution,
                bool _solution_exists, bugger::Vec<std::unique_ptr<bugger::BuggerModul>> &_modules)
            : options({ }), settings_filename(_settings_filename), target_settings_filename(_target_settings_filename), problem(_problem), solution(_solution),
              solution_exists(_solution_exists), modules(_modules) { }

      bool
      is_time_exceeded( const Timer& timer ) const
      {
         return options.tlim != std::numeric_limits<double>::max() &&
                timer.getTime() >= options.tlim;
      }

      void check_feasibility_of_solution( ) {
         if(!solution_exists)
            return;
         const Vec<double>& ub = problem.getUpperBounds();
         const Vec<double>& lb = problem.getLowerBounds();
         bool failure= false;
         double max = 0;

         msg.info("\nTesting solution:\n");
         for( int col = 0; col < problem.getNCols(); col++ )
         {
            if( problem.getColFlags()[col].test( ColFlag::kInactive ) )
               continue;

            if ( ! problem.getColFlags()[col].test( ColFlag::kLbInf ) && solution.primal[col] < lb[col] )
            {
               msg.detailed( "\tColumn {} violates lower column bound () ({} ! >= {}).\n", problem.getVariableNames()[col], (double) solution.primal[col], (double) lb[col]  );
               failure = true;
               max = MAX(max, abs(lb[col]- solution.primal[col]));
            }

            if ( ! problem.getColFlags()[col].test( ColFlag::kUbInf ) && solution.primal[col] > ub[col]  )
            {
               msg.detailed( "\tColumn {} violates upper column bound ({} ! <= {}).\n", problem.getVariableNames()[col], (double) solution.primal[col], (double) ub[col]  );
               failure = true;
               max = MAX(max, abs(ub[col]- solution.primal[col]));

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

            bool lhs_inf = problem.getRowFlags()[row].test( RowFlag::kLhsInf );
            if( ( ! lhs_inf ) &&  rowValue < lhs[row]  )
            {
               msg.detailed( "\tRow {:<3} violates row bounds ({:<3} < {:<3}).\n",
                             problem.getConstraintNames()[row], (double) lhs[row], (double) rowValue );
               failure = true;
               max = MAX(max, abs(lhs[row]- rowValue));
            }
            bool rhs_inf = problem.getRowFlags()[row].test( RowFlag::kRhsInf );
            if( ( ! rhs_inf ) &&  rowValue > rhs[row] )
            {
               msg.detailed( "\tRow {:<3} violates row bounds ({:<3} < {:<3}).\n",
                         problem.getConstraintNames()[row], (double) rowValue, (double) rhs[row] );
               failure = true;
               max = MAX(max, abs(rhs[row]- rowValue));
            }
         }

         if(failure)
            msg.info("Solution is not exactly feasible (max violation: {}) using floating point arithmetic. Consider polishing the solution!\n", max);
         else
            msg.info("Solution is feasible\n");
         msg.info("\n");

      }

      void apply(bugger::Timer &timer, std::string filename) {

         SolverSettings solver_settings = parseSettings(settings_filename);

         auto solverstatus = getOriginalSolveStatus( solver_settings );

         msg.info("original instance solve-status is ");
         switch( solverstatus )
         {
            case SolverStatus::kOptimal:
               msg.info("OPTIMAL.\n");
               break;
            case SolverStatus::kUnbounded:
               msg.info("UNBOUNDED.\n");
               break;
            case SolverStatus::kInfeasible:
               msg.info("INFEASIBLE.\n");
               break;
            case SolverStatus::kInfeasibleOrUnbounded:
               msg.info("INFEASIBLE or UNBOUNDED.\n");
               break;
            case SolverStatus::kUndefinedError:
               msg.info("ERROR.\n");
               break;
            case SolverStatus::kUnknown:
               msg.info("UNKNOWN.\n");
               break;
            default:
               assert(false);
         }

         using uptr = std::unique_ptr<bugger::BuggerModul>;

         Num<double> num{};
         num.setFeasTol( options.feastol );
         num.setEpsilon( options.epsilon );

         num.setZeta( options.zeta );
         bool settings_modul_activated = !target_settings_filename.empty( );
         if( settings_modul_activated )
            addModul(uptr(new SettingModul( msg, num, solverstatus, parseSettings(target_settings_filename))));
         addModul(uptr(new ConstraintModul( msg, num, solverstatus)));
         addModul(uptr(new VariableModul( msg, num, solverstatus)));
         addModul(uptr(new SideModul( msg, num, solverstatus)));
         addModul(uptr(new ObjectiveModul( msg, num, solverstatus)));
         addModul(uptr(new CoefficientModul( msg, num, solverstatus)));
         addModul(uptr(new FixingModul( msg, num, solverstatus)));
         addModul(uptr(new VarroundModul( msg, num, solverstatus)));
         addModul(uptr(new ConsRoundModul( msg, num, solverstatus)));

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

         check_feasibility_of_solution();

         for( int round = options.initround, stage = options.initstage, success = 0; round < options.maxrounds && stage < options.maxstages; ++round )
         {
            std::string newfilename = filename.substr(0, filename.length( ) - ending) + "_" + std::to_string(round) + ".mps";
            //TODO: one can think about shrinking the matrix but I think we are fine with just ignoring deactivated columns and rows
            bugger::MpsWriter<double>::writeProb( newfilename, problem );
            if( settings_modul_activated )
            {
               std::string newsettingsname =
                     filename.substr(0, settings_filename.length( ) - ending) + "_" + std::to_string(round) + ".set";
               createSolver( )->writeSettings(newsettingsname, solver_settings);
            }

            if( is_time_exceeded(timer) )
               break;

            msg.info("Round {} Stage {}\n", round+1, stage+1);

            for( int module = 0; module <= stage && stage < options.maxstages; ++module )
            {
               results[ module ] = modules[ module ]->run(problem, solver_settings, solution, solution_exists, options, timer);

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

      SolverStatus getOriginalSolveStatus( const SolverSettings& settings) {
         auto solver = createSolver();
         solver->doSetUp(problem, settings, false, solution);
         Vec<char> empty_passcodes{};
         const std::pair<char, SolverStatus> &pair = solver->solve(empty_passcodes);
         return pair.second;
      }

      SolverSettings parseSettings( const std::string& filename) {
         auto solver = createSolver();
         return solver->parseSettings(filename);
      }

      //TODO: this is duplicates function in BuggerModul -> move this to a function to hand it to BuggerModul so that is has to be declared only once
      std::unique_ptr<SolverInterface>
      createSolver(){
#ifdef BUGGER_HAVE_SCIP
         return std::unique_ptr<SolverInterface>(new ScipInterface { });
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
