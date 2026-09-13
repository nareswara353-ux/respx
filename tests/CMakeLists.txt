set(UNIT_TESTS
    unit/test_sds.c
    unit/test_hash.c
    unit/test_skiplist.c
    unit/test_resp.c
)

foreach(test_src ${UNIT_TESTS})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} kave_core)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

add_executable(test_server integration/test_server.c)
target_link_libraries(test_server kave_core)
add_test(NAME test_server COMMAND test_server)

add_executable(benchmark benchmark/benchmark.c)
target_link_libraries(benchmark kave_core)
