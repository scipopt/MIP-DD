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

#ifndef _BUGGER_MISC_VERSION_LOGGER_HPP_
#define _BUGGER_MISC_VERSION_LOGGER_HPP_

#include "bugger/misc/NumericalStatistics.hpp"
#include "bugger/misc/OptionsParser.hpp"
#include "bugger/misc/tbb.hpp"
#include "scip/scipgithash.h"
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>
#include <string>
#include <utility>

namespace bugger
{

void
print_header()
{
   std::string mode = "optimized";
#ifndef NDEBUG
   mode = "debug";
#endif
   fmt::print( "MIP-Bugger version {}.{}.{} ", BUGGER_VERSION_MAJOR, BUGGER_VERSION_MINOR, BUGGER_VERSION_PATCH);

   #if defined(__INTEL_COMPILER)
   fmt::print("[Compiler: Intel {}]", __INTEL_COMPILER);
   #elif defined(__clang__)
   fmt::print("[Compiler: clang {}.{}.{}]", __clang_major__, __clang_minor__, __clang_patchlevel__);
   #elif defined(_MSC_VER)
   fmt::print("[Compiler: microsoft visual c {}]", _MSC_FULL_VER);
   #elif defined(__GNUC__)
   #if defined(__GNUC_PATCHLEVEL__)
   fmt::print("[Compiler: gcc {}.{}.{}]", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
   #else
   fmt::print("[Compiler: gcc {}.{}]", __GNUC__, __GNUC_MINOR__);
   #endif
   #else
   fmt::print("[Compiler: unknown]");
   #endif

   fmt::print("[mode: {}]", mode);
#ifdef BUGGER_GITHASH_AVAILABLE
   fmt::print("[GitHash: {}]", BUGGER_GITHASH);
#endif

   fmt::print( "\n" );
   fmt::print( "Copyright (C) 2024 Zuse Institute Berlin (ZIB)\n" );
   fmt::print( "\n" );

   fmt::print( "External libraries: \n" );

#ifdef BOOST_FOUND
   fmt::print( "  Boost    {}.{}.{} \t (https://www.boost.org/)\n",
               BOOST_VERSION_NUMBER_MINOR( BOOST_VERSION ),
               BOOST_VERSION_NUMBER_PATCH( BOOST_VERSION ) / 100,
               BOOST_VERSION_NUMBER_MAJOR( BOOST_VERSION ) );
#endif

#ifdef BUGGER_TBB
   fmt::print( "  TBB            \t Thread building block https://github.com/oneapi-src/oneTBB developed by Intel\n" );
#endif
   fmt::print( "\n" );
}



} // namespace bugger

#endif
