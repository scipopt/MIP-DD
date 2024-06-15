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

#include "bugger/misc/Num.hpp"
#include "bugger/data/Solution.hpp"
#include "bugger/misc/Hash.hpp"

#ifdef BUGGER_USE_BOOST_IOSTREAMS_WITH_ZLIB
#include <boost/iostreams/filter/gzip.hpp>
#endif
#ifdef BUGGER_USE_BOOST_IOSTREAMS_WITH_BZIP2
#include <boost/iostreams/filter/bzip2.hpp>
#endif


namespace bugger
{
/// Parser for sol files storing solution information
template <typename REAL>
struct SolParser
{
   static boost::optional<Solution<REAL>>
   readSol( const String& filename, const Vec<String>& colnames )
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

      sol.primal.resize( colnames.size() );
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
            sol.primal[it->second] = parse_number<REAL>( tokens[1] );
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
   skip_header( const Vec<String>& colnames, boost::iostreams::filtering_istream& filteringIstream, String& strline )
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
};

} // namespace bugger

#endif
