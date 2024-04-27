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

#ifndef __BUGGER_MODULE_OBJECTIVE_HPP__
#define __BUGGER_MODULE_OBJECTIVE_HPP__

#include "bugger/modules/BuggerModul.hpp"


namespace bugger {

   template <typename REAL>
   class ObjectiveModul : public BuggerModul<REAL> {

   public:

      explicit ObjectiveModul(const Message& _msg, const Num<REAL>& _num, const BuggerParameters& _parameters,
                              std::shared_ptr<SolverFactory<REAL>>& _factory)
                              : BuggerModul<REAL>(_msg, _num, _parameters, _factory) {
         this->setName("objective");
      }

   private:

      bool
      isObjectiveAdmissible(const Problem<REAL>& problem, const int& col) const {
         return !this->num.isZetaZero(problem.getObjective( ).coefficients[ col ])
           && ( problem.getColFlags( )[ col ].test(ColFlag::kLbInf)
             || problem.getColFlags( )[ col ].test(ColFlag::kUbInf)
             || !this->num.isZetaEq(problem.getLowerBounds( )[ col ], problem.getUpperBounds( )[ col ]) );
      }

      ModulStatus
      execute(SolverSettings& settings, Problem<REAL>& problem, Solution<REAL>& solution) override {

         if( solution.status == SolutionStatus::kUnbounded )
            return ModulStatus::kNotAdmissible;

         long long batchsize = 1;

         if( this->parameters.nbatches > 0 )
         {
            batchsize = this->parameters.nbatches - 1;
            for( int i = problem.getNCols( ) - 1; i >= 0; --i )
               if( isObjectiveAdmissible(problem, i) )
                  ++batchsize;
            if( batchsize == this->parameters.nbatches - 1 )
               return ModulStatus::kNotAdmissible;
            batchsize /= this->parameters.nbatches;
         }

         bool admissible = false;
         auto copy = Problem<REAL>(problem);
         Vec<int> applied_reductions { };
         Vec<int> batches { };
         batches.reserve(batchsize);

         for( int col = copy.getNCols( ) - 1; col >= 0; --col )
         {
            if( isObjectiveAdmissible(copy, col) )
            {
               admissible = true;
               copy.getObjective( ).coefficients[ col ] = 0.0;
               batches.push_back(col);
            }

            if( !batches.empty() && ( batches.size() >= batchsize || col <= 0 ) )
            {
               if( this->call_solver(settings, copy, solution) == BuggerStatus::kOkay )
               {
                  copy = Problem<REAL>(problem);
                  for( const auto &item: applied_reductions )
                     copy.getObjective( ).coefficients[ item ] = 0.0;
               }
               else
                  applied_reductions.insert(applied_reductions.end(), batches.begin(), batches.end());
               batches.clear();
            }
         }

         if( !admissible )
            return ModulStatus::kNotAdmissible;
         if( applied_reductions.empty() )
            return ModulStatus::kUnsuccesful;
         problem = copy;
         this->nchgcoefs += applied_reductions.size();
         return ModulStatus::kSuccessful;
      }
   };

} // namespace bugger

#endif
