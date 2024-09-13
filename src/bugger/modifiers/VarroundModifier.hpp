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

#ifndef __BUGGER_MODIFIERS_VARROUNDMODIFIER_HPP__
#define __BUGGER_MODIFIERS_VARROUNDMODIFIER_HPP__

#include "bugger/modifiers/BuggerModifier.hpp"


namespace bugger
{
   template <typename REAL>
   class VarroundModifier : public BuggerModifier<REAL>
   {
   public:

      explicit VarroundModifier(const Message& _msg, const Num<REAL>& _num, const BuggerParameters& _parameters,
                    std::shared_ptr<SolverFactory<REAL>>& _factory)
                    : BuggerModifier<REAL>(_msg, _num, _parameters, _factory)
      {
         this->setName("varround");
      }

   private:

      bool
      isVarroundAdmissible(const Problem<REAL>& problem, const int& col) const
      {
         if( problem.getColFlags( )[ col ].test(ColFlag::kFixed) )
            return false;
         if( !this->num.isZetaIntegral(problem.getObjective( ).coefficients[ col ]) )
            return true;
         bool lbinf = problem.getColFlags( )[ col ].test(ColFlag::kLbInf);
         bool ubinf = problem.getColFlags( )[ col ].test(ColFlag::kUbInf);
         REAL lb { problem.getLowerBounds( )[ col ] };
         REAL ub { problem.getUpperBounds( )[ col ] };
         return ( lbinf || ubinf || !this->num.isZetaEq(lb, ub) ) && ( ( !lbinf && !this->num.isZetaIntegral(lb) ) || ( !ubinf && !this->num.isZetaIntegral(ub) ) );
      }

      ModifierStatus
      execute(SolverSettings& settings, Problem<REAL>& problem, Solution<REAL>& solution) override
      {
         if( solution.status == SolutionStatus::kInfeasible || solution.status == SolutionStatus::kUnbounded )
            return ModifierStatus::kNotAdmissible;

         long long batchsize = 1;

         if( this->parameters.nbatches > 0 )
         {
            batchsize = this->parameters.nbatches - 1;
            for( int i = 0; i < problem.getNCols( ); ++i )
               if( isVarroundAdmissible(problem, i) )
                  ++batchsize;
            if( batchsize == this->parameters.nbatches - 1 )
               return ModifierStatus::kNotAdmissible;
            batchsize /= this->parameters.nbatches;
         }

         auto copy = Problem<REAL>(problem);
         Vec<std::pair<int, REAL>> applied_objectives { };
         Vec<std::pair<int, REAL>> applied_lowers { };
         Vec<std::pair<int, REAL>> applied_uppers { };
         Vec<std::pair<int, REAL>> batches_obj { };
         Vec<std::pair<int, REAL>> batches_lb { };
         Vec<std::pair<int, REAL>> batches_ub { };
         batches_obj.reserve(batchsize);
         batches_lb.reserve(batchsize);
         batches_ub.reserve(batchsize);
         int batch = 0;

         for( int col = 0; col < copy.getNCols( ); ++col )
         {
            if( isVarroundAdmissible(copy, col) )
            {
               ++this->last_admissible;
               REAL lb { round(copy.getLowerBounds( )[ col ]) };
               REAL ub { round(copy.getUpperBounds( )[ col ]) };
               if( solution.status == SolutionStatus::kFeasible )
               {
                  REAL value { solution.primal[ col ] };
                  lb = min(lb, this->num.epsFloor(value));
                  ub = max(ub, this->num.epsCeil(value));
               }
               if( !this->num.isZetaIntegral(copy.getObjective( ).coefficients[ col ]) )
               {
                  REAL obj { round(copy.getObjective( ).coefficients[ col ]) };
                  copy.getObjective( ).coefficients[ col ] = obj;
                  batches_obj.emplace_back(col, obj);
               }
               if( !copy.getColFlags( )[ col ].test(ColFlag::kLbInf) && !this->num.isZetaEq(copy.getLowerBounds( )[ col ], lb) )
               {
                  copy.getLowerBounds( )[ col ] = lb;
                  batches_lb.emplace_back(col, lb);
               }
               if( !copy.getColFlags( )[ col ].test(ColFlag::kUbInf) && !this->num.isZetaEq(copy.getUpperBounds( )[ col ], ub) )
               {
                  copy.getUpperBounds( )[ col ] = ub;
                  batches_ub.emplace_back(col, ub);
               }
               ++batch;
            }

            if( batch >= 1 && ( batch >= batchsize || col >= copy.getNCols( ) - 1 ) )
            {
               if( this->call_solver(settings, copy, solution) == BuggerStatus::kOkay )
               {
                  copy = Problem<REAL>(problem);
                  for( const auto& item: applied_objectives )
                     copy.getObjective( ).coefficients[ item.first ] = item.second;
                  for( const auto& item: applied_lowers )
                     copy.getLowerBounds( )[ item.first ] = item.second;
                  for( const auto& item: applied_uppers )
                     copy.getUpperBounds( )[ item.first ] = item.second;
               }
               else
               {
                  applied_objectives.insert(applied_objectives.end(), batches_obj.begin(), batches_obj.end());
                  applied_lowers.insert(applied_lowers.end(), batches_lb.begin(), batches_lb.end());
                  applied_uppers.insert(applied_uppers.end(), batches_ub.begin(), batches_ub.end());
               }
               batches_obj.clear();
               batches_lb.clear();
               batches_ub.clear();
               batch = 0;
            }
         }

         if( this->last_admissible == 0 )
            return ModifierStatus::kNotAdmissible;
         if( applied_objectives.empty() && applied_lowers.empty() && applied_uppers.empty() )
            return ModifierStatus::kUnsuccesful;
         problem = copy;
         this->nchgcoefs += applied_objectives.size() + applied_lowers.size() + applied_uppers.size();
         return ModifierStatus::kSuccessful;
      }
   };

} // namespace bugger

#endif
