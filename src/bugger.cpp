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

#ifndef _BUGGER_RUN__HPP_
#define _BUGGER_RUN__HPP_

#include <algorithm>
#include <fstream>
#include <memory>
#include <utility>

#include "bugger/data/BuggerOptions.hpp"

#include "bugger/misc/MultiPrecision.hpp"
#include "bugger/misc/Vec.hpp"
#include "bugger/misc/OptionsParser.hpp"

#include "bugger/misc/VersionLogger.hpp"

#include "bugger/interfaces/ScipInterface.hpp"
#include "bugger/modules/BuggerModul.hpp"
#include "bugger/modules/CoefficientModul.hpp"
#include "bugger/modules/ConsRoundModul.hpp"
#include "bugger/modules/ConstraintModul.hpp"
//#include "bugger/modules/FixingModul.hpp"
#include "bugger/modules/ObjectiveModul.hpp"
#include "bugger/modules/SettingModul.hpp"
#include "bugger/modules/SideModul.hpp"
#include "bugger/modules/VariableModul.hpp"
#include "bugger/modules/VarroundModul.hpp"

#include <boost/program_options.hpp>

namespace bugger {

   class BuggerRun {

   private:
      BuggerOptions options;
      ScipInterface scip;
      Vec<std::unique_ptr<BuggerModul>>& modules;
      Vec<ModulStatus> results;
   public:

      BuggerRun(ScipInterface &_scip, Vec<std::unique_ptr<BuggerModul>>& _modules)
            : options({ }), scip(_scip), modules( _modules )
      {
      }

      ModulStatus
      evaluateResults()
      {
         int largestValue = static_cast<int>( ModulStatus::kDidNotRun );

         for( auto& i : results )
            largestValue = std::max( largestValue, static_cast<int>( i ) );

         return static_cast<ModulStatus>( largestValue );
      }

      void apply(Timer& timer ) {
         char filename[SCIP_MAXSTRLEN];
         int length;
         int success;

         results.resize(modules.size());
//         length = SCIPstrncpy(filename, file, SCIP_MAXSTRLEN);
//         file = filename + length;
//         length = SCIP_MAXSTRLEN - length;

         if( options.nrounds < 0 )
            options.nrounds = INT_MAX;

         if( options.nstages < 0 || options.nstages > modules.size() )
            options.nstages = modules.size();

         for( int round = 0; round < options.nrounds; ++round )
         {
            //TODO:

//            SCIPsnprintf(file, length, "_%d.set", round);
//            ( SCIPwriteParams(scip.getSCIP(), filename, FALSE, TRUE) );
//            SCIPsnprintf(file, length, "_%d.cip", round);
//            ( SCIPwriteOrigProblem(scip.getSCIP(), filename, NULL, FALSE) );

            for( int module = 0; module < modules.size(); module++ )
               //TODO: add more information about the fixings
               results[module] = modules[module]->run(scip, options, timer);
               //TODO:
            ModulStatus status = evaluateResults();
            if( status != ModulStatus::kSuccessful)
               break;
         }
         printStats();
      }

      void printStats(){
         //TODO: move msg
            Message msg {};
            msg.info( "\n {:>18} {:>12} {:>18} {:>18} {:>18} {:>18} \n", "modules",
                      "nb calls", "success calls(%)", "execution time(s)" );
            for(const auto & module : modules)
               module->printStats( msg );

         }

      void addDefaultModules( ) {
         using uptr = std::unique_ptr<BuggerModul>;
         //TODO define order
         addPresolveMethod(uptr(new CoefficientModul( )));
         addPresolveMethod(uptr(new ConsRoundModul( )));
         addPresolveMethod(uptr(new ConstraintModul( )));
//         addPresolveMethod(uptr(new FixingModul( )));
         addPresolveMethod(uptr(new ObjectiveModul( )));
         addPresolveMethod(uptr(new SideModul( )));
         addPresolveMethod(uptr(new SettingModul( )));
         addPresolveMethod(uptr(new VariableModul( )));
         addPresolveMethod(uptr(new VarroundModul( )));
      }

      void
      addPresolveMethod(std::unique_ptr<BuggerModul> module) {
         modules.emplace_back( std::move( module ) );
      }

      ParameterSet getParameters( ) {
         ParameterSet paramSet;
         options.addParameters(paramSet);
         for( const auto &module: modules )
            module->addParameters(paramSet);
         return paramSet;
      }
   };
}

void parse_parameters(bugger::OptionsInfo &optionsInfo, bugger::BuggerRun bugger);

int
main(int argc, char *argv[]) {
   using namespace bugger;

   print_header( );

   // get the options passed by the user
   OptionsInfo optionsInfo;
   try
   {
      optionsInfo = parseOptions(argc, argv);
   }
   catch( const boost::program_options::error &ex )
   {
      std::cerr << "Error while parsing the options.\n" << '\n';
      std::cerr << ex.what( ) << '\n';
      return 1;
   }

   if( !optionsInfo.is_complete )
      return 0;

   ScipInterface scip { };
   auto code = scip.parse(optionsInfo.instance_file);
   if(code != SCIP_OKAY)
   {
      std::cerr << "Error thrown by SCIP.\n";
      return 1;
   }
   scip.read_parameters(optionsInfo.scip_settings_file);
   scip.read_solution(optionsInfo.solution_file);

   //TODO: why can this not be auto generated in the class?
   Vec<std::unique_ptr<BuggerModul>> list{};
   BuggerRun bugger{scip, list};
   bugger.addDefaultModules( );
   //TODO: somehow calling this function deletes SCIP data
//   parse_parameters(optionsInfo, bugger);
   double time = 0;
   Timer timer (time);

   bugger.apply( timer );

   return 0;
}

void parse_parameters(bugger::OptionsInfo &optionsInfo, bugger::BuggerRun& bugger) {
   if( !optionsInfo.param_settings_file.empty( ) || !optionsInfo.unparsed_options.empty( ))
   {
      bugger::ParameterSet paramSet = bugger.getParameters( );
      if( !optionsInfo.param_settings_file.empty( ))
      {
         std::ifstream input(optionsInfo.param_settings_file);
         if( input )
         {
            bugger::String theoptionstr;
            bugger::String thevaluestr;
            for( bugger::String line; getline(input, line); )
            {
               std::size_t pos = line.find_first_of('#');
               if( pos != bugger::String::npos )
                  line = line.substr(0, pos);

               pos = line.find_first_of('=');

               if( pos == bugger::String::npos )
                  continue;

               theoptionstr = line.substr(0, pos - 1);
               thevaluestr = line.substr(pos + 1);

               boost::algorithm::trim(theoptionstr);
               boost::algorithm::trim(thevaluestr);

               try
               {
                  paramSet.parseParameter(theoptionstr.c_str( ),
                                          thevaluestr.c_str( ));
                  fmt::print("set {} = {}\n", theoptionstr, thevaluestr);
               }
               catch( const std::exception &e )
               {
                  fmt::print("parameter '{}' could not be set: {}\n", line,
                             e.what( ));
               }
            }
         }
         else
            fmt::print("could not read parameter file '{}'\n",
                       optionsInfo.param_settings_file);
      }

      if( !optionsInfo.unparsed_options.empty( ))
      {
         bugger::String theoptionstr;
         bugger::String thevaluestr;

         for( const auto &option: optionsInfo.unparsed_options )
         {
            std::size_t pos = option.find_first_of('=');
            if( pos != bugger::String::npos && pos > 2 )
            {
               theoptionstr = option.substr(2, pos - 2);
               thevaluestr = option.substr(pos + 1);
               try
               {
                  paramSet.parseParameter(theoptionstr.c_str( ),
                                          thevaluestr.c_str( ));
                  fmt::print("set {} = {}\n", theoptionstr, thevaluestr);
               }
               catch( const std::exception &e )
               {
                  fmt::print("parameter '{}' could not be set: {}\n",
                             option, e.what( ));
               }
            }
            else
            {
               fmt::print(
                     "parameter '{}' could not be set: value expected\n",
                     option);
            }
         }
      }
   }
} // namespace bugger

#endif