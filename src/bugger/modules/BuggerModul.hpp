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

#ifndef _BUGGER_PRESOLVE_METHOD_HPP_
#define _BUGGER_PRESOLVE_METHOD_HPP_

#include "bugger/data/BuggerOptions.hpp"
#include "bugger/io/Message.hpp"
#include "bugger/misc/Num.hpp"
#include "bugger/misc/Vec.hpp"
#include "bugger/misc/fmt.hpp"
#include "bugger/misc/Timer.hpp"
#ifdef BUGGER_TBB
#include "bugger/misc/tbb.hpp"
#else
#include <chrono>
#endif
#include <bitset>

//TODO: ideally the class does not know about SCIP or the solver at all -> use interface?
#ifdef  BUGGER_HAVE_SCIP
#include "scip/cons_linear.h"
#include "scip/scip.h"
#include "scip/scipdefplugins.h"
#include "scip/struct_paramset.h"
#include "scip/def.h"
#endif

namespace bugger
{

   enum class ModulStatus : int
   {
      kDidNotRun = 0,

      kUnsuccesful = 1,

      kSuccessful = 2,

   };

class BuggerModul
{
 public:
   BuggerModul()
   {
      ncalls = 0;
      nsuccessCall = 0;
      name = "unnamed";
      execTime = 0.0;
      enabled = true;
      skip = 0;
   }

   virtual ~BuggerModul() = default;


   virtual bool
   initialize( )
   {
      return false;
   }

   virtual void
   addModuleParameters( ParameterSet& paramSet )
   {
   }

   void
   addParameters( ParameterSet& paramSet )
   {
      paramSet.addParameter(
          fmt::format( "{}.enabled", this->name ).c_str(),
          fmt::format( "is presolver {} enabled", this->name ).c_str(),
          this->enabled );

      addModuleParameters( paramSet );
   }

   ModulStatus
   run( ScipInterface& scip, const BuggerOptions& options, const Timer& timer )
   {
      if( !enabled || delayed )
         return ModulStatus::kDidNotRun;

      if( skip != 0 )
      {
         --skip;
         return ModulStatus::kDidNotRun;
      }

      ++ncalls;

#ifdef BUGGER_TBB
      auto start = tbb::tick_count::now();
#else
      auto start = std::chrono::steady_clock::now();
#endif
      ModulStatus result = execute( scip, options, timer );
#ifdef BUGGER_TBB
      auto end = tbb::tick_count::now();
      auto duration = end - start;
      execTime = execTime + duration.seconds();
#else
      auto end = std::chrono::steady_clock::now();
      execTime = execTime + std::chrono::duration_cast<std::chrono::milliseconds>(
                                end- start ).count()/1000;
#endif


      return result;
   }

   void
   printStats( const Message& message, std::pair<int, int> stats )
   {
      double success =
          ncalls == 0 ? 0.0
                      : ( double( nsuccessCall ) / double( ncalls ) ) * 100.0;
      double applied =
          stats.first == 0
              ? 0.0
              : ( double( stats.second ) / double( stats.first ) ) * 100.0;
      message.info( " {:>18} {:>12} {:>18.1f} {:>18} {:>18.1f} {:>18.3f}\n",
                    name, ncalls, success, stats.first, applied, execTime );
   }


   bool
   isEnabled() const
   {
      return this->enabled;
   }

   bool
   isDelayed() const
   {
      return this->delayed;
   }

   const std::string&
   getName() const
   {
      return this->name;
   }

   unsigned int
   getNCalls() const
   {
      return ncalls;
   }

   void
   setDelayed( bool value )
   {
      this->delayed = value;
   }

   void
   setEnabled( bool value )
   {
      this->enabled = value;
   }

 protected:

   virtual ModulStatus
   execute( ScipInterface& iscip, const BuggerOptions& options, const Timer& timer ) = 0;

   void
   setName( const std::string& value )
   {
      this->name = value;
   }


   static bool
   is_time_exceeded( const Timer& timer,  double tlim)
   {
      return tlim != std::numeric_limits<double>::max() &&
             timer.getTime() >= tlim;
   }


   void
   skipRounds( unsigned int nrounds )
   {
      this->skip += nrounds;
   }

   template <typename LOOP>
   void
   loop( int start, int end, LOOP&& loop_instruction )
   {
#ifdef BUGGER_TBB
      tbb::parallel_for( tbb::blocked_range<int>( start, end ),
                         [&]( const tbb::blocked_range<int>& r )
                         {
                            for( int i = r.begin(); i != r.end(); ++i )
                               loop_instruction( i );
                         } );
#else
      for( int i = 0; i < end; i++ )
      {
         loop_instruction( i );
      }
#endif
   }

      /** tests the given SCIP instance in a copy and reports detected bug; if a primal bug solution is provided, the
    *  resulting dual bound is also checked; on UNIX platforms aborts are caught, hence assertions can be enabled here
    */
      char runSCIP( ScipInterface& iscip )
      {
         SCIP* test = NULL;
         SCIP_HASHMAP* varmap = NULL;
         SCIP_HASHMAP* consmap = NULL;
         char retcode = 1;
         int i;

         //TODO: overload with command line paramter
#ifdef CATCH_ASSERT_BUG
         if( fork() == 0 )
      exit(trySCIP(iscip.getSCIP(), iscip.get_solution(), &test, &varmap, &consmap));
   else
   {
      int status;

      wait(&status);

      if( WIFEXITED(status) )
         retcode = WEXITSTATUS(status);
      else if( WIFSIGNALED(status) )
      {
         retcode = WTERMSIG(status);

         if( retcode == SIGINT )
            retcode = 0;
      }
   }
#else
         retcode = trySCIP(iscip.getSCIP(), iscip.get_solution(), &test, &varmap, &consmap);
#endif

         if( test != NULL )
            SCIPfree(&test);
         if( consmap != NULL )
            SCIPhashmapFree(&consmap);
         if( varmap != NULL )
            SCIPhashmapFree(&varmap);
         SCIPinfoMessage(iscip.getSCIP(), NULL, "\n");

         // TODO: what are passcodes doing?
//         for( i = 0; i < presoldata->npasscodes; ++i )
//            if( retcode == presoldata->passcodes[i] )
//               return 0;

         return retcode;
      }

      /** creates a SCIP instance test, variable map varmap, and constraint map consmap, copies setting, problem, and
 *  solutions apart from the primal bug solution, tries to solve, and reports detected bug
 */
      static
      char trySCIP(
            SCIP*                      scip,          /**< SCIP data structure */
            SCIP_SOL*                  solution,      /**< primal bug solution */
            SCIP**                     test,          /**< pointer to store test instance */
            SCIP_HASHMAP**             varmap,        /**< pointer to store variable map */
            SCIP_HASHMAP**             consmap        /**< pointer to store constraint map */
      )
      {
         SCIP_Real reference = SCIPgetObjsense(scip) * SCIPinfinity(scip);
         SCIP_Bool valid = FALSE;
         SCIP_SOL** sols;
         int nsols;
         int i;

         sols = SCIPgetSols(scip);
         nsols = SCIPgetNSols(scip);
         //TODO: change from SCIP_CALL_RETURN to *_ABORT
         SCIP_CALL_ABORT( SCIPhashmapCreate(varmap, SCIPblkmem(scip), SCIPgetNVars(scip)) );
         SCIP_CALL_ABORT( SCIPhashmapCreate(consmap, SCIPblkmem(scip), SCIPgetNConss(scip)) );
         SCIP_CALL_ABORT( SCIPcreate(test) );
#ifndef SCIP_DEBUG
         SCIPsetMessagehdlrQuiet(*test, TRUE);
#endif
         SCIP_CALL_ABORT( SCIPincludeDefaultPlugins(*test) );
         SCIP_CALL_ABORT( SCIPcopyParamSettings(scip, *test) );
         SCIP_CALL_ABORT( SCIPcopyOrigProb(scip, *test, *varmap, *consmap, SCIPgetProbName(scip)) );
         SCIP_CALL_ABORT( SCIPcopyOrigVars(scip, *test, *varmap, *consmap, NULL, NULL, 0) );
         SCIP_CALL_ABORT( SCIPcopyOrigConss(scip, *test, *varmap, *consmap, FALSE, &valid) );
         //TODO: fix this
//         (*test)->stat->subscipdepth = 0;

         if( !valid )
            SCIP_CALL_ABORT( SCIP_INVALIDDATA );

         for( i = 0; i < nsols; ++i )
         {
            SCIP_SOL* sol;

            sol = sols[i];

            if( sol == solution )
               reference = SCIPgetSolOrigObj(scip, sol);
            else
            {
               SCIP_SOL* initsol = NULL;

               SCIP_CALL_ABORT( SCIPtranslateSubSol(*test, scip, sol, NULL, SCIPgetOrigVars(scip), &initsol) );

               if( initsol == NULL )
                  SCIP_CALL_ABORT( SCIP_INVALIDCALL );

               SCIP_CALL_ABORT( SCIPaddSolFree(*test, &initsol, &valid) );
            }
         }

         SCIP_CALL_ABORT( SCIPsolve(*test) );

         if( SCIPisSumNegative(scip, SCIPgetObjsense(*test) * (reference - SCIPgetDualbound(*test))) )
            SCIP_CALL_ABORT( SCIP_INVALIDRESULT );

         return 0;
      }

 private:
   std::string name;
   double execTime;
   bool enabled;
   bool delayed;
   unsigned int ncalls;
   unsigned int nsuccessCall;
   unsigned int skip;
   };

} // namespace bugger

#endif
