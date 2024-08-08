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

#ifndef _BUGGER_CORE_PROBLEM_HPP_
#define _BUGGER_CORE_PROBLEM_HPP_

#include "bugger/data/ConstraintMatrix.hpp"
#include "bugger/data/Objective.hpp"
#include "bugger/data/SingleRow.hpp"
#include "bugger/data/VariableDomains.hpp"
#include "bugger/data/Solution.hpp"
#include "bugger/io/Message.hpp"
#include "bugger/misc/MultiPrecision.hpp"
#include "bugger/misc/StableSum.hpp"
#include "bugger/misc/String.hpp"
#include "bugger/misc/Vec.hpp"
#include "bugger/misc/fmt.hpp"
#ifdef BUGGER_TBB
#include "bugger/misc/tbb.hpp"
#endif


namespace bugger
{
/// struct to hold counters for up an downlocks of a column
struct Locks
{
   int up;
   int down;

   template <typename Archive>
   void
   serialize( Archive& ar, const unsigned int version )
   {
      ar& up;
      ar& down;
   }
};

/// class representing the problem consisting of the constraint matrix, the left
/// and right hand side values, the variable domains, the column bounds,
/// column integrality restrictions, and the objective function
template <typename REAL>
class Problem
{
 public:
   /// set objective function
   void
   setObjective( Vec<REAL> coefficients, REAL offset = 0.0, bool minimize = true )
   {
      objective = Objective<REAL>{ std::move( coefficients ), offset, minimize };
   }

   /// set objective function
   void
   setObjective( Objective<REAL>&& obj )
   {
      objective = obj;
   }

   /// set (transposed) constraint matrix
   void
   setConstraintMatrix( SparseStorage<REAL> cons_matrix, Vec<REAL> lhs_values,
                        Vec<REAL> rhs_values, Vec<RowFlags> row_flags,
                        bool transposed = false )
   {
      assert( lhs_values.size() == rhs_values.size() );
      assert( lhs_values.size() == row_flags.size() );
      assert( ( transposed ? cons_matrix.getNCols()
                           : cons_matrix.getNRows() ) == row_flags.size() );

      auto cons_matrix_other = cons_matrix.getTranspose();
      if( transposed )
         constraintMatrix = ConstraintMatrix<REAL>{
             std::move( cons_matrix_other ), std::move( cons_matrix ),
             std::move( lhs_values ), std::move( rhs_values ),
             std::move( row_flags ) };
      else
         constraintMatrix = ConstraintMatrix<REAL>{
             std::move( cons_matrix ), std::move( cons_matrix_other ),
             std::move( lhs_values ), std::move( rhs_values ),
             std::move( row_flags ) };
   }

   /// set constraint matrix
   void
   setConstraintMatrix( ConstraintMatrix<REAL>&& cons_matrix )
   {
      constraintMatrix = cons_matrix;
   }

   /// set domains of variables
   void
   setVariableDomains( VariableDomains<REAL>&& domains )
   {
      variableDomains = domains;

      nintegers = 0;
      ncontinuous = 0;

      for( ColFlags cf : variableDomains.flags )
      {
         if( cf.test( ColFlag::kIntegral ) )
            ++nintegers;
         else
            ++ncontinuous;
      }
   }

   /// set domains of variables
   void
   setVariableDomains( Vec<REAL> lower_bounds, Vec<REAL> upper_bounds,
                       Vec<ColFlags> col_flags )
   {
      variableDomains = VariableDomains<REAL>{ std::move( lower_bounds ),
                                               std::move( upper_bounds ),
                                               std::move( col_flags ) };
      nintegers = 0;
      ncontinuous = 0;

      for( ColFlags cf : variableDomains.flags )
      {
         if( cf.test( ColFlag::kIntegral ) )
            ++nintegers;
         else
            ++ncontinuous;
      }
   }

   /// set constraint types
   void
   setConstraintTypes( Vec<char> cons_types )
   {
      constraintTypes = std::move( cons_types );
   }

   /// set problem name
   void
   setName( String name_ )
   {
      this->name = std::move( name_ );
   }

   /// set variable names
   void
   setVariableNames( Vec<String> var_names )
   {
      variableNames = std::move( var_names );
   }

   /// set constraint names
   void
   setConstraintNames( Vec<String> cons_names )
   {
      constraintNames = std::move( cons_names );
   }

   /// returns number of active integral columns
   int
   getNumIntegralCols() const
   {
      return nintegers;
   }

   /// returns number of active integral columns
   int&
   getNumIntegralCols()
   {
      return nintegers;
   }

   /// returns number of active continuous columns
   int
   getNumContinuousCols() const
   {
      return ncontinuous;
   }

   /// returns number of active continuous columns
   int&
   getNumContinuousCols()
   {
      return ncontinuous;
   }

   /// get the problem matrix
   const ConstraintMatrix<REAL>&
   getConstraintMatrix() const
   {
      return constraintMatrix;
   }

   /// get the problem matrix
   ConstraintMatrix<REAL>&
   getConstraintMatrix()
   {
      return constraintMatrix;
   }

   /// get number of columns
   int
   getNCols() const
   {
      return constraintMatrix.getNCols();
   }

   /// get number of rows
   int
   getNRows() const
   {
      return constraintMatrix.getNRows();
   }

   /// get the objective function
   const Objective<REAL>&
   getObjective() const
   {
      return objective;
   }

   /// get the objective function
   Objective<REAL>&
   getObjective()
   {
      return objective;
   }

   /// get the variable domains
   const VariableDomains<REAL>&
   getVariableDomains() const
   {
      return variableDomains;
   }

   /// get the variable domains
   VariableDomains<REAL>&
   getVariableDomains()
   {
      return variableDomains;
   }

   const Vec<ColFlags>&
   getColFlags() const
   {
      return variableDomains.flags;
   }

   Vec<ColFlags>&
   getColFlags()
   {
      return variableDomains.flags;
   }

   const Vec<RowFlags>&
   getRowFlags() const
   {
      return constraintMatrix.getRowFlags();
   }

   Vec<RowFlags>&
   getRowFlags()
   {
      return constraintMatrix.getRowFlags();
   }

   /// get the (dense) vector of variable lower bounds
   const Vec<REAL>&
   getLowerBounds() const
   {
      return variableDomains.lower_bounds;
   }

   /// get the (dense) vector of variable lower bounds
   Vec<REAL>&
   getLowerBounds()
   {
      return variableDomains.lower_bounds;
   }

   /// get the (dense) vector of variable upper bounds
   const Vec<REAL>&
   getUpperBounds() const
   {
      return variableDomains.upper_bounds;
   }

   /// get the (dense) vector of variable upper bounds
   Vec<REAL>&
   getUpperBounds()
   {
      return variableDomains.upper_bounds;
   }

   /// get the (dense) vector of column sizes
   const Vec<int>&
   getColSizes() const
   {
      return constraintMatrix.getColSizes();
   }

   /// get the (dense) vector of column sizes
   Vec<int>&
   getColSizes()
   {
      return constraintMatrix.getColSizes();
   }

   /// get the (dense) vector of row sizes
   const Vec<int>&
   getRowSizes() const
   {
      return constraintMatrix.getRowSizes();
   }

   /// get the (dense) vector of row sizes
   Vec<int>&
   getRowSizes()
   {
      return constraintMatrix.getRowSizes();
   }

   /// return const reference to vector of row activities
   const Vec<RowActivity<REAL>>&
   getRowActivities() const
   {
      return rowActivities;
   }

   /// return reference to vector of row activities
   Vec<RowActivity<REAL>>&
   getRowActivities()
   {
      return rowActivities;
   }

   /// returns a reference to the vector of locks of each column, the locks
   /// include the objective cutoff constraint
   Vec<Locks>&
   getLocks()
   {
      return locks;
   }

   /// get the constraint types
   const Vec<char>&
   getConstraintTypes() const
   {
      return constraintTypes;
   }

   /// get the problem name
   const String&
   getName() const
   {
      return name;
   }

   /// get the variable names
   const Vec<String>&
   getVariableNames() const
   {
      return variableNames;
   }

   /// get the constraint names
   const Vec<String>&
   getConstraintNames() const
   {
      return constraintNames;
   }

   /// get primal objective value for given solution
   REAL
   getPrimalObjective(const Solution<REAL>& solution) const
   {
      assert( solution.status == SolutionStatus::kFeasible || solution.status == SolutionStatus::kUnbounded );
      assert( solution.primal.size() == getNCols() );
      const auto& data = getObjective().coefficients;
      StableSum<REAL> sum { getObjective().offset };
      for( int i = 0; i < getNCols(); ++i )
         if( !getColFlags()[i].test( ColFlag::kFixed ) )
            sum.add(data[i] * solution.primal[i]);
      return sum.get( );
   }

   /// get ray objective value for given solution
   REAL
   getRayObjective(const Solution<REAL>& solution) const
   {
      assert( solution.status == SolutionStatus::kUnbounded );
      assert( solution.ray.size() == getNCols() );
      const auto& data = getObjective().coefficients;
      StableSum<REAL> sum;
      for( int i = 0; i < getNCols(); ++i )
         if( !getColFlags()[i].test( ColFlag::kFixed ) )
            sum.add(data[i] * solution.ray[i]);
      return sum.get( );
   }

   /// get primal activity value for given solution and index of optionally rounded row
   REAL
   getPrimalActivity(const Solution<REAL>& solution, int row, bool roundrow = false) const
   {
      assert( solution.status == SolutionStatus::kFeasible || solution.status == SolutionStatus::kUnbounded );
      assert( solution.primal.size() == getNCols() );
      const auto& data = getConstraintMatrix().getRowCoefficients(row);
      if( getConstraintTypes( )[ row ] == 'a' )
      {
         StableSum<REAL> sum;
         REAL minvalue { 1 };
         REAL resvalue { -1 };
         int resindex = -1;
         for( int i = 0; i < data.getLength( ); ++i )
         {
            REAL value { solution.primal[data.getIndices( )[i]] };
            if( data.getValues( )[i] < 0 )
               value = 1 - value;
            if( resindex < 0 && abs(data.getValues( )[i]) > 1 )
            {
               resvalue = value;
               resindex = i;
               sum.add(1 - value);
            }
            else
            {
               if( minvalue > value )
                  minvalue = value;
               sum.add(value);
            }
         }
         assert( resindex >= 0 );
         resvalue = resvalue - minvalue;
         minvalue = sum.get() - (data.getLength( ) - 1);
         if( minvalue > resvalue )
            return minvalue > 0 ? -minvalue : 0;
         else
            return resvalue > 0 ? resvalue : 0;
      }
      else
      {
         StableSum<REAL> sum;
         for( int i = 0; i < data.getLength( ); ++i )
            sum.add((roundrow ? round(data.getValues( )[i]) : data.getValues( )[i]) * solution.primal[data.getIndices( )[i]]);
         return sum.get( );
      }
   }

   /// get ray activity value for given solution and index of optionally rounded row
   REAL
   getRayActivity(const Solution<REAL>& solution, int row, bool roundrow = false) const
   {
      assert( solution.status == SolutionStatus::kUnbounded );
      assert( solution.ray.size() == getNCols() );
      const auto& data = getConstraintMatrix().getRowCoefficients(row);
      if( getConstraintTypes( )[ row ] == 'a' )
      {
         StableSum<REAL> sum;
         REAL minvalue { std::numeric_limits<REAL>::max() };
         REAL resvalue { std::numeric_limits<REAL>::min() };
         int resindex = -1;
         for( int i = 0; i < data.getLength( ); ++i )
         {
            REAL value { solution.ray[data.getIndices( )[i]] };
            if( data.getValues( )[i] < 0 )
               value *= -1;
            if( resindex < 0 && abs(data.getValues( )[i]) > 1 )
            {
               resvalue = value;
               resindex = i;
               sum.add(-value);
            }
            else
            {
               if( minvalue > value )
                  minvalue = value;
               sum.add(value);
            }
         }
         assert( resindex >= 0 );
         resvalue = resvalue - minvalue;
         minvalue = sum.get();
         if( minvalue > resvalue )
            return minvalue > 0 ? -minvalue : 0;
         else
            return resvalue > 0 ? resvalue : 0;
      }
      else
      {
         StableSum<REAL> sum;
         for( int i = 0; i < data.getLength( ); ++i )
            sum.add((roundrow ? round(data.getValues( )[i]) : data.getValues( )[i]) * solution.ray[data.getIndices( )[i]]);
         return sum.get( );
      }
   }

   /// print feasibility for given solution and return whether it is tolerable
   bool
   checkFeasibility(const Solution<REAL>& solution, const Num<REAL>& num, const Message& msg) const
   {
      msg.info("\nCheck:\n");
      if( solution.status == SolutionStatus::kUnknown )
      {
         msg.info("Unknown.\n");
         return true;
      }
      else if( solution.status == SolutionStatus::kInfeasible )
      {
         msg.info("Infeasible.\n");
         return true;
      }
      else if( solution.status == SolutionStatus::kUnbounded )
      {
         msg.info("Unbounded.\n");
         return true;
      }
      assert( solution.status == SolutionStatus::kFeasible );
      assert( solution.primal.size() == getNCols() );
      const auto& lb = getLowerBounds();
      const auto& ub = getUpperBounds();
      REAL viol;
      REAL maxviol { };
      int maxindex = -1;
      bool maxrow = false;
      bool maxupper = false;
      bool maxintegral = false;
      for( int col = 0; col < getNCols(); col++ )
      {
         if( getColFlags()[col].test( ColFlag::kInactive ) )
            continue;
         if( !getColFlags()[col].test( ColFlag::kLbInf ) && solution.primal[col] < lb[col] )
         {
            msg.detailed( "\tColumn {:<3} violates lower bound ({:<3} < {:<3})\n", getVariableNames()[col], solution.primal[col], lb[col] );
            viol = lb[col] - solution.primal[col];
            if( viol > maxviol )
            {
               maxviol = viol;
               maxindex = col;
               maxrow = false;
               maxupper = false;
               maxintegral = false;
            }
         }
         if( !getColFlags()[col].test( ColFlag::kUbInf ) && solution.primal[col] > ub[col] )
         {
            msg.detailed( "\tColumn {:<3} violates upper bound ({:<3} > {:<3})\n", getVariableNames()[col], solution.primal[col], ub[col] );
            viol = solution.primal[col] - ub[col];
            if( viol > maxviol )
            {
               maxviol = viol;
               maxindex = col;
               maxrow = false;
               maxupper = true;
               maxintegral = false;
            }
         }
         if( getColFlags()[col].test( ColFlag::kIntegral ) && solution.primal[col] != round(solution.primal[col]) )
         {
            msg.detailed( "\tColumn {:<3} violates integrality property ({:<3} != {:<3})\n", getVariableNames()[col], solution.primal[col], round(solution.primal[col]) );
            viol = abs(solution.primal[col] - round(solution.primal[col]));
            if( viol > maxviol )
            {
               maxviol = viol;
               maxindex = col;
               maxrow = false;
               maxupper = false;
               maxintegral = true;
            }
         }
      }
      const auto& lhs = getConstraintMatrix().getLeftHandSides();
      const auto& rhs = getConstraintMatrix().getRightHandSides();
      for( int row = 0; row < getNRows(); row++ )
      {
         if( getRowFlags()[row].test( RowFlag::kRedundant ) )
            continue;
         REAL activity { getPrimalActivity(solution, row) };
         if( !getRowFlags()[row].test( RowFlag::kLhsInf ) && activity < lhs[row] )
         {
            msg.detailed( "\tRow {:<3} violates left side ({:<3} < {:<3})\n", getConstraintNames()[row], activity, lhs[row] );
            viol = lhs[row] - activity;
            if( viol > maxviol )
            {
               maxviol = viol;
               maxindex = row;
               maxrow = true;
               maxupper = false;
               maxintegral = false;
            }
         }
         if( !getRowFlags()[row].test( RowFlag::kRhsInf ) && activity > rhs[row] )
         {
            msg.detailed( "\tRow {:<3} violates right side ({:<3} > {:<3})\n", getConstraintNames()[row], activity, rhs[row] );
            viol = activity - rhs[row];
            if( viol > maxviol )
            {
               maxviol = viol;
               maxindex = row;
               maxrow = true;
               maxupper = true;
               maxintegral = false;
            }
         }
      }
      bool infeasible = num.isEpsGT(maxviol, 0);
      msg.info("Solution is {}.\n", infeasible ? "infeasible" : num.isZetaGT(maxviol, 0) ? "tolerable" : "feasible");
      if( maxindex >= 0 )
         msg.info("Maximum violation {:<3} of {} {:<3} {}.\n", maxviol, maxrow ? "row" : "column", (maxrow ? getConstraintNames() : getVariableNames())[maxindex], maxintegral ? "integral" : (maxrow ? (maxupper ? "right" : "left") : (maxupper ? "upper" : "lower")));
      else
         msg.info("No violations detected.\n");
      msg.info("\n");
      return !infeasible;
   }

   std::pair<Vec<int>, Vec<int>>
   compress( bool full = false )
   {
      std::pair<Vec<int>, Vec<int>> mappings =
          constraintMatrix.compress( full );

      // update information about columns that is stored by index
#ifdef BUGGER_TBB
      tbb::parallel_invoke(
          [this, &mappings, full]() {
             compress_vector( mappings.second, objective.coefficients );
             if( full )
                objective.coefficients.shrink_to_fit();
          },
          [this, &mappings, full]() {
             variableDomains.compress( mappings.second, full );
          },
          [this, &mappings, full]() {
             // compress row activities
             // recomputeAllActivities();
             if( rowActivities.size() != 0 )
                compress_vector( mappings.first, rowActivities );

             if( full )
                rowActivities.shrink_to_fit();
          } );
#else
      compress_vector( mappings.second, objective.coefficients );
      variableDomains.compress( mappings.second, full );
      if( rowActivities.size() != 0 )
         compress_vector( mappings.first, rowActivities );
      if( full )
      {
         objective.coefficients.shrink_to_fit();
         rowActivities.shrink_to_fit();
      }
#endif

      // compress row activities
      return mappings;
   }

   /// sets the tolerance of the input format
   void
   setInputTolerance( REAL inputTolerance_ )
   {
      this->inputTolerance = std::move( inputTolerance_ );
   }

   void
   recomputeAllActivities()
   {
      rowActivities.resize( getNRows() );

      // loop through rows once, compute initial acitvities, detect trivial
      // redundancy
#ifdef BUGGER_TBB
      tbb::parallel_for(
          tbb::blocked_range<int>( 0, getNRows() ),
          [this]( const tbb::blocked_range<int>& r ) {
             for( int row = r.begin(); row < r.end(); ++row )
#else
      for( int row = 0; row < getNRows(); ++row )
#endif
             {
                auto rowvec = constraintMatrix.getRowCoefficients( row );
                rowActivities[row] = compute_row_activity(
                    rowvec.getValues(), rowvec.getIndices(), rowvec.getLength(),
                    variableDomains.lower_bounds, variableDomains.upper_bounds,
                    variableDomains.flags );
             }
#ifdef BUGGER_TBB
          } );
#endif
   }

   void
   recomputeLocks()
   {
      locks.resize( getNCols() );

      // loop through rows once, compute initial activities, detect trivial
      // redundancy
#ifdef BUGGER_TBB
      tbb::parallel_for(
          tbb::blocked_range<int>( 0, getNCols() ),
          [this]( const tbb::blocked_range<int>& c ) {
             for( int col = c.begin(); col != c.end(); ++col )
#else
      for( int col = 0; col < getNCols(); ++col )
#endif
             {
                auto colvec = constraintMatrix.getColumnCoefficients( col );

                const REAL* vals = colvec.getValues();
                const int* inds = colvec.getIndices();
                int len = colvec.getLength();
                const auto& rflags = getRowFlags();

                for( int i = 0; i != len; ++i )
                   count_locks( vals[i], rflags[inds[i]], locks[col].down,
                                locks[col].up );
             }
#ifdef BUGGER_TBB
          } );
#endif
   }

   std::pair<int, int>
   removeRedundantBounds( const Num<REAL>& num, Vec<ColFlags>& cflags,
                          Vec<RowActivity<REAL>>& activities ) const
   {
      const Vec<REAL>& lhs = constraintMatrix.getLeftHandSides();
      const Vec<REAL>& rhs = constraintMatrix.getRightHandSides();
      const Vec<int>& colsize = constraintMatrix.getColSizes();
      const Vec<RowFlags>& rflags = getRowFlags();

      const Vec<REAL>& lbs = getLowerBounds();
      const Vec<REAL>& ubs = getUpperBounds();
      int nremoved = 0;
      int nnewfreevars = 0;

      Vec<std::tuple<int, REAL, int>> colperm( getNCols() );

      for( int i = 0; i != getNCols(); ++i )
         colperm[i] = std::make_tuple(
             colsize[i],
             constraintMatrix.getColumnCoefficients( i ).getDynamism(), i );

      pdqsort( colperm.begin(), colperm.end() );

      for( const auto& tuple : colperm )
      {
         int col = std::get<2>( tuple );

         if( cflags[col].test( ColFlag::kInactive ) ||
             !cflags[col].test( ColFlag::kUnbounded ) )
            continue;

         auto colvec = constraintMatrix.getColumnCoefficients( col );
         const int* colrows = colvec.getIndices();
         const REAL* colvals = colvec.getValues();
         const int collen = colvec.getLength();

         int k = 0;

         ColFlags colf = cflags[col];

         while( ( !colf.test( ColFlag::kLbInf ) ||
                  !colf.test( ColFlag::kUbInf ) ) &&
                k != collen )
         {
            int row = colrows[k];

            if( rflags[row].test( RowFlag::kRedundant ) )
            {
               ++k;
               continue;
            }

            if( !colf.test( ColFlag::kLbInf ) &&
                row_implies_LB( num, lhs[row], rhs[row], rflags[row],
                                activities[row], colvals[k], lbs[col], ubs[col],
                                cflags[col] ) )
               colf.set( ColFlag::kLbInf );

            if( !colf.test( ColFlag::kUbInf ) &&
                row_implies_UB( num, lhs[row], rhs[row], rflags[row],
                                activities[row], colvals[k], lbs[col], ubs[col],
                                cflags[col] ) )
               colf.set( ColFlag::kUbInf );

            ++k;
         }

         if( colf.test( ColFlag::kLbInf ) && colf.test( ColFlag::kUbInf ) )
         {
            int oldnremoved = nremoved;
            if( !cflags[col].test( ColFlag::kLbInf ) )
            {
               update_activities_remove_finite_bound( colrows, colvals, collen,
                                                      BoundChange::kLower,
                                                      lbs[col], activities );
               cflags[col].set( ColFlag::kLbInf );
               ++nremoved;
            }

            if( !cflags[col].test( ColFlag::kUbInf ) )
            {
               update_activities_remove_finite_bound( colrows, colvals, collen,
                                                      BoundChange::kUpper,
                                                      ubs[col], activities );
               cflags[col].set( ColFlag::kUbInf );
               ++nremoved;
            }

            if( oldnremoved != nremoved )
               ++nnewfreevars;
         }
      }

      return std::make_pair( nremoved, nnewfreevars );
   }

   template <typename Archive>
   void
   serialize( Archive& ar, const unsigned int version )
   {
      ar& inputTolerance;
      ar& objective;
      ar& constraintMatrix;
      ar& variableDomains;
      ar& ncontinuous;
      ar& nintegers;

      ar& rowActivities;
      ar& locks;
      ar& constraintTypes;

      ar& name;
      ar& variableNames;
      ar& constraintNames;
   }

 private:
   REAL inputTolerance{ 0 };
   Objective<REAL> objective;
   ConstraintMatrix<REAL> constraintMatrix;
   VariableDomains<REAL> variableDomains;
   int ncontinuous;
   int nintegers;

   /// minimal and maximal row activities
   Vec<RowActivity<REAL>> rowActivities;

   /// up and down locks for each column
   Vec<Locks> locks;

   /// constraint type specifiers
   Vec<char> constraintTypes;

   String name;
   Vec<String> variableNames;
   Vec<String> constraintNames;
};

} // namespace bugger

#endif
