add_contract(vest vest
   ${CMAKE_CURRENT_SOURCE_DIR}/src/escrow.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/transfer.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/utils.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/vest.cpp
)

target_include_directories(vest
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(vest
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/vest.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/vest.contracts.md @ONLY )

target_compile_options( vest PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
