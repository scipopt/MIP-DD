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
#include "bugger/modules/ConsRoundModul.hpp"
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
      bugger::Problem<double> &problem;
      bugger::Solution<double> &solution;
      bool solution_exists;
      bugger::Vec<std::unique_ptr<bugger::BuggerModul>> &modules;
      bugger::Vec<bugger::ModulStatus> results;
      bugger::Vec<int> origcol_mapping;
      bugger::Vec<int> origrow_mapping;
      bugger::Message msg { };

   public:

      BuggerRun(bugger::Problem<double> &_problem, bugger::Solution<double> &_solution, bool _solution_exists,
                bugger::Vec<std::unique_ptr<bugger::BuggerModul>> &_modules)
            : options({ }), problem(_problem), solution(_solution), solution_exists(_solution_exists),
              modules(_modules) {
      }



      void apply(bugger::Timer &timer, std::string filename) {
         results.resize(modules.size( ));

         //TODO: delete the variable names and constraint names also during updates
         for( unsigned int i = 0; i < problem.getNRows( ); ++i )
            origrow_mapping.push_back(( int ) i);

         for( unsigned int i = 0; i < problem.getNCols( ); ++i )
            origcol_mapping.push_back(( int ) i);

         if( options.nrounds < 0 )
            options.nrounds = INT_MAX;

         if( options.nstages < 0 || options.nstages > modules.size( ))
            options.nstages = modules.size( );

         int ending = 4;
         if( filename.substr(filename.length( ) - 3) == ".gz" )
            ending = 7;
         if( filename.substr(filename.length( ) - 3) == ".bz2" )
            ending = 7;

         auto solverstatus = getOriginalSolveStatus( );

         for( int round = 0; round < options.nrounds; ++round )
         {
            for( int module = 0; module < modules.size( ); module++ )
               results[ module ] = modules[ module ]->run(problem, solution, solution_exists, options, timer);

            //TODO:write also parameters
            std::string newfilename =
                  filename.substr(0, filename.length( ) - ending) + "_" + std::to_string(round) + ".mps";

            bugger::MpsWriter<double>::writeProb(newfilename, problem, origrow_mapping, origcol_mapping);
            bugger::ModulStatus status = evaluateResults( );
            if( status != bugger::ModulStatus::kSuccessful )
               break;
         }
         printStats( );
      }



      void addDefaultModules( ) {
         using uptr = std::unique_ptr<bugger::BuggerModul>;
         addModul(uptr(new SettingModul(msg)));
         addModul(uptr(new ConstraintModul(msg)));
         addModul(uptr(new VariableModul(msg)));
         addModul(uptr(new SideModul(msg)));
         addModul(uptr(new ObjectiveModul(msg)));
         addModul(uptr(new CoefficientModul(msg)));
         addModul(uptr(new FixingModul(msg)));
         addModul(uptr(new VarroundModul(msg)));
         addModul(uptr(new ConsRoundModul(msg)));
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

      SolverStatus getOriginalSolveStatus( ) {
         auto solver = createSolver();
         solver->doSetUp(problem, false, solution);
         return solver->solve();
      }

      //TODO this is duplicates function in BuggerModul -> move this to a function to hand it to BuggerModul so that is has to be declared only once
      SolverInterface*
      createSolver(){
#ifdef BUGGER_HAVE_SCIP
         return new ScipInterface { };
#else
         msg.error("No solver specified -- aborting ....");
         return nullptr;
#endif
      }

      bugger::ModulStatus
      evaluateResults( ) {
         int largestValue = static_cast<int>( bugger::ModulStatus::kDidNotRun );

         for( auto &i: results )
            largestValue = std::max(largestValue, static_cast<int>( i ));

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
