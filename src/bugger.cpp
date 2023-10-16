/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*    PaPILO --- Parallel Presolve for Integer and Linear Optimization       */
/*                                                                           */
/* Copyright (C) 2020-2023 Konrad-Zuse-Zentrum                               */
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


#include "exact/misc/MultiPrecision.hpp"
#include "exact/misc/OptionsParser.hpp"
#include "exact/misc/VersionLogger.hpp"
#include "exact/misc/Timer.hpp"
#include "exact/interfaces/ScipInterface.hpp"
#include "exact/interfaces/ViprInterface.hpp"
#include "exact/interfaces/EventCatcher.hpp"

#include <boost/program_options.hpp>
#include <fstream>

int
main( int argc, char* argv[] )
{
   using namespace exact;

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

   return 0;
}
