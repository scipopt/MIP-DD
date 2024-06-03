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
   read_number( const std::string& s )
   {
      REAL number { };
      bool is_rational = !std::is_same<REAL, Rational>::value;
      if( !is_rational )
      {
         std::stringstream ss;
         ss << s;
         ss >> number;
         if( ss.fail() || !ss.eof() )
         {
            fmt::print( stderr,
                        "WARNING: {} not of arithmetic {}!\n",
                        s, typeid(REAL).name() );
            number = 0;
         }
         return number;
      }
      bool negated = false;
      bool dot = false;
      bool exponent = false;
      int exp = 0;
      bool exp_negated = false;
      int digits_after_dot = 0;
      for( char c : s )
      {
         if( '0' <= c && c <= '9' )
         {
            int digit = c - '0';
            if( exponent )
            {
               exp *= 10;
               exp += digit;
            }
            else if( !dot )
            {
               number *= 10;
               number += digit;
            }
            else
            {
               ++digits_after_dot;
               number += digit / pow(10, digits_after_dot);
            }
         }
         else if( c == '.' && !dot )
         {
            assert(digits_after_dot == 0);
            assert(!exponent);
            dot = true;
         }
         else if( ( c == 'E' || c == 'e' ) && !exponent )
            exponent = true;
         else if( c == '-' && ( ( exponent && !exp_negated ) || !negated ) )
         {
            if( exponent )
               exp_negated = true;
            else
               negated = true;
         }
         else if( c != '+' || ( ( !exponent || exp_negated ) && negated ) )
         {
            fmt::print( stderr,
                        "WARNING: {} not of arithmetic {}!\n",
                        s, typeid(REAL).name() );
            number = 0;
            break;
         }
      }
      if( !exp_negated )
         number *= pow(10, exp);
      else
         number /= pow(10, exp);
      return negated ? -number : number;
   }
};

} // namespace bugger

#endif
