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

#ifndef BUGGER_MODUL_FIXING_HPP_
#define BUGGER_MODUL_FIXING_HPP_

#include "bugger/modules/BuggerModul.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"

namespace bugger {

   class FixingModul : public BuggerModul {
   public:
      FixingModul( const Message &_msg, const Num<double> &_num, std::shared_ptr<SolverFactory>& factory) : BuggerModul(factory) {
         this->setName("fixing");
         this->msg = _msg;
         this->num = _num;

      }

      bool
      initialize( ) override {
         return false;
      }

      bool isFixingAdmissible(const Problem<double>& problem, int var) {
         return !problem.getColFlags( )[ var ].test(ColFlag::kFixed)
             && !problem.getColFlags( )[ var ].test(ColFlag::kLbInf)
             && !problem.getColFlags( )[ var ].test(ColFlag::kUbInf)
             && num.isZetaEq(problem.getLowerBounds( )[ var ], problem.getUpperBounds( )[ var ]);
      }

      ModulStatus
      execute(Problem<double> &problem, SolverSettings& settings, Solution<double> &solution,
              const BuggerOptions &options, const Timer &timer) override {

         int batchsize = 1;

         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;
            for( int var = problem.getNCols( ) - 1; var >= 0; --var )
               if( isFixingAdmissible(problem, var) )
                  ++batchsize;
            if( batchsize == options.nbatches - 1 )
               return ModulStatus::kNotAdmissible;
            batchsize /= options.nbatches;
         }

         bool admissible = false;
         auto copy = Problem<double>(problem);
         Vec<int> applied_vars { };
         MatrixBuffer<double> applied_entries { };
         Vec<std::pair<int, double>> applied_reductions { };
         Vec<int> batches_vars { };
         MatrixBuffer<double> batches_coeff { };
         Vec<std::pair<int, double>> batches_offset { };
         batches_vars.reserve(batchsize);

         for( int var = copy.getNCols( ) - 1; var >= 0; --var )
         {
            if( isFixingAdmissible(copy, var) )
            {
               admissible = true;
               auto data = copy.getConstraintMatrix( ).getColumnCoefficients(var);
               double fixedval;

               if( solution.status == SolutionStatus::kFeasible )
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

               for( int index = data.getLength( ) - 1; index >= 0; --index )
               {
                  int row = data.getIndices( )[ index ];
                  if( !num.isZetaZero(data.getValues( )[ index ]) && !copy.getConstraintMatrix( ).getRowFlags( )[ row ].test(RowFlag::kRedundant) )
                  {
                     double offset = -data.getValues( )[ index ] * fixedval;

                     batches_coeff.addEntry(row, var, 0.0);
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf) )
                        copy.getConstraintMatrix( ).modifyLeftHandSide( row, num, copy.getConstraintMatrix( ).getLeftHandSides( )[ row ] + offset );
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf) )
                        copy.getConstraintMatrix( ).modifyRightHandSide( row, num, copy.getConstraintMatrix( ).getRightHandSides( )[ row ] + offset );
                     batches_offset.emplace_back(row, offset);
                  }
               }

               assert(!copy.getColFlags( )[ var ].test(ColFlag::kFixed));
               copy.getColFlags( )[ var ].set(ColFlag::kFixed);
               batches_vars.push_back(var);
            }

            if( !batches_vars.empty() && ( batches_vars.size() >= batchsize || var <= 0 ) )
            {
               copy.getConstraintMatrix( ).changeCoefficients(batches_coeff);
               auto solver = createSolver();
               solver->doSetUp(settings, copy, solution);
               if( call_solver(solver.get( ), msg, options) == BuggerStatus::kOkay )
               {
                  copy = Problem<double>(problem);
                  copy.getConstraintMatrix( ).changeCoefficients(applied_entries);
                  for( const auto &item: applied_reductions )
                  {
                     if( !copy.getRowFlags( )[ item.first ].test(RowFlag::kLhsInf) )
                        copy.getConstraintMatrix( ).modifyLeftHandSide( item.first, num, copy.getConstraintMatrix( ).getLeftHandSides( )[ item.first ] + item.second );
                     if( !copy.getRowFlags( )[ item.first ].test(RowFlag::kRhsInf) )
                        copy.getConstraintMatrix( ).modifyRightHandSide( item.first, num, copy.getConstraintMatrix( ).getRightHandSides( )[ item.first ] + item.second );
                  }
                  for( const auto &item: applied_vars )
                  {
                     assert(!copy.getColFlags( )[ item ].test(ColFlag::kFixed));
                     copy.getColFlags( )[ item ].set(ColFlag::kFixed);
                  }
               }
               else
               {
                  SmallVec<int, 32> buffer;
                  const MatrixEntry<double> *iter = batches_coeff.template begin<true>(buffer);
                  while( iter != batches_coeff.end( ) )
                  {
                     applied_entries.addEntry(iter->row, iter->col, iter->val);
                     iter = batches_coeff.template next<true>( buffer );
                  }
                  applied_reductions.insert(applied_reductions.end(), batches_offset.begin(), batches_offset.end());
                  applied_vars.insert(applied_vars.end(), batches_vars.begin(), batches_vars.end());
               }
               batches_coeff.clear();
               batches_offset.clear();
               batches_vars.clear();
            }
         }

         if(!admissible)
            return ModulStatus::kNotAdmissible;
         if( applied_vars.empty() )
            return ModulStatus::kUnsuccesful;
         else
         {
            problem = copy;
            naggrvars += applied_vars.size();
            return ModulStatus::kSuccessful;
         }
      }
   };

} // namespace bugger

#endif
