cmake_minimum_required(VERSION 3.8.2)

project("Conway's Game of Life")

find_package(Vulkan REQUIRED)

add_executable(
    game
    src/main.cpp
)
target_link_libraries(
    game
    PUBLIC Vulkan::Vulkan
)
target_compile_features(
    game
    PUBLIC cxx_std_14
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
    )
endif()