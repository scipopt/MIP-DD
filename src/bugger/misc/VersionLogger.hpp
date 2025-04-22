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

#ifndef _BUGGER_MISC_VERSION_LOGGER_HPP_
#define _BUGGER_MISC_VERSION_LOGGER_HPP_


namespace bugger
{
template <typename REAL>
void
print_header()
{
   fmt::print( "MIP-DD version {}.{}.{} ", BUGGER_VERSION_MAJOR, BUGGER_VERSION_MINOR, BUGGER_VERSION_PATCH);

   fmt::print("[arithmetic: {}]", typeid(REAL).name());

   fmt::print("[precision: {} bytes]", sizeof(REAL));

#if defined(__INTEL_COMPILER)
   fmt::print("[Compiler: Intel {}]", __INTEL_COMPILER);
#elif defined(__clang__)
   fmt::print("[Compiler: clang {}.{}.{}]", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(_MSC_VER)
   fmt::print("[Compiler: microsoft visual c {}]", _MSC_FULL_VER);
#elif defined(__GNUC__)
#if defined(__GNUC_PATCHLEVEL__)
   fmt::print("[Compiler: gcc {}.{}.{}]", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
   fmt::print("[Compiler: gcc {}.{}]", __GNUC__, __GNUC_MINOR__);
#endif
#else
   fmt::print("[Compiler: unknown]");
#endif

#ifdef NDEBUG
   fmt::print("[mode: optimized]");
#else
   fmt::print("[mode: debug]");
#endif

#ifdef BUGGER_GITHASH_AVAILABLE
   fmt::print("[GitHash: {}]", BUGGER_GITHASH);
#endif

   fmt::print( "\n" );
   fmt::print( "Copyright (C) 2024 Zuse Institute Berlin (ZIB)\n" );
   fmt::print( "\n" );

   fmt::print( "External libraries: \n" );

   fmt::print( "  Boost    {}.{}.{} \t (https://www.boost.org/)\n",
               BOOST_VERSION_NUMBER_MINOR( BOOST_VERSION ),
               BOOST_VERSION_NUMBER_PATCH( BOOST_VERSION ) / 100,
               BOOST_VERSION_NUMBER_MAJOR( BOOST_VERSION ) );

#ifdef BUGGER_TBB
   fmt::print( "  TBB            \t Thread building block https://github.com/oneapi-src/oneTBB developed by Intel\n" );
#endif
   fmt::print( "\n" );
}

} // namespace bugger

#endif
