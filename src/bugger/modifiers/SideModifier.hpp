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

#ifndef __BUGGER_MODIFIERS_SIDEMODIFIER_HPP__
#define __BUGGER_MODIFIERS_SIDEMODIFIER_HPP__

#include "bugger/modifiers/BuggerModifier.hpp"


namespace bugger
{
   template <typename REAL>
   class SideModifier : public BuggerModifier<REAL>
   {
   public:

      explicit SideModifier(const Message& _msg, const Num<REAL>& _num, const BuggerParameters& _parameters,
                         std::shared_ptr<SolverFactory<REAL>>& _factory)
                         : BuggerModifier<REAL>(_msg, _num, _parameters, _factory)
      {
         this->setName("side");
      }

   private:

      bool
      isSideAdmissible(const Problem<REAL>& problem, const int& row) const
      {
         if( problem.getRowFlags( )[ row ].test(RowFlag::kRedundant)
          || ( !problem.getRowFlags( )[ row ].test(RowFlag::kLhsInf)
            && !problem.getRowFlags( )[ row ].test(RowFlag::kRhsInf)
            && this->num.isZetaGE(problem.getConstraintMatrix( ).getLeftHandSides( )[ row ], problem.getConstraintMatrix( ).getRightHandSides( )[ row ]) ) )
            return false;
         const auto& data = problem.getConstraintMatrix( ).getRowCoefficients(row);
         for( int index = 0; index < data.getLength( ); ++index )
         {
            int col = data.getIndices( )[ index ];
            if( !this->num.isZetaZero(data.getValues( )[ index ])
             && !problem.getColFlags( )[ col ].test(ColFlag::kFixed)
             && ( problem.getColFlags( )[ col ].test(ColFlag::kLbInf)
               || problem.getColFlags( )[ col ].test(ColFlag::kUbInf)
               || this->num.isZetaLT(problem.getLowerBounds( )[ col ], problem.getUpperBounds( )[ col ]) ) )
               return true;
         }
         return false;
      }

      ModifierStatus
      execute(SolverSettings& settings, Problem<REAL>& problem, Solution<REAL>& solution) override
      {
         if( solution.status == SolutionStatus::kUnbounded )
            return ModifierStatus::kNotAdmissible;

         long long batchsize = 1;

         if( this->parameters.nbatches > 0 )
         {
            batchsize = this->parameters.nbatches - 1;
            for( int row = problem.getNRows( ) - 1; row >= 0; --row )
               if( isSideAdmissible(problem, row) )
                  ++batchsize;
            if( batchsize == this->parameters.nbatches - 1 )
               return ModifierStatus::kNotAdmissible;
            batchsize /= this->parameters.nbatches;
         }

         auto copy = Problem<REAL>(problem);
         auto& matrix = copy.getConstraintMatrix( );
         Vec<std::pair<int, REAL>> applied_reductions { };
         Vec<std::pair<int, REAL>> batches { };
         batches.reserve(batchsize);

         for( int row = copy.getNRows( ) - 1; row >= 0; --row )
         {
            if( isSideAdmissible(copy, row) )
            {
               ++this->last_admissible;
               const auto& data = matrix.getRowCoefficients(row);
               bool integral = true;
               REAL fixedval { };
               for( int index = 0; index < data.getLength( ); ++index )
               {
                  if( !copy.getColFlags( )[ data.getIndices( )[ index ] ].test(ColFlag::kFixed) && ( !copy.getColFlags( )[ data.getIndices( )[ index ] ].test(ColFlag::kIntegral) || !this->num.isEpsIntegral(data.getValues( )[ index ]) ) )
                  {
                     integral = false;
                     break;
                  }
               }
               if( solution.status == SolutionStatus::kFeasible )
               {
                  fixedval = copy.getPrimalActivity(solution, row);
                  if( integral )
                     fixedval = round(fixedval);
               }
               else
               {
                  if( integral )
                  {
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf) )
                        fixedval = min(fixedval, this->num.epsFloor(matrix.getRightHandSides( )[ row ]));
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf) )
                        fixedval = max(fixedval, this->num.epsCeil(matrix.getLeftHandSides( )[ row ]));
                  }
                  else
                  {
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf) )
                        fixedval = min(fixedval, matrix.getRightHandSides( )[ row ]);
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf) )
                        fixedval = max(fixedval, matrix.getLeftHandSides( )[ row ]);
                  }
               }
               matrix.modifyLeftHandSide( row, this->num, fixedval );
               matrix.modifyRightHandSide( row, this->num, fixedval );
               batches.emplace_back(row, fixedval);
            }

            if( !batches.empty() && ( batches.size() >= batchsize || row <= 0 ) )
            {
               if( this->call_solver(settings, copy, solution) == BuggerStatus::kOkay )
               {
                  copy = Problem<REAL>(problem);
                  for( const auto& item: applied_reductions )
                  {
                     matrix.modifyLeftHandSide( item.first, this->num, item.second );
                     matrix.modifyRightHandSide( item.first, this->num, item.second );
                  }
               }
               else
                  applied_reductions.insert(applied_reductions.end(), batches.begin(), batches.end());
               batches.clear();
            }
         }

         if( this->last_admissible == 0 )
            return ModifierStatus::kNotAdmissible;
         if( applied_reductions.empty() )
            return ModifierStatus::kUnsuccesful;
         problem = copy;
         this->nchgsides += 2 * applied_reductions.size();
         return ModifierStatus::kSuccessful;
      }
   };

} // namespace bugger

#endif
