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

#ifndef __BUGGER_MODIFIERS_CONSTRAINTMODIFIER_HPP__
#define __BUGGER_MODIFIERS_CONSTRAINTMODIFIER_HPP__

#include "bugger/modifiers/BuggerModifier.hpp"


namespace bugger
{
   template <typename REAL>
   class ConstraintModifier : public BuggerModifier<REAL>
   {
   public:

      explicit ConstraintModifier(const Message& _msg, const Num<REAL>& _num, const BuggerParameters& _parameters,
                               std::shared_ptr<SolverFactory<REAL>>& _factory)
                               : BuggerModifier<REAL>(_msg, _num, _parameters, _factory)
      {
         this->setName("constraint");
      }

   private:

      bool
      isConstraintAdmissible(const Problem<REAL>& problem, const Solution<REAL>& solution, const int& row) const
      {
         if( problem.getConstraintMatrix( ).getRowFlags( )[ row ].test(RowFlag::kRedundant) )
            return false;
         if( solution.status != SolutionStatus::kInfeasible )
            return true;
         if( !problem.getRowFlags( )[ row ].test(RowFlag::kLhsInf)
          && !problem.getRowFlags( )[ row ].test(RowFlag::kRhsInf)
          && this->num.isZetaGT(problem.getConstraintMatrix( ).getLeftHandSides( )[ row ], problem.getConstraintMatrix( ).getRightHandSides( )[ row ]) )
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
               return false;
         }
         return true;
      }

      ModifierStatus
      execute(SolverSettings& settings, Problem<REAL>& problem, Solution<REAL>& solution) override
      {
         long long nbatches = this->parameters.emphasis == 1 ? 0 : this->parameters.nbatches;
         long long batchsize = 1;

         if( nbatches > 0 )
         {
            batchsize = nbatches - 1;
            for( int i = problem.getNRows( ) - 1; i >= 0; --i )
               if( isConstraintAdmissible(problem, solution, i) )
                  ++batchsize;
            if( batchsize == nbatches - 1 )
               return ModifierStatus::kNotAdmissible;
            batchsize /= nbatches;
         }

         auto copy = Problem<REAL>(problem);
         Vec<int> applied_reductions { };
         Vec<int> batches { };
         batches.reserve(batchsize);

         for( int row = copy.getNRows( ) - 1; row >= 0; --row )
         {
            if( isConstraintAdmissible(copy, solution, row) )
            {
               ++this->last_admissible;
               assert(!copy.getRowFlags( )[ row ].test(RowFlag::kRedundant));
               copy.getRowFlags( )[ row ].set(RowFlag::kRedundant);
               batches.push_back(row);
            }

            if( !batches.empty() && ( batches.size() >= batchsize || row <= 0 ) )
            {
               if( this->call_solver(settings, copy, solution) == BuggerStatus::kOkay )
               {
                  copy = Problem<REAL>(problem);
                  for( const auto& item: applied_reductions )
                  {
                     assert(!copy.getRowFlags( )[ item ].test(RowFlag::kRedundant));
                     copy.getRowFlags( )[ item ].set(RowFlag::kRedundant);
                  }
               }
               else
                  applied_reductions.insert(applied_reductions.end(), batches.begin(), batches.end());
               batches.clear();
            }
         }

         if( this->last_admissible == 0 )
            return ModifierStatus::kNotAdmissible;
         if( this->parameters.emphasis == 1 && this->parameters.nbatches > 0 && this->last_admissible > this->parameters.nbatches )
            this->last_admissible = this->parameters.nbatches;
         if( applied_reductions.empty() )
            return ModifierStatus::kUnsuccesful;
         problem = copy;
         this->ndeletedrows += applied_reductions.size();
         return ModifierStatus::kSuccessful;
      }
   };

} // namespace bugger

#endif
