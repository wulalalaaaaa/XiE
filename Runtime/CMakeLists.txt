# 自动收集 Runtime 源文件，新增文件后重新配置即可出现在 VS
file(GLOB_RECURSE XIE_RUNTIME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_executable(xie_runtime
    ${XIE_RUNTIME_SOURCES}
)

target_compile_features(xie_runtime PRIVATE cxx_std_20)

target_link_libraries(xie_runtime
    PRIVATE
        xie_engine
)

# 运行前自动把依赖 DLL 复制到 xie_runtime.exe 同目录（解决找不到 xie_engine.dll）
if (WIN32)
    add_custom_command(TARGET xie_runtime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:xie_engine>
            $<TARGET_FILE_DIR:xie_runtime>
        COMMENT "Copy xie_engine.dll next to xie_runtime.exe"
        VERBATIM
    )
endif()

# 在 Visual Studio 中按真实目录树显示
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${XIE_RUNTIME_SOURCES})
