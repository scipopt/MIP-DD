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

#ifndef _EXACT_MISC_WRAPPERS_HPP_
#define _EXACT_MISC_WRAPPERS_HPP_


#include "exact/io/MpsParser.hpp"
#include "exact/io/MpsWriter.hpp"
#include "exact/io/SolParser.hpp"
#include "exact/io/SolWriter.hpp"
#include "exact/misc/NumericalStatistics.hpp"
#include "exact/misc/OptionsParser.hpp"
#include "exact/misc/tbb.hpp"
#include "Timer.hpp"

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>
#include <string>
#include <utility>

namespace exact
{

enum class ResultStatus
{
   kOk = 0,
   kUnbndOrInfeas,
   kError
};

template <typename REAL>
ResultStatus
presolve_and_solve(const OptionsInfo& opts)
{
   try
   {
      double readtime = 0;
      Problem<REAL> problem;
      boost::optional<Problem<REAL>> prob;

      {
         Timer t( readtime );
         prob = MpsParser<REAL>::loadProblem( opts.instance_file );
      }

      // Check whether reading was successful or not
      if( !prob )
      {
         fmt::print( "error loading problem {}\n", opts.instance_file );
         return ResultStatus::kError;
      }
      problem = *prob;

      fmt::print( "reading took {:.3} seconds\n", readtime );

      NumericalStatistics<REAL> nstats( problem );
      nstats.printStatistics();

      if( !opts.param_settings_file.empty() || !opts.unparsed_options.empty() ||
          opts.print_params )
      {
         ParameterSet paramSet { };

         if( !opts.param_settings_file.empty() && !opts.print_params )
         {
            std::ifstream input( opts.param_settings_file );
            if( input )
            {
               String theoptionstr;
               String thevaluestr;
               for( String line; getline( input, line ); )
               {
                  std::size_t pos = line.find_first_of( '#' );
                  if( pos != String::npos )
                     line = line.substr( 0, pos );

                  pos = line.find_first_of( '=' );

                  if( pos == String::npos )
                     continue;

                  theoptionstr = line.substr( 0, pos - 1 );
                  thevaluestr = line.substr( pos + 1 );

                  boost::algorithm::trim( theoptionstr );
                  boost::algorithm::trim( thevaluestr );

                  try
                  {
                     paramSet.parseParameter( theoptionstr.c_str(),
                                              thevaluestr.c_str() );
                     fmt::print( "set {} = {}\n", theoptionstr, thevaluestr );
                  }
                  catch( const std::exception& e )
                  {
                     fmt::print( "parameter '{}' could not be set: {}\n", line,
                                 e.what() );
                  }
               }
            }
            else
            {
               fmt::print( "could not read parameter file '{}'\n",
                           opts.param_settings_file );
            }
         }

         if( !opts.unparsed_options.empty() )
         {
            String theoptionstr;
            String thevaluestr;

            for( const auto& option : opts.unparsed_options )
            {
               std::size_t pos = option.find_first_of( '=' );
               if( pos != String::npos && pos > 2 )
               {
                  theoptionstr = option.substr( 2, pos - 2 );
                  thevaluestr = option.substr( pos + 1 );
                  try
                  {
                     paramSet.parseParameter( theoptionstr.c_str(),
                                              thevaluestr.c_str() );
                     fmt::print( "set {} = {}\n", theoptionstr, thevaluestr );
                  }
                  catch( const std::exception& e )
                  {
                     fmt::print( "parameter '{}' could not be set: {}\n",
                                 option, e.what() );
                  }
               }
               else
               {
                  fmt::print(
                      "parameter '{}' could not be set: value expected\n",
                      option );
               }
            }
         }

         if( opts.print_params )
         {
            if( !opts.param_settings_file.empty() )
            {
               std::ofstream outfile( opts.param_settings_file );

               if( outfile )
               {
                  std::ostream_iterator<char> out_it( outfile );
                  paramSet.printParams( out_it );
               }
               else
               {
                  fmt::print( "could not write to parameter file '{}'\n",
                              opts.param_settings_file );
               }
            }
            else
            {
               String paramDesc;
               paramSet.printParams( std::back_inserter( paramDesc ) );
               puts( paramDesc.c_str() );
            }
         }
      }

   }
   catch( std::bad_alloc& ex )
   {
      fmt::print( "Memory out exception occured! Please assign more memory\n" );
      return ResultStatus::kError;
   }

   return ResultStatus::kOk;
}


} // namespace exact

#endif
