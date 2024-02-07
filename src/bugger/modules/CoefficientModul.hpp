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

      bool isFixingAdmissible(const Problem<double>& problem, int col) {
         return !problem.getColFlags( )[ col ].test(ColFlag::kFixed)
             && !problem.getColFlags( )[ col ].test(ColFlag::kLbInf)
             && !problem.getColFlags( )[ col ].test(ColFlag::kUbInf)
             && num.isZetaEq(problem.getLowerBounds( )[ col ], problem.getUpperBounds( )[ col ]);
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
         Vec<std::pair<int, double>> applied_lefts { };
         Vec<std::pair<int, double>> applied_rights { };
         Vec<MatrixEntry<double>> batches_coeff { };
         Vec<std::pair<int, double>> batches_lhs { };
         Vec<std::pair<int, double>> batches_rhs { };
         batches_lhs.reserve(batchsize);
         batches_rhs.reserve(batchsize);
         int batch = 0;

         for( int row = copy.getNRows( ) - 1; row >= 0; --row )
         {
            if( isCoefficientAdmissible(copy, row) )
            {
               admissible = true;
               auto data = copy.getConstraintMatrix( ).getRowCoefficients(row);
               bool integral = true;
               double offset = 0.0;
               for( int index = data.getLength( ) - 1; index >= 0; --index )
               {
                  int col = data.getIndices( )[ index ];
                  double val = data.getValues( )[ index ];
                  if( !num.isZetaZero(val) && isFixingAdmissible(copy, col) )
                  {
                     double fixedval;
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
                     offset -= val * fixedval;
                     batches_coeff.emplace_back(row, col, 0.0);
                  }
                  else if( !copy.getColFlags( )[ col ].test(ColFlag::kFixed) && ( !copy.getColFlags( )[ col ].test(ColFlag::kIntegral) || !num.isEpsIntegral(val) ) )
                     integral = false;
               }
               if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf) )
               {
                  double lhs = copy.getConstraintMatrix( ).getLeftHandSides( )[ row ] + offset;
                  if( integral )
                     lhs = num.round(lhs);
                  if( !num.isZetaEq(copy.getConstraintMatrix( ).getLeftHandSides( )[ row ], lhs) )
                  {
                     copy.getConstraintMatrix( ).modifyLeftHandSide(row, num, lhs);
                     batches_lhs.emplace_back(row, lhs);
                  }
               }
               if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf) )
               {
                  double rhs = copy.getConstraintMatrix( ).getRightHandSides( )[ row ] + offset;
                  if( integral )
                     rhs = num.round(rhs);
                  if( !num.isZetaEq(copy.getConstraintMatrix( ).getRightHandSides( )[ row ], rhs) )
                  {
                     copy.getConstraintMatrix( ).modifyRightHandSide(row, num, rhs);
                     batches_rhs.emplace_back(row, rhs);
                  }
               }
               ++batch;
            }

            if( batch >= 1 && ( batch >= batchsize || row <= 0 ) )
            {
               apply_changes(copy, batches_coeff);
               auto solver = createSolver();
               solver->doSetUp(settings, copy, solution);
               if( !options.debug_filename.empty( ))
                  solver->writeInstance(options.debug_filename, settings, copy, false);
               if( call_solver(solver.get( ), msg, options) == BuggerStatus::kOkay )
               {
                  copy = Problem<double>(problem);
                  apply_changes(copy, applied_entries);
                  for( const auto &item: applied_lefts )
                     copy.getConstraintMatrix( ).modifyLeftHandSide( item.first, num, item.second );
                  for( const auto &item: applied_rights )
                     copy.getConstraintMatrix( ).modifyRightHandSide( item.first, num, item.second );
               }
               else
               {
                  applied_entries.insert(applied_entries.end(), batches_coeff.begin(), batches_coeff.end());
                  applied_lefts.insert(applied_lefts.end(), batches_lhs.begin(), batches_lhs.end());
                  applied_rights.insert(applied_rights.end(), batches_rhs.begin(), batches_rhs.end());
               }
               batches_coeff.clear();
               batches_lhs.clear();
               batches_rhs.clear();
               batch = 0;
            }
         }

         if( !admissible )
            return ModulStatus::kNotAdmissible;
         if( applied_entries.empty() )
            return ModulStatus::kUnsuccesful;
         problem = copy;
         nchgcoefs += applied_entries.size();
         nchgsides += applied_lefts.size() + applied_rights.size();
         return ModulStatus::kSuccessful;
      }
   };

} // namespace bugger

#endif
