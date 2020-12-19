# Версия CMake
cmake_minimum_required(VERSION 3.14)

# Название приложения
set(TARGET_NAME "01_AutoMaterials")
set(TARGET_BIN_NAME "01_AutoMaterials")

# Добавляем .exe (проект в Visual Studio)
add_executable(${TARGET_NAME}
        "Main.cpp")

# Меняем название запускаемого файла в зависимости от типа сборки
set_property(TARGET ${TARGET_NAME} PROPERTY OUTPUT_NAME "${TARGET_BIN_NAME}$<$<CONFIG:Debug>:_Debug>_${PLATFORM_BIT_SUFFIX}")

# Дополнительные флаги и объявления компиляции
if(MSVC)
    # Отключение стандартных min-max функций для MSVC
    target_compile_definitions(${TARGET_NAME} PUBLIC "-DNOMINMAX")
    # Установка уровня warning (3)
    target_compile_options(${TARGET_NAME} PUBLIC /W3 /permissive-)
    # Статическая линковка с runtime библиотекой
    set_property(TARGET ${TARGET_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    # Установка уровня warning, флаг быстрой математики (ffast-math)
    target_compile_options(${TARGET_NAME} PUBLIC -Wall -Wextra -pedantic -ffast-math)
    # Статическая линковка с runtime библиотекой
    set_property(TARGET ${TARGET_NAME} PROPERTY LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive")
endif()