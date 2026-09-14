# 本文件由ara-gen(1.0.0)根据arxml生成

set(EXE_src
    main.cpp
)
set(EXE_target ${ARA_GEN_EXECUTABLE})
add_executable(${EXE_target} ${EXE_src})

set_target_properties(${EXE_target}
    PROPERTIES
        OUTPUT_NAME ${ARA_GEN_EXECUTABLE}
    )

target_link_libraries(${EXE_target}
    ${ARA_GEN_BUILD_HELPER_LIBRARY}
)
add_dependencies(${EXE_target} ${ARA_GEN_BUILD_HELPER_LIBRARY})

# isoft-ap 对 target 的统一处理
aragen_helper_register_target(TARGET ${EXE_target} TYPE EXE)
