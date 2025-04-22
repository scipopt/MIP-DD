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

#ifndef __BUGGER_MODIFIERS_OBJECTIVEMODIFIER_HPP__
#define __BUGGER_MODIFIERS_OBJECTIVEMODIFIER_HPP__

#include "bugger/modifiers/BuggerModifier.hpp"


namespace bugger
{
   template <typename REAL>
   class ObjectiveModifier : public BuggerModifier<REAL>
   {
   public:

      explicit ObjectiveModifier(const Message& _msg, const Num<REAL>& _num, const BuggerParameters& _parameters,
                              std::shared_ptr<SolverFactory<REAL>>& _factory)
                              : BuggerModifier<REAL>(_msg, _num, _parameters, _factory)
      {
         this->setName("objective");
      }

   private:

      bool
      isObjectiveAdmissible(const Problem<REAL>& problem, const int& col) const
      {
         return !problem.getColFlags( )[ col ].test(ColFlag::kFixed)
             && !this->num.isZetaZero(problem.getObjective( ).coefficients[ col ])
             && ( problem.getColFlags( )[ col ].test(ColFlag::kLbInf)
               || problem.getColFlags( )[ col ].test(ColFlag::kUbInf)
               || !this->num.isZetaEq(problem.getLowerBounds( )[ col ], problem.getUpperBounds( )[ col ]) );
      }

      ModifierStatus
      execute(SolverSettings& settings, Problem<REAL>& problem, Solution<REAL>& solution) override
      {
         if( solution.status == SolutionStatus::kUnbounded
            || ( solution.status == SolutionStatus::kFeasible && solution.primal.size() != problem.getNCols() ) )
            return ModifierStatus::kNotAdmissible;

         long long nbatches = this->parameters.emphasis == EMPHASIS_AGGRESSIVE ? 0 : this->parameters.nbatches;
         long long batchsize = 1;

         if( nbatches > 0 )
         {
            batchsize = nbatches - 1;
            for( int i = problem.getNCols( ) - 1; i >= 0; --i )
               if( isObjectiveAdmissible(problem, i) )
                  ++batchsize;
            if( batchsize == nbatches - 1 )
               return ModifierStatus::kNotAdmissible;
            batchsize /= nbatches;
         }

         auto copy = Problem<REAL>(problem);
         Vec<int> applied_reductions { };
         Vec<int> batches { };
         batches.reserve(batchsize);

         for( int col = copy.getNCols( ) - 1; col >= 0; --col )
         {
            if( isObjectiveAdmissible(copy, col) )
            {
               ++this->last_admissible;
               copy.getObjective( ).coefficients[ col ] = 0;
               batches.push_back(col);
            }

            if( !batches.empty() && ( batches.size() >= batchsize || col <= 0 ) )
            {
               if( this->call_solver(settings, copy, solution) == BuggerStatus::kOkay )
               {
                  copy = Problem<REAL>(problem);
                  for( const auto& item: applied_reductions )
                     copy.getObjective( ).coefficients[ item ] = 0;
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
         this->nchgcoefs += applied_reductions.size();
         return ModifierStatus::kSuccessful;
      }
   };

} // namespace bugger

#endif
