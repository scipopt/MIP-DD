/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*    BUGGER                                                                 */
/*                                                                           */
/* Copyright (C) 2023             Konrad-Zuse-Zentrum                        */
/*                     fuer Informationstechnik Berlin                       */
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

#ifndef _BUGGER_MODUL_VARIABLE_HPP_
#define _BUGGER_MODUL_VARIABLE_HPP_

#include "bugger/modules/BuggerModul.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"

namespace bugger {

   class VariableModul : public BuggerModul {
   public:
      VariableModul( const Message &_msg, const Num<double> &_num, const SolverStatus& _status) : BuggerModul() {
         this->setName("variable");
         this->msg = _msg;
         this->num = _num;
         this->originalSolverStatus = _status;
      }

      bool
      initialize( ) override {
         return false;
      }

      bool isVariableAdmissible(const Problem<double>& problem, int var) {
         return problem.getColFlags( )[ var ].test(ColFlag::kLbInf)
             || problem.getColFlags( )[ var ].test(ColFlag::kUbInf)
             || !num.isZetaEq(problem.getLowerBounds( )[ var ], problem.getUpperBounds( )[ var ]);
      }

      ModulStatus
      execute(Problem<double> &problem, SolverSettings& settings, Solution<double>& solution, bool solution_exists,
              const BuggerOptions &options,  const Timer &timer) override {

         auto copy = Problem<double>(problem);
         Vec<std::pair<int, double>> applied_reductions { };
         Vec<std::pair<int, double>> batches { };
         int batchsize = 1;

         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;
            for( int i = problem.getNCols() - 1; i >= 0; --i )
               if( isVariableAdmissible(problem, i) )
                  ++batchsize;
            if( batchsize == options.nbatches - 1 )
               return ModulStatus::kNotAdmissible;
            batchsize /= options.nbatches;
         }

         batches.reserve(batchsize);
         bool admissible = false;

         for( int var = copy.getNCols() - 1; var >= 0; --var )
         {
            if( isVariableAdmissible(copy, var) )
            {
               double fixedval;
               admissible = true;
               if( solution_exists )
               {
                  fixedval = solution.primal[ var ];
                  if( copy.getColFlags( )[ var ].test(ColFlag::kIntegral) )
                     fixedval = num.round(fixedval);
               }
               else
               {
                  fixedval = 0.0;
                  if( copy.getColFlags( )[ var ].test(ColFlag::kIntegral) )
                  {
                     if( !copy.getColFlags( )[ var ].test(ColFlag::kUbInf) )
                        fixedval = num.min(fixedval, num.epsFloor(copy.getUpperBounds( )[ var ]));
                     if( !copy.getColFlags( )[ var ].test(ColFlag::kLbInf) )
                        fixedval = num.max(fixedval, num.epsCeil(copy.getLowerBounds( )[ var ]));
                  }
                  else
                  {
                     if( !copy.getColFlags( )[ var ].test(ColFlag::kUbInf) )
                        fixedval = num.min(fixedval, copy.getUpperBounds( )[ var ]);
                     if( !copy.getColFlags( )[ var ].test(ColFlag::kLbInf) )
                        fixedval = num.max(fixedval, copy.getLowerBounds( )[ var ]);
                  }
               }

               copy.getColFlags( )[ var ].unset(ColFlag::kLbInf);
               copy.getColFlags( )[ var ].unset(ColFlag::kUbInf);
               copy.getLowerBounds( )[ var ] = fixedval;
               copy.getUpperBounds( )[ var ] = fixedval;
               batches.push_back({ var, fixedval });
            }

            if( !batches.empty() && ( batches.size() >= batchsize || var <= 0 ) )
            {
               auto solver = createSolver();
               solver->doSetUp(copy,  settings, solution_exists, solution);
               if( call_solver(solver.get(), msg, originalSolverStatus, settings) == BuggerStatus::kNotReproduced)
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
         if(!admissible)
            return ModulStatus::kNotAdmissible;
         if( applied_reductions.empty() )
            return ModulStatus::kUnsuccesful;
         else
         {
            problem = copy;
            nfixedvars += applied_reductions.size();
            return ModulStatus::kSuccessful;
         }
      }
   };

} // namespace bugger

#endif
