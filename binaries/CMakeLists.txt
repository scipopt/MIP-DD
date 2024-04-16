include(GenerateExportHeader)

set(CMAKE_CXX_STANDARD 14)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(bugger-executable ${CMAKE_CURRENT_LIST_DIR}/../src/bugger.cpp)
set_target_properties(bugger-executable PROPERTIES OUTPUT_NAME bugger RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_definitions(bugger-executable PRIVATE BUGGER_USE_EXTERN_TEMPLATES BUGGER_USE_EXTERN_TEMPLATES)

set(DEFINED_SOLVER FALSE)

# Link solver SCIP
if(DEFINED SCIP_DIR)
    if(DEFINED_SOLVER)
        message(FATAL_ERROR "Multiple solvers provided!")
    else()
        set(DEFINED_SOLVER TRUE)
    endif()
    find_package(SCIP REQUIRED)
    include_directories(${SCIP_INCLUDE_DIRS})
    target_compile_definitions(bugger-executable PRIVATE BUGGER_WITH_SCIP)
    add_custom_target(bugger-symlink ALL COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_SONAME_FILE:${SCIP_LIBRARIES}> ${CMAKE_BINARY_DIR}/$<TARGET_SONAME_FILE_NAME:${SCIP_LIBRARIES}>)
    target_link_libraries(bugger-executable ${SCIP_LIBRARIES})
    install(FILES ${PROJECT_SOURCE_DIR}/src/bugger/interfaces/ScipInterface.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bugger/interfaces)
endif()

# Link solver SoPlex
if(DEFINED SOPLEX_DIR)
    if(DEFINED_SOLVER)
        message(FATAL_ERROR "Multiple solvers provided!")
    else()
        set(DEFINED_SOLVER TRUE)
    endif()
    find_package(SOPLEX REQUIRED)
    include_directories(${SOPLEX_INCLUDE_DIRS})
    target_compile_definitions(bugger-executable PRIVATE BUGGER_WITH_SOPLEX)
    add_custom_target(bugger-symlink ALL COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_SONAME_FILE:${SOPLEX_LIBRARIES}> ${CMAKE_BINARY_DIR}/$<TARGET_SONAME_FILE_NAME:${SOPLEX_LIBRARIES}>)
    target_link_libraries(bugger-executable ${SOPLEX_LIBRARIES})
    install(FILES ${PROJECT_SOURCE_DIR}/src/bugger/interfaces/SoplexInterface.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bugger/interfaces)
endif()

if(NOT DEFINED_SOLVER)
    message(FATAL_ERROR "No solver provided!")
endif()

target_link_libraries(bugger-executable bugger-core)
