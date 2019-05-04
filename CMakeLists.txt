cmake_minimum_required(VERSION 3.8.2)

project("Conway's Game of Life")

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

add_custom_target(
    shaders
    $ENV{VULKAN_SDK}/bin/glslangValidator -V ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vertex.vert.glsl
    COMMAND $ENV{VULKAN_SDK}/bin/glslangValidator -V ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fragment.frag.glsl
    COMMAND $ENV{VULKAN_SDK}/bin/glslangValidator -V ${CMAKE_CURRENT_SOURCE_DIR}/shaders/compute.comp.glsl
    DEPENDS shaders/vertex.vert.glsl shaders/fragment.frag.glsl shaders/compute.comp.glsl
    BYPRODUCTS vertex.spv fragment.spv compute.spv
)

add_executable(
    game
    src/main.cpp
    src/vulkan_methods.cpp
)
target_link_libraries(
    game
    PUBLIC Vulkan::Vulkan
    PUBLIC glfw
)
target_compile_features(
    game
    PUBLIC cxx_std_17
)
add_dependencies(
    game
    shaders
)

if (APPLE)
    add_custom_command(
        TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:game>/lib/
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:game>/vulkan/
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:game>/vulkan/explicit_layer.d/
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:game>/vulkan/icd.d/
        
        COMMAND cat $ENV{VULKAN_SDK}/etc/vulkan/icd.d/MoltenVK_icd.json | sed -e 's/\\.\\.\\/\\.\\.\\/\\.\\.\\//..\\/..\\//g' | tee $<TARGET_FILE_DIR:game>/vulkan/icd.d/MoltenVK_icd.json
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $ENV{VULKAN_SDK}/lib/libMoltenVK.dylib $<TARGET_FILE_DIR:game>/lib/libMoltenVK.dylib
        
        COMMAND cat $ENV{VULKAN_SDK}/etc/vulkan/explicit_layer.d/VkLayer_standard_validation.json | sed -e 's/\\.\\.\\/\\.\\.\\/\\.\\.\\//..\\/..\\//g' | tee $<TARGET_FILE_DIR:game>/vulkan/explicit_layer.d/VkLayer_standard_validation.json
        
        COMMAND cat $ENV{VULKAN_SDK}/etc/vulkan/explicit_layer.d/VkLayer_khronos_validation.json | sed -e 's/\\.\\.\\/\\.\\.\\/\\.\\.\\//..\\/..\\//g' | tee $<TARGET_FILE_DIR:game>/vulkan/explicit_layer.d/VkLayer_khronos_validation.json
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $ENV{VULKAN_SDK}/lib/libVkLayer_khronos_validation.dylib $<TARGET_FILE_DIR:game>/lib/libVkLayer_khronos_validation.dylib

        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:glfw> $<TARGET_FILE_DIR:game>/
    )
endif()