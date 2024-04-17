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

#include <fstream>
#include <algorithm>
#include <boost/program_options.hpp>

#include "bugger/data/BuggerRun.hpp"
#include "bugger/misc/VersionLogger.hpp"
#include "bugger/modules/ConstraintModul.hpp"
#include "bugger/modules/VariableModul.hpp"
#include "bugger/modules/CoefficientModul.hpp"
#include "bugger/modules/FixingModul.hpp"
#include "bugger/modules/SettingModul.hpp"
#include "bugger/modules/SideModul.hpp"
#include "bugger/modules/ObjectiveModul.hpp"
#include "bugger/modules/VarroundModul.hpp"
#include "bugger/modules/ConsroundModul.hpp"
#include "bugger/interfaces/ScipInterface.hpp"


using namespace bugger;

std::shared_ptr<SolverFactory<double>>
load_solver_factory( ) {
#ifdef BUGGER_HAVE_SCIP
   return std::shared_ptr<SolverFactory<double>>(new ScipFactory<double>( ));
#else
   msg.error("No solver specified -- aborting ....");
   return nullptr;
#endif
}

int
main(int argc, char *argv[]) {

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

/*
 * TODO: since the parameters need to be loaded after the modules are created (and the type initialized)
 *       the arithmetic type can not be a parameter. So the best solution is to add a cmd parameter and extract the below to a new function
 *       For exact-scip I would then check the arithmetic type and state a warning if it is not rational.
 */

   Message msg { };
   Num<double> num { };
   BuggerParameters parameters { };

   std::shared_ptr<SolverFactory<double>> factory { load_solver_factory() };
   Vec<std::unique_ptr<BuggerModul<double>>> modules { };

   modules.emplace_back(new ConstraintModul<double>(msg, num, parameters, factory));
   modules.emplace_back(new VariableModul<double>(msg, num, parameters, factory));
   modules.emplace_back(new CoefficientModul<double>(msg, num, parameters, factory));
   modules.emplace_back(new FixingModul<double>(msg, num, parameters, factory));
   SettingModul<double>* setting = new SettingModul<double>(msg, num, parameters, factory);
   modules.emplace_back(setting);
   modules.emplace_back(new SideModul<double>(msg, num, parameters, factory));
   modules.emplace_back(new ObjectiveModul<double>(msg, num, parameters, factory));
   modules.emplace_back(new VarroundModul<double>(msg, num, parameters, factory));
   modules.emplace_back(new ConsRoundModul<double>(msg, num, parameters, factory));

   if( !optionsInfo.param_settings_file.empty( ) || !optionsInfo.unparsed_options.empty( ) )
   {
      ParameterSet paramSet { };
      msg.addParameters(paramSet);
      parameters.addParameters(paramSet);
      for( const auto &module: modules )
         module->addParameters(paramSet);
      factory->addParameters(paramSet);

      if( !optionsInfo.param_settings_file.empty( ) )
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

   num.setFeasTol( parameters.feastol );
   num.setEpsilon( parameters.epsilon );
   num.setZeta( parameters.zeta );
   if( parameters.maxrounds < 0 )
      parameters.maxrounds = INT_MAX;
   if( parameters.initround < 0 || parameters.initround >= parameters.maxrounds )
      parameters.initround = parameters.maxrounds-1;
   if( parameters.maxstages < 0 || parameters.maxstages > modules.size( ) )
      parameters.maxstages = modules.size( );
   if( parameters.initstage < 0 || parameters.initstage >= parameters.maxstages )
      parameters.initstage = parameters.maxstages-1;
   if( optionsInfo.target_settings_file.empty( ) )
      setting->setEnabled(false);

   BuggerRun( msg, num, parameters, factory, modules ).apply( optionsInfo, setting );

   return 0;
}
