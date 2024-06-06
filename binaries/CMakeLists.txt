include(GenerateExportHeader)

set(CMAKE_CXX_STANDARD 14)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(bugger-executable ${CMAKE_CURRENT_LIST_DIR}/../src/bugger.cpp)
set_target_properties(bugger-executable PROPERTIES OUTPUT_NAME bugger RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_link_libraries(bugger-executable bugger-core)

option(SOPLEX "should SoPlex library be linked" OFF)
option(SCIP "should SCIP library be linked" ON)

# Link solver SoPlex
if(SOPLEX AND DEFINED SOPLEX_DIR)
    find_package(SOPLEX REQUIRED HINTS ${SOPLEX_DIR})
    target_include_directories(bugger-executable SYSTEM BEFORE PUBLIC ${SOPLEX_INCLUDE_DIRS})
    target_link_libraries(bugger-executable ${SOPLEX_LIBRARIES})
    target_compile_definitions(bugger-executable PRIVATE BUGGER_WITH_SOPLEX)
    add_custom_target(bugger-symlink ALL COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_SONAME_FILE:libsoplexshared> "${CMAKE_BINARY_DIR}/$<TARGET_SONAME_FILE_NAME:libsoplexshared>")
    install(FILES ${PROJECT_SOURCE_DIR}/src/bugger/interfaces/SoplexInterface.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bugger/interfaces)
# Link solver SCIP
elseif(SCIP AND DEFINED SCIP_DIR)
    find_package(SCIP REQUIRED HINTS ${SCIP_DIR})
    target_include_directories(bugger-executable SYSTEM BEFORE PUBLIC ${SCIP_INCLUDE_DIRS})
    target_link_libraries(bugger-executable ${SCIP_LIBRARIES})
    target_compile_definitions(bugger-executable PRIVATE BUGGER_WITH_SCIP)
    add_custom_target(bugger-symlink ALL COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_SONAME_FILE:${SCIP_LIBRARIES}> "${CMAKE_BINARY_DIR}/$<TARGET_SONAME_FILE_NAME:${SCIP_LIBRARIES}>")
    install(FILES ${PROJECT_SOURCE_DIR}/src/bugger/interfaces/ScipInterface.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bugger/interfaces)
# No solver found
else()
    message(FATAL_ERROR "No solver found! Select solver and specify its directory.")
endif()
