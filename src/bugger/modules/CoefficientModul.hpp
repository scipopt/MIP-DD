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
      CoefficientModul(const std::string& _setting, const Message &_msg, const Num<double> &_num) : BuggerModul(_setting) {
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
      execute(Problem<double> &problem, Solution<double> &solution, bool solution_exists, const BuggerOptions &options,
              const SolverSettings& settings, const Timer &timer) override {

         auto copy = Problem<double>(problem);
         MatrixBuffer<double> applied_entries { };
         Vec<std::pair<int, double>> applied_reductions { };
         MatrixBuffer<double> batches_coeff { };
         Vec<std::pair<int, double>> batches_offset { };
         int batchsize = 1;
         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;
            for( int i = problem.getNRows( ) - 1; i >= 0; --i )
               if( isCoefficientAdmissible(problem, i) )
                  ++batchsize;
            batchsize /= options.nbatches;
         }

         batches_offset.reserve(batchsize);
         bool admissible = false;

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

                     offset -= data.getValues( )[ index ] * fixedval;
                     batches_coeff.addEntry(row, var, 0.0);
                  }
               }

               if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf) )
                  copy.getConstraintMatrix( ).modifyLeftHandSide( row, num, copy.getConstraintMatrix( ).getLeftHandSides( )[ row ] + offset );
               if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf) )
                  copy.getConstraintMatrix( ).modifyRightHandSide( row, num, copy.getConstraintMatrix( ).getRightHandSides( )[ row ] + offset );
               batches_offset.push_back({ row, offset });
            }

            if( !batches_offset.empty() && ( batches_offset.size() >= batchsize || row <= 0 ) )
            {
               copy.getConstraintMatrix( ).changeCoefficients(batches_coeff);
               auto solver = createSolver();
//               solver->parseParameters();
               solver->doSetUp(copy, solution_exists, solution);
               if( solver->run(msg, originalSolverStatus, settings) == BuggerStatus::kSuccess )
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
               }
               batches_coeff.clear();
               batches_offset.clear();
            }
         }

         if(!admissible)
            return ModulStatus::kDidNotRun;
         if( applied_reductions.empty() )
            return ModulStatus::kUnsuccesful;
         else
         {
            problem = copy;
            nchgcoefs += applied_entries.getNnz();
            nchgsides += 2 * applied_reductions.size();
            return ModulStatus::kSuccessful;
         }
      }
   };

} // namespace bugger

#endif
