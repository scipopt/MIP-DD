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
      const std::string& setting;
      bugger::Problem<double> &problem;
      bugger::Solution<double> &solution;
      bool solution_exists;
      bugger::Vec<std::unique_ptr<bugger::BuggerModul>> &modules;
      bugger::Vec<bugger::ModulStatus> results;
      bugger::Vec<int> origcol_mapping;
      bugger::Vec<int> origrow_mapping;
      bugger::Message msg { };

   public:

      BuggerRun(const std::string& _setting, bugger::Problem<double> &_problem, bugger::Solution<double> &_solution,
                bool _solution_exists, bugger::Vec<std::unique_ptr<bugger::BuggerModul>> &_modules)
            : options({ }), setting(_setting), problem(_problem), solution(_solution),
              solution_exists(_solution_exists), modules(_modules) { }

      bool
      is_time_exceeded( const Timer& timer ) const
      {
         return options.tlim != std::numeric_limits<double>::max() &&
                timer.getTime() >= options.tlim;
      }

      bugger::BuggerOptions
      getOptions()
      {
         return options;
      }

      void apply(bugger::Timer &timer, std::string filename) {
         results.resize(modules.size( ));



         auto solverstatus = getOriginalSolveStatus( );
         for( int module = 0; module < modules.size( ); module++ )
             modules[ module ]->setOriginalSolverStatus(solverstatus);

         msg.info("original instance solve-status {}\n", solverstatus);

         for( unsigned int i = 0; i < problem.getNRows( ); ++i )
            origrow_mapping.push_back(( int ) i);

         for( unsigned int i = 0; i < problem.getNCols( ); ++i )
            origcol_mapping.push_back(( int ) i);

         if( options.maxrounds < 0 )
            options.maxrounds = INT_MAX;

         if( options.maxstages < 0 || options.maxstages > modules.size( ) )
            options.maxstages = modules.size( );

         int ending = 4;
         if( filename.substr(filename.length( ) - 3) == ".gz" )
            ending = 7;
         if( filename.substr(filename.length( ) - 3) == ".bz2" )
            ending = 7;

         for( int round = options.initround, stage = options.initstage, success = 0; round < options.maxrounds && stage < options.maxstages; ++round )
         {
            //TODO: Write also parameters
            std::string newfilename = filename.substr(0, filename.length( ) - ending) + "_" + std::to_string(round) + ".mps";
            bugger::MpsWriter<double>::writeProb(newfilename, problem, origrow_mapping, origcol_mapping);

            if( is_time_exceeded(timer) )
               break;

            for( int module = 0; module <= stage && stage < options.maxstages; ++module )
            {
               results[ module ] = modules[ module ]->run(problem, solution, solution_exists, options, timer);

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

      void addDefaultModules( const Num<double>& num ) {
         using uptr = std::unique_ptr<bugger::BuggerModul>;
         addModul(uptr(new SettingModul(setting, msg, num)));
         addModul(uptr(new ConstraintModul(setting, msg, num)));
         addModul(uptr(new VariableModul(setting, msg, num)));
         addModul(uptr(new SideModul(setting, msg, num)));
         addModul(uptr(new ObjectiveModul(setting, msg, num)));
         addModul(uptr(new CoefficientModul(setting, msg, num)));
         addModul(uptr(new FixingModul(setting, msg, num)));
         addModul(uptr(new VarroundModul(setting, msg, num)));
         addModul(uptr(new ConsRoundModul(setting, msg, num)));
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

      //TODO: this is duplicates function in BuggerModul -> move this to a function to hand it to BuggerModul so that is has to be declared only once
      SolverInterface*
      createSolver(){
#ifdef BUGGER_HAVE_SCIP
         return new ScipInterface {setting};
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
