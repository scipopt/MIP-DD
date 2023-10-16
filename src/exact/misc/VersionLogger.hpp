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

#ifndef _EXACT_MISC_VERSION_LOGGER_HPP_
#define _EXACT_MISC_VERSION_LOGGER_HPP_

#include "exact/io/MpsParser.hpp"
#include "exact/io/MpsWriter.hpp"
#include "exact/io/SolParser.hpp"
#include "exact/io/SolWriter.hpp"
#include "exact/misc/NumericalStatistics.hpp"
#include "exact/misc/OptionsParser.hpp"
#include "exact/misc/tbb.hpp"
#include "scip/scipgithash.h"
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>
#include <string>
#include <utility>

namespace exact
{

void
join( const Vec<std::string>& v, char c, std::string& s )
{

   s.clear();

   for( auto p = v.begin(); p != v.end(); ++p )
   {
      s += *p;
      if( p != v.end() - 1 )
         s += c;
   }
}

void
print_header()
{
   std::string mode = "optimized";
#ifndef NDEBUG
   mode = "debug";
#endif

#ifdef BUGGER_GITHASH_AVAILABLE
   fmt::print( "exact version {}.{}.{} [mode: {}][GitHash: {}]\n",
               BUGGER_VERSION_MAJOR, BUGGER_VERSION_MINOR, BUGGER_VERSION_PATCH,
               mode, BUGGER_GITHASH );
#else
   fmt::print( "exact version {}.{}.{} [mode: {}][GitHash: ]\n",
               BUGGER_VERSION_MAJOR, BUGGER_VERSION_MINOR, BUGGER_VERSION_PATCH,
               mode );
#endif
   fmt::print( "Copyright (C) 2020-2022 Zuse Institute Berlin (ZIB)\n" );
   fmt::print( "\n" );

   fmt::print( "External libraries: \n" );

#ifdef BOOST_FOUND
   fmt::print( "  Boost    {}.{}.{} \t (https://www.boost.org/)\n",
               BOOST_VERSION_NUMBER_MINOR( BOOST_VERSION ),
               BOOST_VERSION_NUMBER_PATCH( BOOST_VERSION ) / 100,
               BOOST_VERSION_NUMBER_MAJOR( BOOST_VERSION ) );
#endif

#ifdef EXACT_HAVE_PAPILO
   fmt::print( "  PaPILO     {}.{}.{} \t Parallel Presolving Integer und Linear Optimization "
               "developed at Zuse "
               "Institute Berlin (scip.zib.de) [GitHash: {}]\n",
               PAPILO_VERSION_MAJOR, PAPILO_VERSION_MINOR, PAPILO_VERSION_PATCH,
               SCIPgetGitHash() );
#endif

   fmt::print( "  TBB            \t Thread building block https://github.com/oneapi-src/oneTBB developed by Intel\n");


   fmt::print( "  SCIP     {}.{}.{} \t Mixed Integer Programming Solver "
               "developed at Zuse "
               "Institute Berlin (scip.zib.de) [GitHash: {}]\n",
               SCIP_VERSION_MAJOR, SCIP_VERSION_MINOR, SCIP_VERSION_PATCH,
               SCIPgetGitHash() );
   fmt::print( "\n" );
}



} // namespace exact

#endif
