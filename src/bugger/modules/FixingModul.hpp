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

#ifndef __BUGGER_MODULE_FIXING_HPP__
#define __BUGGER_MODULE_FIXING_HPP__

#include "bugger/modules/BuggerModul.hpp"


namespace bugger
{
   template <typename REAL>
   class FixingModul : public BuggerModul<REAL>
   {
   public:

      explicit FixingModul(const Message& _msg, const Num<REAL>& _num, const BuggerParameters& _parameters,
                           std::shared_ptr<SolverFactory<REAL>>& _factory)
                           : BuggerModul<REAL>(_msg, _num, _parameters, _factory)
      {
         this->setName("fixing");
      }

   private:

      bool
      isFixingAdmissible(const Problem<REAL>& problem, const int& col) const
      {
         return !problem.getColFlags( )[ col ].test(ColFlag::kFixed)
             && !problem.getColFlags( )[ col ].test(ColFlag::kLbInf)
             && !problem.getColFlags( )[ col ].test(ColFlag::kUbInf)
             && this->num.isZetaEq(problem.getLowerBounds( )[ col ], problem.getUpperBounds( )[ col ]);
      }

      ModulStatus
      execute(SolverSettings& settings, Problem<REAL>& problem, Solution<REAL>& solution) override
      {
         long long batchsize = 1;

         if( this->parameters.nbatches > 0 )
         {
            batchsize = this->parameters.nbatches - 1;
            for( int col = problem.getNCols( ) - 1; col >= 0; --col )
               if( isFixingAdmissible(problem, col) )
                  ++batchsize;
            if( batchsize == this->parameters.nbatches - 1 )
               return ModulStatus::kNotAdmissible;
            batchsize /= this->parameters.nbatches;
         }

         bool admissible = false;
         auto copy = Problem<REAL>(problem);
         Vec<int> applied_reductions { };
         Vec<MatrixEntry<REAL>> applied_entries { };
         Vec<std::pair<int, REAL>> applied_lefts { };
         Vec<std::pair<int, REAL>> applied_rights { };
         Vec<int> batches_vars { };
         Vec<MatrixEntry<REAL>> batches_coeff { };
         Vec<std::pair<int, REAL>> batches_lhs { };
         Vec<std::pair<int, REAL>> batches_rhs { };
         batches_vars.reserve(batchsize);

         for( int col = copy.getNCols( ) - 1; col >= 0; --col )
         {
            if( isFixingAdmissible(copy, col) )
            {
               admissible = true;
               const auto& col_data = copy.getConstraintMatrix( ).getColumnCoefficients(col);
               REAL fixedval { };
               if( solution.status == SolutionStatus::kFeasible )
               {
                  fixedval = solution.primal[ col ];
                  if( copy.getColFlags( )[ col ].test(ColFlag::kIntegral) )
                     fixedval = this->num.round(fixedval);
               }
               else
               {
                  if( copy.getColFlags( )[ col ].test(ColFlag::kIntegral) )
                  {
                     if( !copy.getColFlags( )[ col ].test(ColFlag::kUbInf) )
                        fixedval = this->num.min(fixedval, this->num.epsFloor(copy.getUpperBounds( )[ col ]));
                     if( !copy.getColFlags( )[ col ].test(ColFlag::kLbInf) )
                        fixedval = this->num.max(fixedval, this->num.epsCeil(copy.getLowerBounds( )[ col ]));
                  }
                  else
                  {
                     if( !copy.getColFlags( )[ col ].test(ColFlag::kUbInf) )
                        fixedval = this->num.min(fixedval, copy.getUpperBounds( )[ col ]);
                     if( !copy.getColFlags( )[ col ].test(ColFlag::kLbInf) )
                        fixedval = this->num.max(fixedval, copy.getLowerBounds( )[ col ]);
                  }
               }
               assert(!copy.getColFlags( )[ col ].test(ColFlag::kFixed));
               copy.getColFlags( )[ col ].set(ColFlag::kFixed);
               batches_vars.push_back(col);
               for( int row_index = col_data.getLength( ) - 1; row_index >= 0; --row_index )
               {
                  int row = col_data.getIndices( )[ row_index ];
                  REAL val = col_data.getValues( )[ row_index ];
                  if( !this->num.isZetaZero(val) && !copy.getConstraintMatrix( ).getRowFlags( )[ row ].test(RowFlag::kRedundant))
                  {
                     const auto& row_data = copy.getConstraintMatrix( ).getRowCoefficients(row);
                     bool integral = true;
                     REAL offset { -val * fixedval };
                     for( int col_index = 0; col_index < row_data.getLength( ); ++col_index )
                     {
                        int index = row_data.getIndices( )[ col_index ];
                        REAL value = row_data.getValues( )[ col_index ];
                        if( !copy.getColFlags( )[ index ].test(ColFlag::kFixed) && ( !copy.getColFlags( )[ index ].test(ColFlag::kIntegral) || !this->num.isEpsIntegral(value) ) )
                        {
                           integral = false;
                           break;
                        }
                     }
                     batches_coeff.emplace_back(row, col, 0);
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf) )
                     {
                        REAL lhs { copy.getConstraintMatrix( ).getLeftHandSides( )[ row ] + offset };
                        if( integral )
                           lhs = this->num.round(lhs);
                        if( !this->num.isZetaEq(copy.getConstraintMatrix( ).getLeftHandSides( )[ row ], lhs) )
                        {
                           copy.getConstraintMatrix( ).modifyLeftHandSide(row, this->num, lhs);
                           batches_lhs.emplace_back(row, lhs);
                        }
                     }
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf) )
                     {
                        REAL rhs { copy.getConstraintMatrix( ).getRightHandSides( )[ row ] + offset };
                        if( integral )
                           rhs = this->num.round(rhs);
                        if( !this->num.isZetaEq(copy.getConstraintMatrix( ).getRightHandSides( )[ row ], rhs) )
                        {
                           copy.getConstraintMatrix( ).modifyRightHandSide(row, this->num, rhs);
                           batches_rhs.emplace_back(row, rhs);
                        }
                     }
                  }
               }
            }

            if( !batches_vars.empty() && ( batches_vars.size() >= batchsize || col <= 0 ) )
            {
               this->apply_changes(copy, batches_coeff);
               if( this->call_solver(settings, copy, solution) == BuggerStatus::kOkay )
               {
                  copy = Problem<REAL>(problem);
                  for( const auto& item: applied_reductions )
                  {
                     assert(!copy.getColFlags( )[ item ].test(ColFlag::kFixed));
                     copy.getColFlags( )[ item ].set(ColFlag::kFixed);
                  }
                  this->apply_changes(copy, applied_entries);
                  for( const auto& item: applied_lefts )
                     copy.getConstraintMatrix( ).modifyLeftHandSide( item.first, this->num, item.second );
                  for( const auto& item: applied_rights )
                     copy.getConstraintMatrix( ).modifyRightHandSide( item.first, this->num, item.second );
               }
               else
               {
                  applied_reductions.insert(applied_reductions.end(), batches_vars.begin(), batches_vars.end());
                  applied_entries.insert(applied_entries.end(), batches_coeff.begin(), batches_coeff.end());
                  applied_lefts.insert(applied_lefts.end(), batches_lhs.begin(), batches_lhs.end());
                  applied_rights.insert(applied_rights.end(), batches_rhs.begin(), batches_rhs.end());
               }
               batches_vars.clear();
               batches_coeff.clear();
               batches_lhs.clear();
               batches_rhs.clear();
            }
         }

         if( !admissible )
            return ModulStatus::kNotAdmissible;
         if( applied_reductions.empty() )
            return ModulStatus::kUnsuccesful;
         problem = copy;
         this->naggrvars += applied_reductions.size();
         return ModulStatus::kSuccessful;
      }
   };

} // namespace bugger

#endif
