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


#include "bugger/misc/MultiPrecision.hpp"
#include "bugger/misc/OptionsParser.hpp"
#include "bugger/misc/VersionLogger.hpp"
#include "bugger/misc/Timer.hpp"
#include "bugger/interfaces/ScipInterface.hpp"


#include <boost/program_options.hpp>
#include <fstream>

int
main( int argc, char* argv[] )
{
   using namespace bugger;

   print_header();

   // get the options passed by the user
   OptionsInfo optionsInfo;
   try
   {
      optionsInfo = parseOptions( argc, argv );
   }
   catch( const boost::program_options::error& ex )
   {
      std::cerr << "Error while parsing the options.\n" << '\n';
      std::cerr << ex.what() << '\n';
      return 1;
   }

   if( !optionsInfo.is_complete )
      return 0;

   double readtime = 0;

   ScipInterface scip{};
   scip.parse(optionsInfo.instance_file);
   scip.read_parameters(optionsInfo.scip_settings_file);
   scip.read_solution(optionsInfo.solution_file);

   //TODO: parse parameters

   //TODO: call reduce class to apply the reductions.

   return 0;
}
