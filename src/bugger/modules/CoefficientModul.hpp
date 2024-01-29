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

#ifndef BUGGER_MODUL_COEFFICIENT_HPP_
#define BUGGER_MODUL_COEFFICIENT_HPP_

#include "bugger/modules/BuggerModul.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"

namespace bugger {

   class CoefficientModul : public BuggerModul {
   public:
      CoefficientModul( const Message &_msg, const Num<double> &_num, std::shared_ptr<SolverFactory>& factory) : BuggerModul(factory) {
         this->setName("coefficient");
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

      bool isCoefficientAdmissible(const Problem<double>& problem, int row) {
         if( problem.getConstraintMatrix( ).getRowFlags( )[ row ].test(RowFlag::kRedundant) )
            return false;
         auto data = problem.getConstraintMatrix( ).getRowCoefficients(row);
         for( int index = 0; index < data.getLength( ); ++index )
            if( !num.isZetaZero(data.getValues( )[ index ]) && isFixingAdmissible(problem, data.getIndices( )[ index ]) )
               return true;
         return false;
      }

      ModulStatus
      execute(Problem<double> &problem, SolverSettings& settings, Solution<double> &solution,
              const BuggerOptions &options, const Timer &timer) override {

         int batchsize = 1;

         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;
            for( int i = problem.getNRows( ) - 1; i >= 0; --i )
               if( isCoefficientAdmissible(problem, i) )
                  ++batchsize;
            if( batchsize == options.nbatches - 1 )
               return ModulStatus::kNotAdmissible;
            batchsize /= options.nbatches;
         }

         bool admissible = false;
         auto copy = Problem<double>(problem);
         Vec<MatrixEntry<double>> applied_entries { };
         Vec<std::pair<int, double>> applied_reductions { };
         Vec<MatrixEntry<double>> batches_coeff { };
         Vec<std::pair<int, double>> batches_offset { };
         batches_offset.reserve(batchsize);

         for( int row = copy.getNRows( ) - 1; row >= 0; --row )
         {
            if( isCoefficientAdmissible(copy, row) )
            {
               admissible = true;
               auto data = copy.getConstraintMatrix( ).getRowCoefficients(row);
               double offset = 0.0;

               for( int index = data.getLength( ) - 1; index >= 0; --index )
               {
                  int var = data.getIndices( )[ index ];
                  if( !num.isZetaZero(data.getValues( )[ index ]) && isFixingAdmissible(copy, var) )
                  {
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

                     offset -= data.getValues( )[ index ] * fixedval;
                     batches_coeff.emplace_back(row, var, 0.0);
                  }
               }

               if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf) )
                  copy.getConstraintMatrix( ).modifyLeftHandSide( row, num, copy.getConstraintMatrix( ).getLeftHandSides( )[ row ] + offset );
               if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf) )
                  copy.getConstraintMatrix( ).modifyRightHandSide( row, num, copy.getConstraintMatrix( ).getRightHandSides( )[ row ] + offset );
               batches_offset.emplace_back(row, offset);
            }

            if( !batches_offset.empty() && ( batches_offset.size() >= batchsize || row <= 0 ) )
            {
               MatrixBuffer<double> matrixBuffer{ };
               for(auto entry: batches_coeff)
                  matrixBuffer.addEntry(entry.row, entry.col, entry.val);
               copy.getConstraintMatrix( ).changeCoefficients(matrixBuffer);
               auto solver = createSolver();
               solver->doSetUp(copy,  settings, solution );
               if( call_solver(solver.get( ), msg, options) == BuggerStatus::kOkay )
               {
                  copy = Problem<double>(problem);
                  MatrixBuffer<double> matrixBuffer2{ };
                  for(auto entry: applied_entries)
                     matrixBuffer2.addEntry(entry.row, entry.col, entry.val);
                  copy.getConstraintMatrix( ).changeCoefficients(matrixBuffer2);
                  for( const auto &item: applied_reductions )
                  {
                     if( !copy.getRowFlags( )[ item.first ].test(RowFlag::kLhsInf) )
                        copy.getConstraintMatrix( ).modifyLeftHandSide( item.first, num, copy.getConstraintMatrix( ).getLeftHandSides( )[ item.first ] + item.second );
                     if( !copy.getRowFlags( )[ item.first ].test(RowFlag::kRhsInf) )
                        copy.getConstraintMatrix( ).modifyRightHandSide( item.first, num, copy.getConstraintMatrix( ).getRightHandSides( )[ item.first ] + item.second );
                  }
               }
               else
               {
                  applied_reductions.insert(applied_reductions.end(), batches_offset.begin(), batches_offset.end());
                  applied_entries.insert(applied_entries.end(), batches_coeff.begin(), batches_coeff.end());
               }
               batches_coeff.clear();
               batches_offset.clear();
            }
         }

         if(!admissible)
            return ModulStatus::kNotAdmissible;
         if( applied_reductions.empty() )
            return ModulStatus::kUnsuccesful;
         else
         {
            problem = copy;
            nchgcoefs += applied_entries.size();
            nchgsides += 2 * applied_reductions.size();
            return ModulStatus::kSuccessful;
         }
      }
   };

} // namespace bugger

#endif
