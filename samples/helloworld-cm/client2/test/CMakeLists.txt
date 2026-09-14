# 本文件由ara-gen(1.0.0)根据arxml生成

set(TEST_src
    test1.cpp
)
set(TEST_target ${ARA_GEN_EXECUTABLE}_test)
add_executable(${TEST_target} ${TEST_src})
add_test(
    NAME ${TEST_target}
    COMMAND ${TEST_target})

target_link_libraries(${TEST_target}
    ${ARA_GEN_BUILD_HELPER_LIBRARY}
    GTest::gtest_main
)
add_dependencies(${TEST_target} ${ARA_GEN_BUILD_HELPER_LIBRARY})

# isoft-ap 对 target 的统一处理
aragen_helper_register_target(TARGET ${TEST_target} TYPE TEST)
