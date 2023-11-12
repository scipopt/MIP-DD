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
#include "bugger/data/BuggerRun.hpp"

#include <boost/program_options.hpp>

namespace bugger {

} // namespace bugger

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

   //TODO: think how to implement this. and handle more cases
//   ScipInterface scip{};
//   auto prob = scip.readProblem(optionsInfo.instance_file);
   auto prob = MpsParser<double>::loadProblem(optionsInfo.instance_file);

   if( !prob )
   {
      fmt::print("error loading problem {}\n", optionsInfo.instance_file);
      return -1;
   }
   auto problem = prob.get( );
   Solution<double> sol;
   bool sol_exists = false;
   if( !optionsInfo.solution_file.empty( ))
   {
      bool success = SolParser<double>::read(optionsInfo.solution_file, problem.getVariableNames( ), sol);
      if( !success )
      {
         fmt::print("error loading problem {}\n", optionsInfo.instance_file);
         return -1;
      }
      sol_exists = true;
   }


   //TODO: why can this not be auto generated in the class?
   Vec<std::unique_ptr<BuggerModul>> list { };
   BuggerRun bugger { problem, sol, sol_exists, list };

   if( !optionsInfo.param_settings_file.empty( ) || !optionsInfo.unparsed_options.empty( ))
   {
      auto paramSet = bugger.getParameters( );
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


   Num<double> num{};
   num.setFeasTol( bugger.getOptions().feastol );
   num.setEpsilon( bugger.getOptions().epsilon );
   num.setZeta( bugger.getOptions().zeta );
   bugger.addDefaultModules( num );

   double time = 0;
   Timer timer(time);

   bugger.apply(timer, optionsInfo.instance_file);

   return 0;
}


#endif