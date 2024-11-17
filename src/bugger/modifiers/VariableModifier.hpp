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

#ifndef __BUGGER_MODIFIERS_VARIABLEMODIFIER_HPP__
#define __BUGGER_MODIFIERS_VARIABLEMODIFIER_HPP__

#include "bugger/modifiers/BuggerModifier.hpp"


namespace bugger
{
   template <typename REAL>
   class VariableModifier : public BuggerModifier<REAL>
   {
   public:

      explicit VariableModifier(const Message& _msg, const Num<REAL>& _num, const BuggerParameters& _parameters,
                             std::shared_ptr<SolverFactory<REAL>>& _factory)
                             : BuggerModifier<REAL>(_msg, _num, _parameters, _factory)
      {
         this->setName("variable");
      }

   private:

      bool
      isVariableAdmissible(const Problem<REAL>& problem, const int& col) const
      {
         if( problem.getColFlags( )[ col ].test(ColFlag::kFixed)
          || ( !problem.getColFlags( )[ col ].test(ColFlag::kLbInf)
            && !problem.getColFlags( )[ col ].test(ColFlag::kUbInf)
            && this->num.isZetaGE(problem.getLowerBounds( )[ col ], problem.getUpperBounds( )[ col ]) ) )
            return false;
         const auto& data = problem.getConstraintMatrix( ).getColumnCoefficients(col);
         for( int index = 0; index < data.getLength( ); ++index )
            if( !this->num.isZetaZero(data.getValues( )[ index ])
             && !problem.getConstraintMatrix( ).getRowFlags( )[ data.getIndices( )[ index ] ].test(RowFlag::kRedundant) )
               return true;
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
            for( int i = problem.getNCols() - 1; i >= 0; --i )
               if( isVariableAdmissible(problem, i) )
                  ++batchsize;
            if( batchsize == this->parameters.nbatches - 1 )
               return ModifierStatus::kNotAdmissible;
            batchsize /= this->parameters.nbatches;
         }

         auto copy = Problem<REAL>(problem);
         Vec<std::pair<int, REAL>> applied_reductions { };
         Vec<std::pair<int, REAL>> batches { };
         batches.reserve(batchsize);

         for( int col = copy.getNCols() - 1; col >= 0; --col )
         {
            if( isVariableAdmissible(copy, col) )
            {
               ++this->last_admissible;
               REAL fixedval { };
               if( solution.primal.size() == copy.getNCols() )
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
               copy.getColFlags( )[ col ].unset(ColFlag::kLbInf);
               copy.getColFlags( )[ col ].unset(ColFlag::kUbInf);
               copy.getLowerBounds( )[ col ] = fixedval;
               copy.getUpperBounds( )[ col ] = fixedval;
               batches.emplace_back(col, fixedval);
            }

            if( !batches.empty() && ( batches.size() >= batchsize || col <= 0 ) )
            {
               if( this->call_solver(settings, copy, solution) == BuggerStatus::kOkay )
               {
                  copy = Problem<REAL>(problem);
                  for( const auto& item: applied_reductions )
                  {
                     copy.getColFlags( )[ item.first ].unset(ColFlag::kLbInf);
                     copy.getColFlags( )[ item.first ].unset(ColFlag::kUbInf);
                     copy.getLowerBounds( )[ item.first ] = item.second;
                     copy.getUpperBounds( )[ item.first ] = item.second;
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
         this->nfixedvars += applied_reductions.size();
         return ModifierStatus::kSuccessful;
      }
   };

} // namespace bugger

#endif
