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

#ifndef __BUGGER_MODIFIERS_COEFFICIENTMODIFIER_HPP__
#define __BUGGER_MODIFIERS_COEFFICIENTMODIFIER_HPP__

#include "bugger/modifiers/BuggerModifier.hpp"


namespace bugger
{
   template <typename REAL>
   class CoefficientModifier : public BuggerModifier<REAL>
   {
   public:

      explicit CoefficientModifier(const Message& _msg, const Num<REAL>& _num, const BuggerParameters& _parameters,
                                std::shared_ptr<SolverFactory<REAL>>& _factory)
                                : BuggerModifier<REAL>(_msg, _num, _parameters, _factory)
      {
         this->setName("coefficient");
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

      bool
      isCoefficientAdmissible(const Problem<REAL>& problem, const int& row) const
      {
         if( problem.getConstraintMatrix( ).getRowFlags( )[ row ].test(RowFlag::kRedundant) )
            return false;
         const auto& data = problem.getConstraintMatrix( ).getRowCoefficients(row);
         for( int index = 0; index < data.getLength( ); ++index )
            if( !this->num.isZetaZero(data.getValues( )[ index ]) && isFixingAdmissible(problem, data.getIndices( )[ index ]) )
               return true;
         return false;
      }

      ModifierStatus
      execute(SolverSettings& settings, Problem<REAL>& problem, Solution<REAL>& solution) override
      {
         long long batchsize = 1;

         if( this->parameters.nbatches > 0 )
         {
            batchsize = this->parameters.nbatches - 1;
            for( int i = problem.getNRows( ) - 1; i >= 0; --i )
               if( isCoefficientAdmissible(problem, i) )
                  ++batchsize;
            if( batchsize == this->parameters.nbatches - 1 )
               return ModifierStatus::kNotAdmissible;
            batchsize /= this->parameters.nbatches;
         }

         bool admissible = false;
         auto copy = Problem<REAL>(problem);
         auto& matrix = copy.getConstraintMatrix( );
         Vec<MatrixEntry<REAL>> applied_entries { };
         Vec<std::pair<int, REAL>> applied_lefts { };
         Vec<std::pair<int, REAL>> applied_rights { };
         Vec<MatrixEntry<REAL>> batches_coeff { };
         Vec<std::pair<int, REAL>> batches_lhs { };
         Vec<std::pair<int, REAL>> batches_rhs { };
         batches_lhs.reserve(batchsize);
         batches_rhs.reserve(batchsize);
         int batch = 0;

         for( int row = copy.getNRows( ) - 1; row >= 0; --row )
         {
            if( isCoefficientAdmissible(copy, row) )
            {
               admissible = true;
               const auto& data = matrix.getRowCoefficients(row);
               bool integral = true;
               REAL offset { };
               for( int index = data.getLength( ) - 1; index >= 0; --index )
               {
                  int col = data.getIndices( )[ index ];
                  REAL val = data.getValues( )[ index ];
                  if( !this->num.isZetaZero(val) && isFixingAdmissible(copy, col) )
                  {
                     REAL fixedval { };
                     if( solution.status == SolutionStatus::kFeasible )
                     {
                        fixedval = solution.primal[ col ];
                        if( copy.getColFlags( )[ col ].test(ColFlag::kIntegral) )
                           fixedval = round(fixedval);
                     }
                     else
                     {
                        if( copy.getColFlags( )[ col ].test(ColFlag::kIntegral) )
                        {
                           if( !copy.getColFlags( )[ col ].test(ColFlag::kUbInf) )
                              fixedval = min(fixedval, this->num.epsFloor(copy.getUpperBounds( )[ col ]));
                           if( !copy.getColFlags( )[ col ].test(ColFlag::kLbInf) )
                              fixedval = max(fixedval, this->num.epsCeil(copy.getLowerBounds( )[ col ]));
                        }
                        else
                        {
                           if( !copy.getColFlags( )[ col ].test(ColFlag::kUbInf) )
                              fixedval = min(fixedval, copy.getUpperBounds( )[ col ]);
                           if( !copy.getColFlags( )[ col ].test(ColFlag::kLbInf) )
                              fixedval = max(fixedval, copy.getLowerBounds( )[ col ]);
                        }
                     }
                     offset -= val * fixedval;
                     batches_coeff.emplace_back(row, col, 0);
                  }
                  else if( !copy.getColFlags( )[ col ].test(ColFlag::kFixed) && ( !copy.getColFlags( )[ col ].test(ColFlag::kIntegral) || !this->num.isEpsIntegral(val) ) )
                     integral = false;
               }
               if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf) )
               {
                  REAL lhs { matrix.getLeftHandSides( )[ row ] + offset };
                  if( integral )
                     lhs = round(lhs);
                  if( !this->num.isZetaEq(matrix.getLeftHandSides( )[ row ], lhs) )
                  {
                     matrix.modifyLeftHandSide(row, this->num, lhs);
                     batches_lhs.emplace_back(row, lhs);
                  }
               }
               if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf) )
               {
                  REAL rhs { matrix.getRightHandSides( )[ row ] + offset };
                  if( integral )
                     rhs = round(rhs);
                  if( !this->num.isZetaEq(matrix.getRightHandSides( )[ row ], rhs) )
                  {
                     matrix.modifyRightHandSide(row, this->num, rhs);
                     batches_rhs.emplace_back(row, rhs);
                  }
               }
               ++batch;
            }

            if( batch >= 1 && ( batch >= batchsize || row <= 0 ) )
            {
               this->apply_changes(copy, batches_coeff);
               if( this->call_solver(settings, copy, solution) == BuggerStatus::kOkay )
               {
                  copy = Problem<REAL>(problem);
                  this->apply_changes(copy, applied_entries);
                  for( const auto& item: applied_lefts )
                     matrix.modifyLeftHandSide( item.first, this->num, item.second );
                  for( const auto& item: applied_rights )
                     matrix.modifyRightHandSide( item.first, this->num, item.second );
               }
               else
               {
                  applied_entries.insert(applied_entries.end(), batches_coeff.begin(), batches_coeff.end());
                  applied_lefts.insert(applied_lefts.end(), batches_lhs.begin(), batches_lhs.end());
                  applied_rights.insert(applied_rights.end(), batches_rhs.begin(), batches_rhs.end());
               }
               batches_coeff.clear();
               batches_lhs.clear();
               batches_rhs.clear();
               batch = 0;
            }
         }

         if( !admissible )
            return ModifierStatus::kNotAdmissible;
         if( applied_entries.empty() )
            return ModifierStatus::kUnsuccesful;
         problem = copy;
         this->nchgcoefs += applied_entries.size();
         this->nchgsides += applied_lefts.size() + applied_rights.size();
         return ModifierStatus::kSuccessful;
      }
   };

} // namespace bugger

#endif
