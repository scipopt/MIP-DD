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
#include "bugger/modules/FixingModul.hpp"
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
      Problem<double>& problem;
      Solution<double>& solution;
      bool solution_exists;
      Vec<std::unique_ptr<BuggerModul>>& modules;
      Vec<ModulStatus> results;

      Vec<int> origcol_mapping;
      Vec<int> origrow_mapping;
      Message msg{};

   public:

      BuggerRun(Problem<double> &_problem, Solution<double>& _solution, bool _solution_exists, Vec<std::unique_ptr<BuggerModul>>& _modules)
            : options({ }), problem(_problem), solution(_solution), solution_exists(_solution_exists), modules( _modules )
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

      void apply(Timer& timer, std::string filename ) {
         results.resize(modules.size());

         //TODO: delete the variable names and constraint names also during updates
         for( unsigned int i = 0; i < problem.getNRows(); ++i )
            origrow_mapping.push_back( (int) i );

         for( unsigned int i = 0; i < problem.getNCols(); ++i )
            origcol_mapping.push_back( (int) i );

         if( options.nrounds < 0 )
            options.nrounds = INT_MAX;

         if( options.nstages < 0 || options.nstages > modules.size() )
            options.nstages = modules.size();

         int ending = 4;
         if( filename.substr( filename.length() - 3 ) == ".gz" )
            ending = 7;
         if( filename.substr( filename.length() - 3 ) == ".bz2" )
            ending = 7;

         for( int round = 0; round < options.nrounds; ++round )
         {
            for( int module = 0; module < modules.size(); module++ )
               results[module] = modules[module]->run(problem, solution, solution_exists, options, timer);

            //TODO:write also parameters
            std::string newfilename = filename.substr( 0, filename.length() - ending ) + "_" +  std::to_string(round) + ".mps";

            MpsWriter<double>::writeProb(newfilename, problem, origrow_mapping, origcol_mapping);
            ModulStatus status = evaluateResults();
            if( status != ModulStatus::kSuccessful)
               break;
         }
         printStats();
      }

      void printStats(){
            msg.info( "\n {:>18} {:>12} {:>12} {:>18} {:>18} \n", "modules",
                      "nb calls", "changes", "success calls(%)", "execution time(s)" );
            for(const auto & module : modules)
               module->printStats( msg );

         }

      void addDefaultModules( ) {
         using uptr = std::unique_ptr<BuggerModul>;
         addModul(uptr(new SettingModul( msg )));
         addModul(uptr(new ConstraintModul( msg )));
         addModul(uptr(new VariableModul( msg )));
         addModul(uptr(new SideModul( msg )));
         addModul(uptr(new ObjectiveModul( msg )));
         addModul(uptr(new CoefficientModul( msg )));
         addModul(uptr(new FixingModul( msg )));
         addModul(uptr(new VarroundModul( msg )));
         addModul(uptr(new ConsRoundModul( msg )));
      }

      void
      addModul(std::unique_ptr<BuggerModul> module) {
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

void parse_parameters(bugger::OptionsInfo &optionsInfo, bugger::ParameterSet bugger);

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

   //TODO: maybe use a static parser from scip to support more cases
   auto prob = MpsParser<double>::loadProblem( optionsInfo.instance_file );

   if( !prob )
   {
      fmt::print( "error loading problem {}\n", optionsInfo.instance_file );
      return -1;
   }
   auto problem = prob.get();
   Solution<double> sol;
   bool sol_exists = false;
   if(!optionsInfo.solution_file.empty())
   {
      bool success = SolParser<double>::read( optionsInfo.solution_file, problem.getVariableNames(), sol );
      if( !success )
      {
         fmt::print( "error loading problem {}\n", optionsInfo.instance_file );
         return -1;
      }
      sol_exists = true;
   }

   //TODO: parse settings file and hand it down
//   scip.read_parameters(optionsInfo.scip_settings_file);

   //TODO: why can this not be auto generated in the class?
   Vec<std::unique_ptr<BuggerModul>> list{};
   BuggerRun bugger{problem, sol, sol_exists, list};
   bugger.addDefaultModules( );
   //TODO scip gets deleted
//   parse_parameters(optionsInfo, bugger.getParameters());
   double time = 0;
   Timer timer (time);

   bugger.apply( timer, optionsInfo.instance_file );

   return 0;
}

void parse_parameters(bugger::OptionsInfo &optionsInfo, bugger::ParameterSet &paramSet) {
   if( !optionsInfo.param_settings_file.empty( ) || !optionsInfo.unparsed_options.empty( ))
   {
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