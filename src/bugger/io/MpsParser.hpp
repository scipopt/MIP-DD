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

#ifndef _BUGGER_IO_MPS_PARSER_HPP_
#define _BUGGER_IO_MPS_PARSER_HPP_

#include <boost/algorithm/string.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/utility/string_ref.hpp>
#include "bugger/misc/Num.hpp"
#include "bugger/data/Problem.hpp"
#include "bugger/misc/Hash.hpp"

#ifdef BUGGER_USE_BOOST_IOSTREAMS_WITH_ZLIB
#include <boost/iostreams/filter/gzip.hpp>
#endif
#ifdef BUGGER_USE_BOOST_IOSTREAMS_WITH_BZIP2
#include <boost/iostreams/filter/bzip2.hpp>
#endif


namespace bugger
{

/// Parser for mps files in fixed and free format
template <typename REAL>
class MpsParser
{

public:
   static boost::optional<Problem<REAL>>
   readProb( const std::string& filename )
   {
      MpsParser<REAL> parser;

      Problem<REAL> problem;

      if( !parser.parseFile( filename ) )
         return boost::none;

      assert( parser.nnz >= 0 );

      Vec<REAL> obj_vec( size_t( parser.nCols ) );

      for( auto i : parser.coeffobj )
         obj_vec[i.first] = i.second;

      problem.setObjective( std::move( obj_vec ), parser.objoffset, parser.objsense );
      problem.setConstraintMatrix(
          SparseStorage<REAL>{ std::move( parser.entries ), parser.nCols,
                               parser.nRows, true },
          std::move( parser.rowlhs ), std::move( parser.rowrhs ),
          std::move( parser.row_flags ), true );
      problem.setVariableDomains( std::move( parser.lb4cols ),
                                  std::move( parser.ub4cols ),
                                  std::move( parser.col_flags ) );
      problem.setVariableNames( std::move( parser.colnames ) );
      problem.setName( std::move( parser.probname ) );
      problem.setConstraintNames( std::move( parser.rownames ) );

      problem.setInputTolerance( 0 );

      return problem;
   }

private:
   MpsParser() {}

   /// load LP from MPS file as transposed triplet matrix
   bool
   parseFile( const std::string& filename );

   bool
   parse( boost::iostreams::filtering_istream& file );

   enum class boundtype
   {
      kLE,
      kEq,
      kGE
   };

   enum class parsekey
   {
      kObjsense,
      kRows,
      kCols,
      kRhs,
      kRanges,
      kBounds,
      kNone,
      kEnd,
      kFail,
      kComment
   };

   void
   printErrorMessage( parsekey keyword )
   {
      switch( keyword )
      {
      case parsekey::kObjsense:
         std::cerr << "read error in section OBJSENSE" << std::endl;
         break;
      case parsekey::kRows:
         std::cerr << "read error in section ROWS" << std::endl;
         break;
      case parsekey::kCols:
         std::cerr << "read error in section COLUMNS" << std::endl;
         break;
      case parsekey::kRhs:
         std::cerr << "read error in section RHS" << std::endl;
         break;
      case parsekey::kBounds:
         std::cerr << "read error in section BOUNDS" << std::endl;
         break;
      case parsekey::kRanges:
         std::cerr << "read error in section RANGES" << std::endl;
         break;
      default:
         std::cerr << "undefined read error" << std::endl;
         break;
      }
   };

   /*
    * data for mps problem
    */

   std::string probname { };
   Vec<Triplet<REAL>> entries;
   Vec<std::pair<int, REAL>> coeffobj;
   Vec<REAL> rowlhs;
   Vec<REAL> rowrhs;
   Vec<std::string> rownames;
   Vec<std::string> colnames;

   HashMap<std::string, int> rowname2idx;
   HashMap<std::string, int> colname2idx;
   Vec<REAL> lb4cols;
   Vec<REAL> ub4cols;
   Vec<boundtype> row_type;
   Vec<RowFlags> row_flags;
   Vec<ColFlags> col_flags;
   REAL objoffset = 0;
   bool objsense = true;

   int nCols = 0;
   int nRows = 0;
   int nnz = -1;

   /// checks first word of strline and wraps it by it_begin and it_end
   parsekey
   checkFirstWord( std::string& strline, std::string::iterator& it,
                   boost::string_ref& word_ref );

   parsekey
   parseDefault( boost::iostreams::filtering_istream& file );

   parsekey
   parseObjsense( boost::iostreams::filtering_istream& file );

   parsekey
   parseRows( boost::iostreams::filtering_istream& file,
              Vec<boundtype>& rowtype );

   parsekey
   parseCols( boost::iostreams::filtering_istream& file,
              const Vec<boundtype>& rowtype );

   parsekey
   parseRhs( boost::iostreams::filtering_istream& file );

   parsekey
   parseRanges( boost::iostreams::filtering_istream& file );

   parsekey
   parseBounds( boost::iostreams::filtering_istream& file );
};

template <typename REAL>
typename MpsParser<REAL>::parsekey
MpsParser<REAL>::checkFirstWord( std::string& strline,
                                 std::string::iterator& it,
                                 boost::string_ref& word_ref )
{
   using namespace boost::spirit;

   it = strline.begin() + strline.find_first_not_of( " " );
   std::string::iterator it_start = it;

   // TODO: Daniel
   qi::parse( it, strline.end(), qi::lexeme[+qi::graph] );

   const std::size_t length = std::distance( it_start, it );

   boost::string_ref word( &( *it_start ), length );

   word_ref = word;

   if( word.front() == 'R' ) // todo
   {
      if( word == "ROWS" )
         return MpsParser<REAL>::parsekey::kRows;
      else if( word == "RHS" )
         return MpsParser<REAL>::parsekey::kRhs;
      else if( word == "RANGES" )
         return MpsParser<REAL>::parsekey::kRanges;
      else
         return MpsParser<REAL>::parsekey::kNone;
   }
   else if( word == "NAME" )
   {
      probname = boost::trim_copy( std::string( it_start+4, strline.end() ) );
      return MpsParser<REAL>::parsekey::kNone;
   }
   else if( word == "OBJSENSE" )
      return MpsParser<REAL>::parsekey::kObjsense;
   else if( word == "COLUMNS" )
      return MpsParser<REAL>::parsekey::kCols;
   else if( word == "BOUNDS" )
      return MpsParser<REAL>::parsekey::kBounds;
   else if( word == "ENDATA" )
      return MpsParser<REAL>::parsekey::kEnd;
   else
      return MpsParser<REAL>::parsekey::kNone;
}

template <typename REAL>
typename MpsParser<REAL>::parsekey
MpsParser<REAL>::parseDefault( boost::iostreams::filtering_istream& file )
{
   std::string strline;
   getline( file, strline );

   std::string::iterator it;
   boost::string_ref word_ref;
   return checkFirstWord( strline, it, word_ref );
}

template <typename REAL>
typename MpsParser<REAL>::parsekey
MpsParser<REAL>::parseObjsense( boost::iostreams::filtering_istream& file )
{
   std::string strline;

   while( getline( file, strline ) )
   {
      std::string::iterator it;
      boost::string_ref word_ref;
      MpsParser<REAL>::parsekey key = checkFirstWord( strline, it, word_ref );

      if( key != parsekey::kNone )
         return key;

      if( word_ref.starts_with("MIN") )
         objsense = true;
      else if( word_ref.starts_with("MAX") )
         objsense = false;
   }

   return parsekey::kFail;
}

template <typename REAL>
typename MpsParser<REAL>::parsekey
MpsParser<REAL>::parseRows( boost::iostreams::filtering_istream& file,
                            Vec<boundtype>& rowtype )
{
   using namespace boost::spirit;

   std::string strline;
   size_t nrows = 0;
   bool hasobj = false;

   while( getline( file, strline ) )
   {
      bool isobj = false;
      std::string::iterator it;
      boost::string_ref word_ref;
      MpsParser<REAL>::parsekey key = checkFirstWord( strline, it, word_ref );

      // start of new section?
      if( key != parsekey::kNone )
      {
         nRows = int( nrows );
         if( !hasobj )
         {
            std::cout << "WARNING: no objective row found" << std::endl;
            rowname2idx.emplace( "artificial_empty_objective", -1 );
         }

         return key;
      }

      if( word_ref.front() == 'G' )
      {
         rowlhs.push_back( 0 );
         rowrhs.push_back( 0 );
         row_flags.emplace_back( RowFlag::kRhsInf );
         rowtype.push_back( boundtype::kGE );
      }
      else if( word_ref.front() == 'E' )
      {
         rowlhs.push_back( 0 );
         rowrhs.push_back( 0 );
         row_flags.emplace_back( RowFlag::kEquation );
         rowtype.push_back( boundtype::kEq );
      }
      else if( word_ref.front() == 'L' )
      {
         rowlhs.push_back( 0 );
         rowrhs.push_back( 0 );
         row_flags.emplace_back( RowFlag::kLhsInf );
         rowtype.push_back( boundtype::kLE );
      }
      // todo properly treat multiple free rows
      else if( word_ref.front() == 'N' )
      {
         if( hasobj )
         {
            rowlhs.push_back( 0 );
            rowrhs.push_back( 0 );
            RowFlags rowf;
            rowf.set( RowFlag::kLhsInf, RowFlag::kRhsInf );
            row_flags.emplace_back( rowf );
            rowtype.push_back( boundtype::kLE );
         }
         else
         {
            isobj = true;
            hasobj = true;
         }
      }
      else if( word_ref.empty() ) // empty line
         continue;
      else
         return parsekey::kFail;

      std::string rowname = ""; // todo use ref

      // get row name
      qi::phrase_parse( it, strline.end(), qi::lexeme[+qi::graph], ascii::space,
                        rowname ); // todo use ref

      // todo whitespace in name possible?
      auto ret = rowname2idx.emplace( rowname, isobj ? ( -1 ) : ( nrows++ ) );

      if( !isobj )
         rownames.push_back( rowname );

      if( !ret.second )
      {
         std::cerr << "duplicate row " << rowname << std::endl;
         return parsekey::kFail;
      }
   }

   return parsekey::kFail;
}

template <typename REAL>
typename MpsParser<REAL>::parsekey
MpsParser<REAL>::parseCols( boost::iostreams::filtering_istream& file,
                            const Vec<boundtype>& rowtype )
{
   using namespace boost::spirit;

   std::string colname = "";
   std::string strline;
   int rowidx;
   int ncols = 0;
   int colstart = 0;
   bool integral_cols = false;

   auto parsename = [&rowidx, this]( std::string name ) {
      auto mit = rowname2idx.find( name );

      assert( mit != rowname2idx.end() );
      rowidx = mit->second;

      if( rowidx >= 0 )
         this->nnz++;
      else
         assert( -1 == rowidx );
   };

   auto addtuple = [&rowidx, &ncols, this]( std::string sval) {
      auto coeff = parse_number<REAL>(sval);
      if( rowidx >= 0 )
         entries.push_back(
             std::make_tuple( ncols - 1, rowidx, REAL{ coeff } ) );
      else
         coeffobj.push_back( std::make_pair( ncols - 1, REAL{ coeff } ) );
   };

   while( getline( file, strline ) )
   {
      std::string::iterator it;
      boost::string_ref word_ref;
      MpsParser<REAL>::parsekey key = checkFirstWord( strline, it, word_ref );

      // start of new section?
      if( key != parsekey::kNone )
      {
         if( ncols > 1 )
            pdqsort( entries.begin() + colstart, entries.end(),
                     []( Triplet<REAL> a, Triplet<REAL> b ) {
                        return std::get<1>( b ) > std::get<1>( a );
                     } );

         return key;
      }

      // check for integrality marker
      std::string marker = ""; // todo use ref
      std::string::iterator it2 = it;

      qi::phrase_parse( it2, strline.end(), qi::lexeme[+qi::graph],
                        ascii::space, marker );

      if( marker == "'MARKER'" )
      {
         marker = "";
         qi::phrase_parse( it2, strline.end(), qi::lexeme[+qi::graph],
                           ascii::space, marker );

         if( ( integral_cols && marker != "'INTEND'" ) ||
             ( !integral_cols && marker != "'INTORG'" ) )
         {
            std::cerr << "integrality marker error " << std::endl;
            return parsekey::kFail;
         }
         integral_cols = !integral_cols;

         continue;
      }

      // new column?
      if( !( word_ref == colname ) )
      {
         if( word_ref.empty() ) // empty line
            continue;

         colname = word_ref.to_string();
         auto ret = colname2idx.emplace( colname, ncols++ );
         colnames.push_back( colname );

         if( !ret.second )
         {
            std::cerr << "duplicate column " << std::endl;
            return parsekey::kFail;
         }

         assert( lb4cols.size() == col_flags.size() );

         col_flags.emplace_back( integral_cols ? ColFlag::kIntegral
                                               : ColFlag::kNone );

         // initialize with default bounds
         if( integral_cols )
         {
            lb4cols.push_back( 0 );
            ub4cols.push_back( 1 );
         }
         else
         {
            lb4cols.push_back( 0 );
            ub4cols.push_back( 0 );
            col_flags.back().set( ColFlag::kUbInf );
         }

         assert( col_flags.size() == lb4cols.size() );

         if( ncols > 1 )
            pdqsort( entries.begin() + colstart, entries.end(),
                     []( Triplet<REAL> a, Triplet<REAL> b ) {
                        return std::get<1>( b ) > std::get<1>( a );
                     } );

         colstart = entries.size();
      }

      assert( ncols > 0 );

      std::istringstream is( strline );
      std::vector<std::string> tokens;
      std::string tmp;
      while( is >> tmp )
         tokens.push_back( tmp );
      if( tokens.size() != 3 && tokens.size() != 5 )
         return parsekey::kFail;
      parsename( tokens[1] );
      addtuple( tokens[2] );
      if( tokens.size() == 5 )
      {
         parsename( tokens[3] );
         addtuple( tokens[4] );
      }
   }

   return parsekey::kFail;
}

template <typename REAL>
typename MpsParser<REAL>::parsekey
MpsParser<REAL>::parseRanges( boost::iostreams::filtering_istream& file )
{
   using namespace boost::spirit;
   std::string strline;
   assert( rowrhs.size() == rowlhs.size() );

   while( getline( file, strline ) )
   {
      std::string::iterator it;
      boost::string_ref word_ref;
      MpsParser<REAL>::parsekey key = checkFirstWord( strline, it, word_ref );

      // start of new section?
      if( key != parsekey::kNone && key != parsekey::kRanges )
         return key;

      if( word_ref.empty() )
         continue;

      int rowidx;

      auto parsename = [&rowidx, this]( std::string name ) {
         auto mit = rowname2idx.find( name );

         assert( mit != rowname2idx.end() );
         rowidx = mit->second;

         assert( rowidx >= 0 && rowidx < nRows );
      };

      auto addrange = [&rowidx, this]( std::string sval ) {
         REAL val  = parse_number<REAL>(sval);
         assert( size_t( rowidx ) < rowrhs.size() );

         if( row_type[rowidx] == boundtype::kGE )
         {
            row_flags[rowidx].unset( RowFlag::kRhsInf );
            rowrhs[rowidx] = rowlhs[rowidx] + REAL(abs( val ));
         }
         else if( row_type[rowidx] == boundtype::kLE )
         {
            row_flags[rowidx].unset( RowFlag::kLhsInf );
            rowlhs[rowidx] = rowrhs[rowidx] - REAL(abs( val ));
         }
         else
         {
            assert( row_type[rowidx] == boundtype::kEq );
            assert( rowrhs[rowidx] == rowlhs[rowidx] );
            assert( row_flags[rowidx].test(RowFlag::kEquation) );

            if( val > 0 )
            {
               row_flags[rowidx].unset( RowFlag::kEquation );
               rowrhs[rowidx] = rowrhs[rowidx] + REAL( val );
            }
            else if( val < 0 )
            {
               rowlhs[rowidx] = rowlhs[rowidx] + REAL( val );
               row_flags[rowidx].unset( RowFlag::kEquation );
            }
         }
      };

      std::istringstream is( strline );
      std::vector<std::string> tokens;
      std::string tmp;
      while( is >> tmp )
         tokens.push_back( tmp );
      if( tokens.size() != 3 && tokens.size() != 5 )
         return parsekey::kFail;
      parsename( tokens[1] );
      addrange( tokens[2] );
      if( tokens.size() == 5 )
      {
         parsename( tokens[3] );
         addrange( tokens[4] );
      }
   }

   return parsekey::kFail;
}

template <typename REAL>
typename MpsParser<REAL>::parsekey
MpsParser<REAL>::parseRhs( boost::iostreams::filtering_istream& file )
{
   using namespace boost::spirit;
   std::string strline;

   while( getline( file, strline ) )
   {
      std::string::iterator it;
      boost::string_ref word_ref;
      MpsParser<REAL>::parsekey key = checkFirstWord( strline, it, word_ref );

      // start of new section?
      if( key != parsekey::kNone && key != parsekey::kRhs )
         return key;

      if( word_ref.empty() )
         continue;

      int rowidx;

      auto parsename = [&rowidx, this]( std::string name ) {
         auto mit = rowname2idx.find( name );

         assert( mit != rowname2idx.end() );
         rowidx = mit->second;

         assert( rowidx >= -1 );
         assert( rowidx < nRows );
      };

      auto addrhs = [&rowidx, this]( String sval ) {
         REAL val = parse_number<REAL>(sval);
         if( rowidx == -1 )
         {
            objoffset = -REAL{ val };
            return;
         }
         if( row_type[rowidx] == boundtype::kEq ||
             row_type[rowidx] == boundtype::kLE )
         {
            assert( size_t( rowidx ) < rowrhs.size() );
            rowrhs[rowidx] = REAL{ val };
            row_flags[rowidx].unset( RowFlag::kRhsInf );
         }

         if( row_type[rowidx] == boundtype::kEq ||
             row_type[rowidx] == boundtype::kGE )
         {
            assert( size_t( rowidx ) < rowlhs.size() );
            rowlhs[rowidx] = REAL{ val };
            row_flags[rowidx].unset( RowFlag::kLhsInf );
         }
      };

      std::istringstream is( strline );
      std::vector<std::string> tokens;
      std::string tmp;
      while( is >> tmp )
         tokens.push_back( tmp );
      if( tokens.size() != 3 && tokens.size() != 5 )
         return parsekey::kFail;
      parsename( tokens[1] );
      addrhs( tokens[2] );
      if( tokens.size() == 5 )
      {
         parsename( tokens[3] );
         addrhs( tokens[4] );
      }
   }

   return parsekey::kFail;
}

template <typename REAL>
typename MpsParser<REAL>::parsekey
MpsParser<REAL>::parseBounds( boost::iostreams::filtering_istream& file )
{
   using namespace boost::spirit;
   std::string strline;

   Vec<bool> ub_is_default( lb4cols.size(), true );
   Vec<bool> lb_is_default( lb4cols.size(), true );

   while( getline( file, strline ) )
   {
      std::string::iterator it;
      boost::string_ref word_ref;
      MpsParser<REAL>::parsekey key = checkFirstWord( strline, it, word_ref );

      // start of new section?
      if( key != parsekey::kNone )
         return key;

      if( word_ref.empty() )
         continue;

      bool islb = false;
      bool isub = false;
      bool isintegral = false;
      bool isdefaultbound = false;

      if( word_ref == "UP" ) // lower bound
         isub = true;
      else if( word_ref == "LO" ) // upper bound
         islb = true;
      else if( word_ref == "FX" ) // fixed
      {
         islb = true;
         isub = true;
      }
      else if( word_ref == "MI" ) // infinite lower bound
      {
         islb = true;
         isdefaultbound = true;
      }
      else if( word_ref == "PL" ) // infinite upper bound (redundant)
      {
         isub = true;
         isdefaultbound = true;
      }
      else if( word_ref == "BV" ) // binary
      {
         isintegral = true;
         isdefaultbound = true;
         islb = true;
         isub = true;
      }
      else if( word_ref == "LI" ) // integer lower bound
      {
         islb = true;
         isintegral = true;
      }
      else if( word_ref == "UI" ) // integer upper bound
      {
         isub = true;
         isintegral = true;
      }
      else if( word_ref == "FR" ) // free variable
      {
         islb = true;
         isub = true;
         isdefaultbound = true;
      }
      else
      {
         if( word_ref == "INDICATORS" )
            std::cerr << "bugger does not support INDICATORS in the MPS file!!"<< std::endl;
         else
            std::cerr << "unknown bound type " << word_ref << std::endl;
         return parsekey::kFail;
      }

      // parse over next word
      qi::phrase_parse( it, strline.end(), qi::lexeme[+qi::graph],
                        ascii::space );

      int colidx;

      auto parsename = [&colidx, this]( std::string name ) {
         auto mit = colname2idx.find( name );
         assert( mit != colname2idx.end() );
         colidx = mit->second;
         assert( colidx >= 0 );
      };

      if( isdefaultbound )
      {
         if( !qi::phrase_parse(
                 it, strline.end(),
                 ( qi::lexeme[qi::as_string[+qi::graph][( parsename )]] ),
                 ascii::space ) )
            return parsekey::kFail;

         if( isintegral ) // binary
         {
            if( islb )
               lb4cols[colidx] = 0;
            if( isub )
            {
               col_flags[colidx].unset( ColFlag::kUbInf );
               ub4cols[colidx] = 1;
            }
            col_flags[colidx].set( ColFlag::kIntegral );
         }
         else
         {
            if( islb )
               col_flags[colidx].set( ColFlag::kLbInf );
            if( isub )
               col_flags[colidx].set( ColFlag::kUbInf );
         }
         continue;
      }

      auto adddomains = [&ub_is_default, &lb_is_default, &colidx, &islb, &isub, &isintegral, this]
            ( std::string sval )
      {
         auto val = parse_number<REAL>(sval);
         if( islb )
         {
            lb4cols[colidx] = REAL{ val };
            lb_is_default[colidx] = false;
            col_flags[colidx].unset( ColFlag::kLbInf );
         }
         if( isub )
         {
            ub4cols[colidx] = REAL{ val };
            ub_is_default[colidx] = false;
            col_flags[colidx].unset( ColFlag::kUbInf );
         }

         if( isintegral )
            col_flags[colidx].set( ColFlag::kIntegral );

         if( col_flags[colidx].test( ColFlag::kIntegral ) )
         {
            col_flags[colidx].set( ColFlag::kIntegral );
            if( !islb && lb_is_default[colidx] )
               lb4cols[colidx] = REAL{ 0.0 };
            if( !isub && ub_is_default[colidx] )
               col_flags[colidx].set( ColFlag::kUbInf );
         }
      };

      std::istringstream is( strline );
      std::vector<std::string> tokens;
      std::string tmp;
      while( is >> tmp )
         tokens.push_back( tmp );
      if( tokens.size() != 4 )
         return parsekey::kFail;
      parsename( tokens[2] );
      adddomains( tokens[3] );
   }

   return parsekey::kFail;
}

template <typename REAL>
bool
MpsParser<REAL>::parseFile( const std::string& filename )
{
   std::ifstream file( filename, std::ifstream::in );
   boost::iostreams::filtering_istream in;

   if( !file )
      return false;
#ifdef BUGGER_USE_BOOST_IOSTREAMS_WITH_ZLIB
   if( boost::algorithm::ends_with( filename, ".gz" ) )
      in.push( boost::iostreams::gzip_decompressor() );
#endif
#ifdef BUGGER_USE_BOOST_IOSTREAMS_WITH_BZIP2
   if( boost::algorithm::ends_with( filename, ".bz2" ) )
      in.push( boost::iostreams::bzip2_decompressor() );
#endif
   in.push( file );

   return parse( in );
}

template <typename REAL>
bool
MpsParser<REAL>::parse( boost::iostreams::filtering_istream& file )
{
   nnz = 0;
   parsekey keyword = parsekey::kNone;
   parsekey keyword_old = parsekey::kNone;

   // parsing loop
   while( keyword != parsekey::kFail && keyword != parsekey::kEnd &&
          !file.eof() && file.good() )
   {
      keyword_old = keyword;
      switch( keyword )
      {
      case parsekey::kObjsense:
         keyword = parseObjsense( file );
         break;
      case parsekey::kRows:
         keyword = parseRows( file, row_type );
         break;
      case parsekey::kCols:
         keyword = parseCols( file, row_type );
         break;
      case parsekey::kRhs:
         keyword = parseRhs( file );
         break;
      case parsekey::kRanges:
         keyword = parseRanges( file );
         break;
      case parsekey::kBounds:
         keyword = parseBounds( file );
         break;
      case parsekey::kFail:
         break;
      default:
         keyword = parseDefault( file );
         break;
      }
   }

   if( keyword == parsekey::kFail || keyword != parsekey::kEnd )
   {
      printErrorMessage( keyword_old );
      return false;
   }

   assert( row_type.size() == unsigned( nRows ) );

   nCols = colname2idx.size();
   nRows = rowname2idx.size() - 1; // subtract obj row

   return true;
}

} // namespace bugger

#endif
