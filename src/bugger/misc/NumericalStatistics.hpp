/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*                            MIP-DD                                         */
/*                                                                           */
/* Copyright (C) 2020-2022  Konrad-Zuse-Zentrum                              */
/*                     fuer Informationstechnik Berlin                       */
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

#ifndef _BUGGER_MISC_NUMERICALSTATISTICS_HPP_
#define _BUGGER_MISC_NUMERICALSTATISTICS_HPP_

#include "bugger/data/ConstraintMatrix.hpp"
#include "bugger/data/Problem.hpp"
#include "bugger/misc/fmt.hpp"
#include <cmath>

namespace bugger
{

template <typename REAL>
struct Num_stats
{
   REAL matrixMin;
   REAL matrixMax;
   REAL objMin;
   REAL objMax;
   REAL boundsMin;
   REAL boundsMax;
   REAL rhsMin;
   REAL rhsMax;
   REAL dynamism;
   REAL rowDynamism;
   REAL colDynamism;
};

template <typename REAL>
class NumericalStatistics
{
 public:
   NumericalStatistics( const Problem<REAL>& p )
       : stats( Num_stats<REAL>() ), prob( p )
   {
      // Set all values in Num_stats

      const ConstraintMatrix<REAL>& cm = prob.getConstraintMatrix();
      const VariableDomains<REAL>& vd = prob.getVariableDomains();
      const Vec<RowFlags>& rf = cm.getRowFlags();
      const Vec<REAL>& lhs = cm.getLeftHandSides();
      const Vec<REAL>& rhs = cm.getRightHandSides();

      int nrows = cm.getNRows();
      int ncols = cm.getNCols();

      stats.matrixMin = 0.0;
      stats.matrixMax = 0.0;
      stats.rowDynamism = 0.0;
      stats.rhsMin = 0.0;
      stats.rhsMax = 0.0;
      bool rhsMinSet = false;

      // Row dynamism, matrixMin/Max, RHS
      for( int r = 0; r < nrows; ++r )
      {
         // matrixMin/Max
         const SparseVectorView<REAL>& row = cm.getRowCoefficients( r );std::pair<REAL, REAL> minmax = row.getMinMaxAbsValue();

         stats.matrixMax = max( minmax.second, stats.matrixMax );
         if( r == 0 )
            stats.matrixMin = stats.matrixMax;
         else
            stats.matrixMin = min( minmax.first, stats.matrixMin );

         // Row dynamism
         if(minmax.first != 0)
         {
            REAL dyn = minmax.second / minmax.first;
            stats.rowDynamism = max( dyn, stats.rowDynamism );
         }

         // RHS min/max

         // Handle case where RHS/LHS is inf
         if( !rhsMinSet )
         {
            rhsMinSet = true;
            if( !rf[r].test( RowFlag::kLhsInf ) &&
                !rf[r].test( RowFlag::kRhsInf ) && lhs[r] != 0 && rhs[r] != 0 )
               stats.rhsMin = min( abs( lhs[r] ), abs( rhs[r] ) );
            else if( !rf[r].test( RowFlag::kLhsInf ) && lhs[r] != 0 )
               stats.rhsMin = abs( lhs[r] );
            else if( !rf[r].test( RowFlag::kRhsInf ) && rhs[r] != 0 )
               stats.rhsMin = abs( rhs[r] );
            else
               rhsMinSet = false;
         }
         else
         {
            if( !rf[r].test( RowFlag::kLhsInf ) &&
                !rf[r].test( RowFlag::kRhsInf ) && lhs[r] != 0 && rhs[r] != 0 )
               stats.rhsMin = min( stats.rhsMin, min( abs( lhs[r] ), abs( rhs[r] ) ) );
            else if( !rf[r].test( RowFlag::kLhsInf ) && lhs[r] != 0 )
               stats.rhsMin = min( stats.rhsMin, abs( lhs[r] ) );
            else if( !rf[r].test( RowFlag::kRhsInf ) && rhs[r] != 0 )
               stats.rhsMin = min( stats.rhsMin, abs( rhs[r] ) );
         }

         if( !rf[r].test( RowFlag::kLhsInf ) &&
             !rf[r].test( RowFlag::kRhsInf ) )
            stats.rhsMax = max( stats.rhsMax, max( abs( lhs[r] ), abs( rhs[r] ) ) );
         else if( !rf[r].test( RowFlag::kLhsInf ) )
            stats.rhsMax = max( stats.rhsMax, abs( lhs[r] ) );
         else if( !rf[r].test( RowFlag::kRhsInf ) )
            stats.rhsMax = max( stats.rhsMax, abs( rhs[r] ) );
      }

      stats.colDynamism = 0.0;
      stats.boundsMin = 0.0;
      stats.boundsMax = 0.0;
      bool boundsMinSet = false;

      // Column dynamism, Variable Bounds
      for( int c = 0; c < ncols; ++c )
      {
         // Column dynamism
         const SparseVectorView<REAL>& col = cm.getColumnCoefficients( c );
         std::pair<REAL, REAL> minmax = col.getMinMaxAbsValue();

         REAL dyn = minmax.first == 0 ? (REAL) 0 : minmax.second / minmax.first;
         stats.colDynamism = max( dyn, stats.colDynamism );

         // Bounds

         // Handle case where first variables are unbounded
         if( !boundsMinSet )
         {
            boundsMinSet = true;
            if( !vd.flags[c].test( ColFlag::kLbInf ) &&
                !vd.flags[c].test( ColFlag::kUbInf ) &&
                vd.lower_bounds[c] != 0 && vd.upper_bounds[c] != 0 )
               stats.boundsMin = min( abs( vd.lower_bounds[c] ), abs( vd.upper_bounds[c] ) );
            else if( !vd.flags[c].test( ColFlag::kLbInf ) &&
                     vd.lower_bounds[c] != 0 )
               stats.boundsMin = abs( vd.lower_bounds[c] );
            else if( !vd.flags[c].test( ColFlag::kUbInf ) &&
                     vd.upper_bounds[c] != 0 )
               stats.boundsMin = abs( vd.upper_bounds[c] );
            else
               boundsMinSet = false;
         }
         else
         {
            if( !vd.flags[c].test( ColFlag::kLbInf ) &&
                !vd.flags[c].test( ColFlag::kUbInf ) &&
                vd.lower_bounds[c] != 0 && vd.upper_bounds[c] != 0 )
               stats.boundsMin = min( stats.boundsMin, min( abs( vd.lower_bounds[c] ), abs( vd.upper_bounds[c] ) ) );
            else if( !vd.flags[c].test( ColFlag::kLbInf ) &&
                     vd.lower_bounds[c] != 0 )
               stats.boundsMin = min( stats.boundsMin, abs( vd.lower_bounds[c] ) );
            else if( !vd.flags[c].test( ColFlag::kUbInf ) &&
                     vd.upper_bounds[c] != 0 )
               stats.boundsMin = min( stats.boundsMin, abs( vd.upper_bounds[c] ) );
         }

         if( !vd.flags[c].test( ColFlag::kLbInf ) &&
             !vd.flags[c].test( ColFlag::kUbInf ) )
            stats.boundsMax = max( stats.boundsMax, max( abs( vd.lower_bounds[c] ), abs( vd.upper_bounds[c] ) ) );
         else if( !vd.flags[c].test( ColFlag::kLbInf ) )
            stats.boundsMax = max( stats.boundsMax, abs( vd.lower_bounds[c] ) );
         else if( !vd.flags[c].test( ColFlag::kUbInf ) )
            stats.boundsMax = max( stats.boundsMax, abs( vd.upper_bounds[c] ) );
      }

      if(stats.matrixMin != 0)
         stats.dynamism = stats.matrixMax / stats.matrixMin;

      // Objective
      const Objective<REAL>& obj = prob.getObjective();

      stats.objMax = 0.0;
      stats.objMin = 0.0;
      bool objMinSet = false;

      for( int i = 0; i < (int) obj.coefficients.size(); ++i )
      {
         if( obj.coefficients[i] != 0 )
         {
            stats.objMax = max( stats.objMax, abs( obj.coefficients[i] ) );
            if( !objMinSet )
            {
               stats.objMin = abs( obj.coefficients[i] );
               objMinSet = true;
            }
            else
               stats.objMin = min( stats.objMin, abs( obj.coefficients[i] ) );
         }
      }
   }

   void
   printStatistics()
   {
      fmt::print( "Numerical Statistics:\n Matrix range    [{:.0e},{:.0e}]\n "
                  "Objective range [{:.0e},{:.0e}]\n Bounds range    "
                  "[{:.0e},{:.0e}]\n RHS range       [{:.0e},{:.0e}]\n "
                  "Dynamism Variables: {:.0e}\n Dynamism Rows     : {:.0e}\n",
                  double( stats.matrixMin ), double( stats.matrixMax ),
                  double( stats.objMin ), double( stats.objMax ),
                  double( stats.boundsMin ), double( stats.boundsMax ),
                  double( stats.rhsMin ), double( stats.rhsMax ),
                  double( stats.colDynamism ), double( stats.rowDynamism ) );
   }

   const Num_stats<REAL>&
   getNum_stats()
   {
      return stats;
   }

 private:
   Num_stats<REAL> stats;
   const Problem<REAL>& prob;
};

} // namespace bugger

#endif
