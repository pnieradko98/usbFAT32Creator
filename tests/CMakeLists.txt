find_package(Qt6 REQUIRED COMPONENTS Test)

# VirtualPenPlanner tests
add_executable(tst_VirtualPenPlanner tst_VirtualPenPlanner.cpp ../src/VirtualPenPlanner.cpp)
target_include_directories(tst_VirtualPenPlanner PRIVATE ../src)
target_link_libraries(tst_VirtualPenPlanner PRIVATE Qt6::Test)
add_test(NAME tst_VirtualPenPlanner COMMAND tst_VirtualPenPlanner)

# CommandBuilder tests
add_executable(tst_CommandBuilder tst_CommandBuilder.cpp ../src/CommandBuilder.cpp)
target_include_directories(tst_CommandBuilder PRIVATE ../src)
target_link_libraries(tst_CommandBuilder PRIVATE Qt6::Test)
add_test(NAME tst_CommandBuilder COMMAND tst_CommandBuilder)
