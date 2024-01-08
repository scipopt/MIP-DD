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
      bugger::Vec<int> origcol_mapping;
      bugger::Vec<int> origrow_mapping;
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

      bugger::BuggerOptions
      getOptions()
      {
         return options;
      }

      void apply(bugger::Timer &timer, std::string filename) {
         results.resize(modules.size( ));

         SolverSettings solver_settings = parseSettings(settings_filename);
         if( !target_settings_filename.empty( ))
         {
            auto target_solver_settings = parseSettings(target_settings_filename);
            //TODO: set target settings file to SettingsModule
            //TODO: file warning if settingsmodule is not there

            bool found = false;
            for( auto &item: modules )
               if( item->getName( ) == "setting" )
               {
                  found = true;
                  item->set_target_settings(target_solver_settings);
               }
            if( !found )
               msg.error("Settings Module not present but Target Settings File provided!");
         }
         else
         {
            for( const auto &item: modules )
               if( item->getName( ) == "setting" )
                  item->setEnabled(false);
         }
         auto solverstatus = getOriginalSolveStatus( solver_settings );
         for(const auto & module : modules)
             module->setOriginalSolverStatus(solverstatus);

         msg.info("original instance solve-status {}\n", solverstatus);

         for( unsigned int i = 0; i < problem.getNRows( ); ++i )
            origrow_mapping.push_back(( int ) i);

         for( unsigned int i = 0; i < problem.getNCols( ); ++i )
            origcol_mapping.push_back(( int ) i);

         if( options.maxrounds < 0 )
            options.maxrounds = INT_MAX;

         if( options.maxstages < 0 || options.maxstages > modules.size( ) )
            options.maxstages = (int) modules.size( );

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

      void addDefaultModules( const Num<double>& num ) {
         using uptr = std::unique_ptr<bugger::BuggerModul>;
         addModul(uptr(new SettingModul( msg, num)));
         addModul(uptr(new ConstraintModul( msg, num)));
         addModul(uptr(new VariableModul( msg, num)));
         addModul(uptr(new SideModul( msg, num)));
         addModul(uptr(new ObjectiveModul( msg, num)));
         addModul(uptr(new CoefficientModul( msg, num)));
         addModul(uptr(new FixingModul( msg, num)));
         addModul(uptr(new VarroundModul( msg, num)));
         addModul(uptr(new ConsRoundModul( msg, num)));
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
         return solver->solve( settings );
      }

      SolverSettings parseSettings( const std::string& filename) {
         auto solver = createSolver();
         assert(!filename.empty());
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
