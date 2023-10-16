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
#include <cctype>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <utility>

#include "bugger/misc/MultiPrecision.hpp"
#include "bugger/data/BuggerOptions.hpp"
#include "bugger/misc/OptionsParser.hpp"
#include "bugger/misc/VersionLogger.hpp"
#include "bugger/interfaces/ScipInterface.hpp"
#include "bugger/modules/BuggerModul.hpp"
#include "bugger/modules/VariableModul.hpp"
#include "bugger/misc/Vec.hpp"

#include <boost/program_options.hpp>

namespace bugger {

   class BuggerRun {

   private:
      BuggerOptions options;
      ScipInterface scip;
      Vec<std::unique_ptr<BuggerModul>>& modules;
   public:

      BuggerRun(ScipInterface &_scip, Vec<std::unique_ptr<BuggerModul>>& _modules)
            : options({ }), scip(_scip), modules( _modules )
      {
      }

      void apply( ) {
         //TODO: implement
      }

      void addDefaultModules( ) {
         using uptr = std::unique_ptr<BuggerModul>;
         addPresolveMethod(uptr(new VariableModul( )));
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
   scip.parse(optionsInfo.instance_file);
   scip.read_parameters(optionsInfo.scip_settings_file);
   scip.read_solution(optionsInfo.solution_file);

   //TODO: why can this not be auto generated in the class?
   Vec<std::unique_ptr<BuggerModul>> list{};
   BuggerRun bugger{scip, list};
   bugger.addDefaultModules( );
   parse_parameters(optionsInfo, bugger);

   bugger.apply( );

   return 0;
}

void parse_parameters(bugger::OptionsInfo &optionsInfo, bugger::BuggerRun bugger) {
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