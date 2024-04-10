/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*                            MIP-DD                                         */
/*                                                                           */
/* Copyright (C) 2024             Zuse Institute Berlin                      */
/*                                                                           */
/*  Licensed under the Apache License, Version 2.0 (the "License");          */
/*  you may not use this file except in compliance with the License.         */
/*  You may obtain a copy of the License at                                  */
/*                                                                           */
/*      http://www.apache.org/licenses/LICENSE-2.0                           */
/*                                                                           */
/*  Unless required by applicable law or agreed to in writing, software      */
/*  distributed under the License is distributed on an "AS IS" BASIS,        */
/*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. */
/*  See the License for the specific language governing permissions and      */
/*  limitations under the License.                                           */
/*                                                                           */
/*  You should have received a copy of the Apache-2.0 license                */
/*  along with MIP-DD; see the file LICENSE. If not visit scipopt.org.       */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef __BUGGER_INTERFACES_SOLVERSTATUS_HPP__
#define __BUGGER_INTERFACES_SOLVERSTATUS_HPP__

template<typename EnumType, EnumType... Values>
class EnumCheck;

template<typename EnumType>
class EnumCheck<EnumType>
{
public:
   template<typename IntType>
   static bool constexpr is_value(IntType) { return false; }
};

template<typename EnumType, EnumType V, EnumType... Next>
class EnumCheck<EnumType, V, Next...> : private EnumCheck<EnumType, Next...>
{
   using super = EnumCheck<EnumType, Next...>;

public:
   template<typename IntType>
   static bool constexpr is_value(IntType v)
   {
      return v == static_cast<IntType>(V) || super::is_value(v);
   }
};

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

   kCertificateCouldNotBeValidated = -19,

};

using SolverStatusCheck = EnumCheck< SolverStatus,
      SolverStatus::kUndefinedError,
      SolverStatus::kUnknown,
      SolverStatus::kOptimal,
      SolverStatus::kInfeasible,
      SolverStatus::kInfeasibleOrUnbounded,
      SolverStatus::kUnbounded,
      SolverStatus::kLimit,
      SolverStatus::kNodeLimit,
      SolverStatus::kTimeLimit,
      SolverStatus::kGapLimit,
      SolverStatus::kPrimalLimit,
      SolverStatus::kDualLimit,
      SolverStatus::kMemLimit,
      SolverStatus::kSolLimit,
      SolverStatus::kBestSolLimit,
      SolverStatus::kRestartLimit,
      SolverStatus::kTerminate,
      SolverStatus::kStallNodeLimit,
      SolverStatus::kTotalNodeLimit,
      SolverStatus::kInterrupt,
      SolverStatus::kCertificateCouldNotBeValidated
      >;

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
         val = "interrupt";
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
         // the function call_solver should ensure that only a known status is processed
         assert(false);
         val = "Error";
         break;
   }
   return out << val;
}


#endif //BUGGER_STATUS_HPP
