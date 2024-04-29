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

#ifndef __BUGGER_MODULE_VARROUND_HPP__
#define __BUGGER_MODULE_VARROUND_HPP__

#include "bugger/modules/BuggerModul.hpp"


namespace bugger {

   template <typename REAL>
   class VarroundModul : public BuggerModul<REAL> {

   public:

      explicit VarroundModul(const Message& _msg, const Num<REAL>& _num, const BuggerParameters& _parameters,
                    std::shared_ptr<SolverFactory<REAL>>& _factory)
                    : BuggerModul<REAL>(_msg, _num, _parameters, _factory) {
         this->setName("varround");
      }

   private:

      bool
      isVarroundAdmissible(const Problem<REAL>& problem, const int& col) const {
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

      ModulStatus
      execute(SolverSettings& settings, Problem<REAL>& problem, Solution<REAL>& solution) override {

         if( solution.status == SolutionStatus::kInfeasible || solution.status == SolutionStatus::kUnbounded )
            return ModulStatus::kNotAdmissible;

         long long batchsize = 1;

         if( this->parameters.nbatches > 0 )
         {
            batchsize = this->parameters.nbatches - 1;
            for( int i = 0; i < problem.getNCols( ); ++i )
               if( isVarroundAdmissible(problem, i) )
                  ++batchsize;
            if( batchsize == this->parameters.nbatches - 1 )
               return ModulStatus::kNotAdmissible;
            batchsize /= this->parameters.nbatches;
         }

         bool admissible = false;
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
               admissible = true;
               REAL lb { this->num.round(copy.getLowerBounds( )[ col ]) };
               REAL ub { this->num.round(copy.getUpperBounds( )[ col ]) };
               if( solution.status == SolutionStatus::kFeasible )
               {
                  REAL value { solution.primal[ col ] };
                  lb = this->num.min(lb, this->num.epsFloor(value));
                  ub = this->num.max(ub, this->num.epsCeil(value));
               }
               if( !this->num.isZetaIntegral(copy.getObjective( ).coefficients[ col ]) )
               {
                  REAL obj { this->num.round(copy.getObjective( ).coefficients[ col ]) };
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

         if( !admissible )
            return ModulStatus::kNotAdmissible;
         if( applied_objectives.empty() && applied_lowers.empty() && applied_uppers.empty() )
            return ModulStatus::kUnsuccesful;
         problem = copy;
         this->nchgcoefs += applied_objectives.size() + applied_lowers.size() + applied_uppers.size();
         return ModulStatus::kSuccessful;
      }
   };

} // namespace bugger

#endif
