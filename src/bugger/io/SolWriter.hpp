/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*                            MIP-DD                                         */
/*                                                                           */
/* Copyright (C) 2024             Zuse Institute Berlin                      */
/*                                                                           */
/*  Licensed under the Apache License, Version 2.0 (the "License");          */
/*  you may not use this file except in compliance with the License.         */
/*  You may obtain a copy of the License at                                  */
/*                                                                           */
/*      http://www.apache.org/licenses/LICENSE-2.0                           */
/*                                                                           */
/*  Unless required by applicable law or agreed to in writing, software      */
/*  distributed under the License is distributed on an "AS IS" BASIS,        */
/*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. */
/*  See the License for the specific language governing permissions and      */
/*  limitations under the License.                                           */
/*                                                                           */
/*  You should have received a copy of the Apache-2.0 license                */
/*  along with MIP-DD; see the file LICENSE. If not visit scipopt.org.       */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef _BUGGER_IO_SOL_WRITER_HPP_
#define _BUGGER_IO_SOL_WRITER_HPP_

#include "bugger/data/Solution.hpp"

#ifdef BUGGER_USE_BOOST_IOSTREAMS_WITH_ZLIB
#include <boost/iostreams/filter/gzip.hpp>
#endif
#ifdef BUGGER_USE_BOOST_IOSTREAMS_WITH_BZIP2
#include <boost/iostreams/filter/bzip2.hpp>
#endif


namespace bugger
{
/// Writer to write solution structures into a sol file
template <typename REAL>
struct SolWriter
{
   static void
   writeSol( const String& filename, const Problem<REAL>& prob, const Solution<REAL>& sol )
   {
      if( sol.status != SolutionStatus::kFeasible )
         return;
      std::ofstream file( filename, std::ofstream::out );
      boost::iostreams::filtering_ostream out;
#ifdef PAPILO_USE_BOOST_IOSTREAMS_WITH_ZLIB
      if( boost::algorithm::ends_with( filename, ".gz" ) )
         out.push( boost::iostreams::gzip_compressor() );
#endif
#ifdef PAPILO_USE_BOOST_IOSTREAMS_WITH_BZIP2
      if( boost::algorithm::ends_with( filename, ".bz2" ) )
         out.push( boost::iostreams::bzip2_compressor() );
#endif
      out.push( file );
      fmt::print( out, "{:<35} {:}\n", "=obj=", prob.getPrimalObjective(sol) );
      for( int i = 0; i < prob.getNCols(); ++i )
      {
         if( !prob.getColFlags()[i].test( ColFlag::kInactive ) && sol.primal[i] != 0 )
            fmt::print( out, "{:<35} {:<18} obj:{:}\n",
                        prob.getVariableNames()[i], sol.primal[i], prob.getObjective().coefficients[i] );
      }
   }
};

} // namespace bugger

#endif
