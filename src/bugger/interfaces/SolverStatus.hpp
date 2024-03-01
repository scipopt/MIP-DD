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

#ifndef __BUGGER_INTERFACES_SOLVERSTATUS_HPP__
#define __BUGGER_INTERFACES_SOLVERSTATUS_HPP__


enum class SolverStatus : int {

   kUndefinedError = -1,

   kUnknown = 0,

   kOptimal = 1,

   kInfeasible = 2,

   kInfeasibleOrUnbounded = 3,

   kUnbounded = 4,

   kLimit = 5,

   kNodeLimit = 6,

   kTimeLimit = 7,

   kGapLimit = 8,

   kPrimalLimit = 9,

   kDualLimit = 10,

   kMemLimit = 11,

   kSolLimit = 12,

   kBestSolLimit = 13,

   kRestartLimit = 14,

   kTerminate = 15,

   kStallNodeLimit = 16,

   kTotalNodeLimit = 17,

   kInterrupt = 18,

};

std::ostream &operator<<(std::ostream &out, const SolverStatus status) {
   std::string val;
   switch( status )
   {
      case SolverStatus::kInfeasible:
         val = "infeasible";
         break;
      case SolverStatus::kInfeasibleOrUnbounded:
         val = "infeasible or unbounded";
         break;
      case SolverStatus::kOptimal:
         val = "optimal";
         break;
      case SolverStatus::kUnbounded:
         val = "unbounded";
         break;
      case SolverStatus::kLimit:
         val = "limit";
         break;
      case SolverStatus::kNodeLimit:
         val = "nodelimit";
         break;
      case SolverStatus::kTimeLimit:
         val = "timelimit";
         break;
      case SolverStatus::kGapLimit:
         val = "gaplimit";
         break;
      case SolverStatus::kPrimalLimit:
         val = "primallimit";
         break;
      case SolverStatus::kDualLimit:
         val = "duallimit";
         break;
      case SolverStatus::kUndefinedError:
         val = "ERROR";
         break;
      case SolverStatus::kTotalNodeLimit:
         val = "totalnodelimit";
         break;
      case SolverStatus::kStallNodeLimit:
         val = "stallnodelimit";
         break;
      case SolverStatus::kMemLimit:
         val = "memlimit";
         break;
      case SolverStatus::kInterrupt:
         val = "Interrupt";
         break;
      case SolverStatus::kTerminate:
         val = "Terminate";
         break;
      case SolverStatus::kRestartLimit:
         val = "restartlimit";
         break;
      case SolverStatus::kBestSolLimit:
         val = "bestsollimit";
         break;
      case SolverStatus::kSolLimit:
         val = "sollimit";
         break;
      case SolverStatus::kUnknown:
         val = "unknown";
         break;
      default:
         val = "Error";
         break;
   }
   return out << val;
}


#endif //BUGGER_STATUS_HPP
