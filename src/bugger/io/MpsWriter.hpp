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

#ifndef _BUGGER_IO_MPS_WRITER_HPP_
#define _BUGGER_IO_MPS_WRITER_HPP_

#include "bugger/data/Problem.hpp"

#ifdef BUGGER_USE_BOOST_IOSTREAMS_WITH_ZLIB
#include <boost/iostreams/filter/gzip.hpp>
#endif
#ifdef BUGGER_USE_BOOST_IOSTREAMS_WITH_BZIP2
#include <boost/iostreams/filter/bzip2.hpp>
#endif


namespace bugger
{
/// Writer to write problem structures into an mps file
template <typename REAL>
struct MpsWriter
{
   static void
   writeProb( const String& filename, const Problem<REAL>& prob )
   {
      const ConstraintMatrix<REAL>& consmatrix = prob.getConstraintMatrix();
      const Vec<String>& consnames = prob.getConstraintNames();
      const Vec<String>& varnames = prob.getVariableNames();
      const Vec<REAL>& lhs = consmatrix.getLeftHandSides();
      const Vec<REAL>& rhs = consmatrix.getRightHandSides();
      const Objective<REAL>& obj = prob.getObjective();
      const Vec<ColFlags>& col_flags = prob.getColFlags();
      const Vec<RowFlags>& row_flags = prob.getRowFlags();

      std::ofstream file( filename, std::ofstream::out );
      boost::iostreams::filtering_ostream out;
#ifdef BUGGER_USE_BOOST_IOSTREAMS_WITH_ZLIB
      if( boost::algorithm::ends_with( filename, ".gz" ) )
         out.push( boost::iostreams::gzip_compressor() );
#endif
#ifdef BUGGER_USE_BOOST_IOSTREAMS_WITH_BZIP2
      if( boost::algorithm::ends_with( filename, ".bz2" ) )
         out.push( boost::iostreams::bzip2_compressor() );
#endif
      out.push( file );

      int nrows = 0;
      int ncols = 0;
      int nintcols = 0;
      int nnnz = 0;
      for( int i = 0; i < consmatrix.getNRows(); ++i )
      {
         if( !row_flags[i].test(RowFlag::kRedundant) )
         {
            ++nrows;
            auto data = consmatrix.getRowCoefficients(i);
            for( int j = 0; j < data.getLength(); ++j )
            {
               if( data.getValues()[j] != 0 )
               {
                  assert(!col_flags[data.getIndices()[j]].test(ColFlag::kFixed));
                  ++nnnz;
               }
            }
         }
      }
      for( int i = 0; i < consmatrix.getNCols(); ++i )
      {
         if( !col_flags[i].test(ColFlag::kFixed) )
         {
            ++ncols;
            if( col_flags[i].test(ColFlag::kIntegral) )
               ++nintcols;
         }
      }

      fmt::print( out, "*Instance {} reduced by delta debugging\n", prob.getName());
      fmt::print( out, "*\tConstraints:         {} of original {}\n", nrows, consmatrix.getNRows() );
      fmt::print( out, "*\tVariables:           {} of original {}\n", ncols, consmatrix.getNCols() );
      fmt::print( out, "*\tInteger:             {} of original {}\n", nintcols, prob.getNumIntegralCols());
      fmt::print( out, "*\tNonzeros:            {} of original {}\n", nnnz, consmatrix.getNnz() );

      fmt::print( out, "*\n*\n");

      fmt::print( out, "NAME          {}\n", prob.getName() );
      fmt::print( out, "OBJSENSE\n" );
      fmt::print( out, obj.sense ? " MIN\n" : " MAX\n" );
      fmt::print( out, "ROWS\n" );
      fmt::print( out, " N  OBJ\n" );
      bool hasRangedRow = false;
      for( int i = 0; i < consmatrix.getNRows(); ++i )
      {
         //TODO this was original an assert so the nnz ana the ncols might not be correct anymore
         if( consmatrix.isRowRedundant( i ) )
            continue;
         char type;

         if( row_flags[i].test( RowFlag::kLhsInf ) &&
             row_flags[i].test( RowFlag::kRhsInf ) )
            type = 'N';
         else if( row_flags[i].test( RowFlag::kRhsInf ) )
            type = 'G';
         else if( row_flags[i].test( RowFlag::kLhsInf ) )
            type = 'L';
         else
         {
            if( !row_flags[i].test( RowFlag::kEquation ) )
               hasRangedRow = true;
            type = 'E';
         }

         fmt::print( out, " {}  {}\n", type, consnames[i] );
      }

      fmt::print( out, "COLUMNS\n" );

      int hasintegral = prob.getNumIntegralCols() != 0;

      for( int integral = 0; integral <= hasintegral; ++integral )
      {
         if( integral )
            fmt::print( out,
                        "    MARK0000  'MARKER'                 'INTORG'\n" );

         for( int i = 0; i < consmatrix.getNCols(); ++i )
         {
            if( col_flags[i].test( ColFlag::kInactive ) )
               continue;

            if( ( !col_flags[i].test( ColFlag::kIntegral ) && integral ) ||
                ( col_flags[i].test( ColFlag::kIntegral ) && !integral ) )
               continue;

            assert(
                !col_flags[i].test( ColFlag::kFixed, ColFlag::kSubstituted ) );

            if( obj.coefficients[i] != 0 )
               fmt::print( out, "    {:<9} OBJ       {:}\n", varnames[i], obj.coefficients[i] );

            SparseVectorView<REAL> column =
                consmatrix.getColumnCoefficients( i );

            const int* rowinds = column.getIndices();
            const REAL* colvals = column.getValues();
            int len = column.getLength();

            for( int j = 0; j < len; ++j )
            {
               int r = rowinds[j];

               // discard redundant rows when writing problem
               if( consmatrix.isRowRedundant( r ) )
                  continue;

               // normal row
               fmt::print( out, "    {:<9} {:<9} {:}\n", varnames[i], consnames[r], colvals[j] );
            }
         }

         if( integral )
            fmt::print( out,
                        "    MARK0000  'MARKER'                 'INTEND'\n" );
      }

      const Vec<REAL>& lower_bounds = prob.getLowerBounds();
      const Vec<REAL>& upper_bounds = prob.getUpperBounds();

      fmt::print( out, "RHS\n" );

      if( obj.offset != 0 )
      {
         if( obj.offset != 0 )
            fmt::print( out, "    B         {:<9} {:}\n", "OBJ", -obj.offset );
      }

      for( int i = 0; i < consmatrix.getNRows(); ++i )
      {
         // discard redundant rows when writing problem
         if( consmatrix.isRowRedundant( i ) )
            continue;

         if( row_flags[i].test( RowFlag::kLhsInf ) &&
             row_flags[i].test( RowFlag::kRhsInf ) )
            continue;

         if( row_flags[i].test( RowFlag::kLhsInf ) )
         {
            if( rhs[i] != 0 )
               fmt::print( out, "    B         {:<9} {:}\n", consnames[i], rhs[i] );
         }
         else
         {
            if( lhs[i] != 0 )
               fmt::print( out, "    B         {:<9} {:}\n", consnames[i], lhs[i] );
         }
      }

      if( hasRangedRow )
      {
         fmt::print( out, "RANGES\n" );
         for( int i = 0; i < consmatrix.getNRows(); ++i )
         {
            if( row_flags[i].test( RowFlag::kLhsInf, RowFlag::kRhsInf,
                                   RowFlag::kEquation, RowFlag::kRedundant ) )
               continue;

            REAL rangeval = rhs[i] - lhs[i];

            if( rangeval != 0 )
               fmt::print( out, "    B         {:<9} {:}\n", consnames[i], rangeval );
         }
      }

      fmt::print( out, "BOUNDS\n" );

      for( int i = 0; i < consmatrix.getNCols(); ++i )
      {
         if( col_flags[i].test( ColFlag::kInactive ) )
            continue;

         if( !col_flags[i].test( ColFlag::kLbInf ) &&
             !col_flags[i].test( ColFlag::kUbInf ) &&
             lower_bounds[i] == upper_bounds[i] )
            fmt::print( out, " FX BND       {:<9} {:}\n", varnames[i], lower_bounds[i] );
         else
         {
            if( col_flags[i].test( ColFlag::kLbInf ) || lower_bounds[i] != 0 )
            {
               if( col_flags[i].test( ColFlag::kLbInf ) )
                  fmt::print( out, " MI BND       {:}\n", varnames[i] );
               else
                  fmt::print( out, " LO BND       {:<9} {:}\n", varnames[i], lower_bounds[i] );
            }

            if( !col_flags[i].test( ColFlag::kUbInf ) )
               fmt::print( out, " UP BND       {:<9} {:}\n", varnames[i], upper_bounds[i] );
            else
               fmt::print( out, " PL BND       {:}\n", varnames[i] );
         }
      }
      fmt::print( out, "ENDATA\n" );
   }
};

} // namespace bugger

#endif
