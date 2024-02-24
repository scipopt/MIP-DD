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

#ifndef __BUGGER_MODULE_VARIABLE_HPP__
#define __BUGGER_MODULE_VARIABLE_HPP__

#include "bugger/modules/BuggerModul.hpp"


namespace bugger {

   class VariableModul : public BuggerModul {

   public:

      explicit VariableModul(const Message& _msg, const Num<double>& _num, const BuggerParameters& _parameters,
                             std::shared_ptr<SolverFactory>& _factory) : BuggerModul(_msg, _num, _parameters, _factory) {
         this->setName("variable");
      }

   private:

      bool
      isVariableAdmissible(const Problem<double>& problem, const int& col) const {
         return problem.getColFlags( )[ col ].test(ColFlag::kLbInf)
             || problem.getColFlags( )[ col ].test(ColFlag::kUbInf)
             || !num.isZetaEq(problem.getLowerBounds( )[ col ], problem.getUpperBounds( )[ col ]);
      }

      ModulStatus
      execute(SolverSettings& settings, Problem<double>& problem, Solution<double>& solution) override {

         if( solution.status == SolutionStatus::kUnbounded )
            return ModulStatus::kNotAdmissible;

         int batchsize = 1;

         if( parameters.nbatches > 0 )
         {
            batchsize = parameters.nbatches - 1;
            for( int i = problem.getNCols() - 1; i >= 0; --i )
               if( isVariableAdmissible(problem, i) )
                  ++batchsize;
            if( batchsize == parameters.nbatches - 1 )
               return ModulStatus::kNotAdmissible;
            batchsize /= parameters.nbatches;
         }

         bool admissible = false;
         auto copy = Problem<double>(problem);
         Vec<std::pair<int, double>> applied_reductions { };
         Vec<std::pair<int, double>> batches { };
         batches.reserve(batchsize);

         for( int col = copy.getNCols() - 1; col >= 0; --col )
         {
            if( isVariableAdmissible(copy, col) )
            {
               double fixedval;
               admissible = true;
               if( solution.status == SolutionStatus::kFeasible )
               {
                  fixedval = solution.primal[ col ];
                  if( copy.getColFlags( )[ col ].test(ColFlag::kIntegral) )
                     fixedval = num.round(fixedval);
               }
               else
               {
                  fixedval = 0.0;
                  if( copy.getColFlags( )[ col ].test(ColFlag::kIntegral) )
                  {
                     if( !copy.getColFlags( )[ col ].test(ColFlag::kUbInf) )
                        fixedval = num.min(fixedval, num.epsFloor(copy.getUpperBounds( )[ col ]));
                     if( !copy.getColFlags( )[ col ].test(ColFlag::kLbInf) )
                        fixedval = num.max(fixedval, num.epsCeil(copy.getLowerBounds( )[ col ]));
                  }
                  else
                  {
                     if( !copy.getColFlags( )[ col ].test(ColFlag::kUbInf) )
                        fixedval = num.min(fixedval, copy.getUpperBounds( )[ col ]);
                     if( !copy.getColFlags( )[ col ].test(ColFlag::kLbInf) )
                        fixedval = num.max(fixedval, copy.getLowerBounds( )[ col ]);
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
               if( call_solver(settings, copy, solution) == BuggerStatus::kOkay )
               {
                  copy = Problem<double>(problem);
                  for( const auto &item: applied_reductions )
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

         if( !admissible )
            return ModulStatus::kNotAdmissible;
         if( applied_reductions.empty() )
            return ModulStatus::kUnsuccesful;
         problem = copy;
         nfixedvars += applied_reductions.size();
         return ModulStatus::kSuccessful;
      }
   };

} // namespace bugger

#endif
