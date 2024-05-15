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

#ifndef _BUGGER_IO_SOL_PARSER_HPP_
#define _BUGGER_IO_SOL_PARSER_HPP_

#include "bugger/misc/Hash.hpp"
#include "bugger/misc/String.hpp"
#include "bugger/misc/Vec.hpp"
#include "bugger/data/Solution.hpp"
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>

namespace bugger
{
template <typename REAL>
struct SolParser
{
   static boost::optional<Solution<REAL>>
   read( const std::string& filename, const Vec<String>& colnames )
   {
      std::ifstream file( filename, std::ifstream::in );
      boost::iostreams::filtering_istream in;

      if( !file )
         return boost::none;

      Solution<REAL> sol { };

#ifdef BUGGER_USE_BOOST_IOSTREAMS_WITH_ZLIB
      if( boost::algorithm::ends_with( filename, ".gz" ) )
         in.push( boost::iostreams::gzip_decompressor() );
#endif

#ifdef BUGGER_USE_BOOST_IOSTREAMS_WITH_BZIP2
      if( boost::algorithm::ends_with( filename, ".bz2" ) )
      in.push( boost::iostreams::bzip2_decompressor() );
#endif

      in.push( file );

      HashMap<String, int> nameToCol;

      for( size_t i = 0; i != colnames.size(); ++i )
      {
         nameToCol.emplace( colnames[i], i );
      }

      sol.primal.resize( colnames.size(), REAL{ 0 } );
      String strline;

      skip_header( colnames, in, strline );

      do
      {
         if( strline.empty() )
            continue;

         auto tokens = split( strline.c_str() );
         assert( !tokens.empty() );
         auto it = nameToCol.find( tokens[0] );

         if( it != nameToCol.end() )
         {
            assert( tokens.size() > 1 );
            sol.primal[it->second] = read_number( tokens[1] );
         }
         else
         {
            fmt::print( stderr,
                        "WARNING: Skipping unknown column {} in reference solution.\n",
                        tokens[0] );
         }
      }
      while( getline( in, strline ) );

      return sol;
   }

 private:

   static void
   skip_header( const Vec<String>& colnames,
                boost::iostreams::filtering_istream& filteringIstream,
                String& strline )
   {
      while(getline( filteringIstream, strline ))
      {
         for(const auto & colname : colnames)
         {
            if( strline.rfind( colname ) == 0 )
               return;
         }
      }
   }

   static Vec<String>
   split( const char* str )
   {
      Vec<String> tokens;
      char c1 = ' ';
      char c2 = '\t';

      do
      {
         const char* begin = str;

         while( *str != c1 && *str != c2 && *str )
            str++;

         tokens.emplace_back( begin, str );

         while( ( *str == c1 || *str == c2 ) && *str )
            str++;

      } while( 0 != *str );

      return tokens;
   }

   static REAL
   read_number(const std::string& s) {
      bool failed = false;
      REAL answer = 0;
      bool negated = false;
      bool dot = false;
      bool exponent = false;
      int exp = 0;
      bool exp_negated = false;
      int digits_after_dot = 0;
      for (char c : s) {
         if ('0' <= c && c <= '9') {
            int digit = c - '0';
            if(exponent)
            {
               exp *= 10;
               exp += digit;
            }
            else if( !dot )
            {
               answer *= 10;
               answer += digit;
            }
            else
            {
               digits_after_dot++;
               answer += digit /  pow( 10, digits_after_dot );
            }
         }
         else if (c == '.' && !dot)
         {
            assert(digits_after_dot == 0);
            assert(!exponent);
            dot = true;
         }
         else if (c == '-' && ((exponent && !exp_negated) || !negated))
         {
            if(exponent)
               exp_negated = true;
            else
               negated = true;
         }
         else if (c == '+' && ((exponent && !exp_negated) || !negated))
         {
         }
         else if( ( c == 'E' || c == 'e' ) && !exponent )
            exponent = true;
         else
         {
            failed = true;
            assert(false);
         }
      }
      if( !exp_negated )
         answer *= pow( 10,  exp );
      else
         answer /= pow( 10,  exp );
      return  negated ? -answer : answer;
   }

};

} // namespace bugger

#endif
