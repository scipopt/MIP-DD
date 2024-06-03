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

#ifndef _BUGGER_MISC_FMT_HPP_
#define _BUGGER_MISC_FMT_HPP_

#ifndef FMT_HEADER_ONLY
#define FMT_HEADER_ONLY
#endif

/* if those macros are not defined and fmt includes windows.h
 * then many macros are defined that can interfere with standard C++ code
 */
#ifndef NOMINMAX
#define NOMINMAX
#define BUGGER_DEFINED_NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define BUGGER_DEFINED_WIN32_LEAN_AND_MEAN
#endif

#ifndef NOGDI
#define NOGDI
#define BUGGER_DEFINED_NOGDI
#endif

#include "bugger/external/fmt/format.h"
#include "bugger/external/fmt/ostream.h"

#ifdef BUGGER_DEFINED_NOGDI
#undef NOGDI
#undef BUGGER_DEFINED_NOGDI
#endif

#ifdef BUGGER_DEFINED_NOMINMAX
#undef NOMINMAX
#undef BUGGER_DEFINED_NOMINMAX
#endif

#ifdef BUGGER_DEFINED_WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#undef BUGGER_DEFINED_WIN32_LEAN_AND_MEAN
#endif

#endif
