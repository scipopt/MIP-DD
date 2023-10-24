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

#ifndef BUGGER_MODUL_VARROUND_HPP_
#define BUGGER_MODUL_VARROUND_HPP_

#include "bugger/modules/BuggerModul.hpp"

namespace bugger {

   class VarroundModul : public BuggerModul {
   public:
      VarroundModul( ) : BuggerModul( ) {
         this->setName("varround");
      }

      bool
      initialize( ) override {
         return false;
      }

      bool SCIPisVarroundAdmissible(const Problem<double> &problem, int var) {
         /* leave sparkling or fixed variables */
         double obj = problem.getObjective( ).coefficients[ var ];
         return num.isIntegral(obj) || problem.getColFlags( )[ var ].test(ColFlag::kUbInf) ||
                problem.getColFlags( )[ var ].test(ColFlag::kLbInf) ||
                ( !num.isEq(problem.getLowerBounds( )[ var ], problem.getLowerBounds( )[ var ]) &&
                  ( num.isIntegral(problem.getLowerBounds( )[ var ]) ||
                    num.isIntegral(problem.getUpperBounds( )[ var ])));
      }

      ModulStatus
      execute(Problem<double> &problem, Solution<double> &solution, bool solution_exists, const BuggerOptions &options,
              const Timer &timer) override {

         ModulStatus result = ModulStatus::kUnsuccesful;

         auto copy = Problem<double>(problem);
         Vec<std::pair<int, double>> applied_lb { };
         Vec<std::pair<int, double>> applied_ub { };
         Vec<std::pair<int, double>> applied_obj { };
         Vec<std::pair<int, double>> batches_lb { };
         Vec<std::pair<int, double>> batches_ub { };
         Vec<std::pair<int, double>> batches_obj { };

         int batchsize = 1;
         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;
            for( int i = 0; i < problem.getNCols( ); ++i )
               if( SCIPisVarroundAdmissible(problem, i))
                  ++batchsize;

            batchsize /= options.nbatches;
         }

         int nbatch = 0;
         for( int var = 0; var < copy.getNCols( ); ++var )
         {
            if( SCIPisVarroundAdmissible(copy, var))
            {
               if( num.isIntegral(copy.getObjective( ).coefficients[ var ]))
               {
                  copy.getObjective( ).coefficients[ var ] = num.round(copy.getObjective( ).coefficients[ var ]);
                  batches_obj.emplace_back(var, copy.getObjective( ).coefficients[ var ]);
               }
               if( solution_exists )
               {
                  if( num.isIntegral(copy.getLowerBounds( )[ var ]))
                  {
                     copy.getLowerBounds( )[ var ] = num.round(copy.getLowerBounds( )[ var ]);
                     batches_lb.emplace_back(var, copy.getLowerBounds( )[ var ]);

                  }
                  if( num.isIntegral(copy.getUpperBounds( )[ var ]))
                  {
                     copy.getUpperBounds( )[ var ] = num.round(copy.getUpperBounds( )[ var ]);
                     batches_ub.emplace_back(var, copy.getUpperBounds( )[ var ]);
                  }
               }
               else
               {
                  if( num.isIntegral(copy.getLowerBounds( )[ var ]))
                  {
                     copy.getLowerBounds( )[ var ] = MIN(num.round(copy.getLowerBounds( )[ var ]),
                                                         num.epsFloor(solution.primal[ var ]));
                     batches_lb.emplace_back(var, copy.getLowerBounds( )[ var ]);

                  }
                  if( num.isIntegral(copy.getUpperBounds( )[ var ]))
                  {
                     copy.getUpperBounds( )[ var ] = MIN(num.round(copy.getUpperBounds( )[ var ]),
                                                         num.epsCeil(solution.primal[ var ]));
                     batches_ub.emplace_back(var, copy.getUpperBounds( )[ var ]);
                  }
               }
               ++nbatch;
            }

            if( nbatch >= 1 && ( nbatch >= batchsize || var >= copy.getNCols( ) - 1 ))
            {
               ScipInterface scipInterface { };
               //TODO pass settings to SCIP
               scipInterface.doSetUp(copy);
               if( scipInterface.runSCIP( ) != Status::kSuccess )
               {
                  copy = Problem<double>(problem);
                  for( const auto &item: applied_lb )
                     problem.getLowerBounds( )[ item.first ] = item.second;
                  for( const auto &item: applied_ub )
                     problem.getUpperBounds( )[ item.first ] = item.second;
                  for( const auto &item: applied_obj )
                     problem.getObjective( ).coefficients[ item.first ] = item.second;

               }
               else
               {
                  //TODO: push back together
                  for( const auto &item: batches_lb )
                     applied_lb.push_back(item);
                  for( const auto &item: batches_obj )
                     applied_obj.push_back(item);
                  for( const auto &item: batches_ub )
                     applied_ub.push_back(item);
                  nchgcoefs += batches_lb.size() + batches_ub.size() + batches_lb.size();
                  batches_lb.clear();
                  batches_ub.clear();
                  batches_obj.clear();
                  result = ModulStatus::kSuccessful;
               }
               nbatch = 0;
            }
         }
         problem = Problem<double>(copy);
         return result;
      }
   };


} // namespace bugger

#endif
