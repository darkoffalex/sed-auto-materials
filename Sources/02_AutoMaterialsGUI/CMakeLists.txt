# Версия CMake
cmake_minimum_required(VERSION 3.14)

# Название приложения
set(TARGET_NAME "02_AutoMaterialsGUI")
set(TARGET_BIN_NAME "02_AutoMaterialsGUI")

# Добавляем .exe (проект в Visual Studio)
add_executable(${TARGET_NAME} WIN32
        "Main.cpp"
        "../../Resources/version.rc")

# Директории с включаемыми файлами (.h)
target_include_directories(${TARGET_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/Include")

# Линковка с Msimg32.lib для некоторых функций GDI
target_link_libraries(${TARGET_NAME} PUBLIC "Msimg32.lib")

# Меняем название запускаемого файла в зависимости от типа сборки
set_property(TARGET ${TARGET_NAME} PROPERTY OUTPUT_NAME "${TARGET_BIN_NAME}$<$<CONFIG:Debug>:_Debug>_${PLATFORM_BIT_SUFFIX}")

# Дополнительные флаги и объявления компиляции
if(MSVC)
    # Отключение стандартного min-max, включение мультибайтовых строк по умолчанию (для unicode используйте -DUNICODE -D_UNICODE)
    target_compile_definitions(${TARGET_NAME} PUBLIC "-DNOMINMAX -DMBCS")
    # Установка уровня warning (1)
    target_compile_options(${TARGET_NAME} PUBLIC /W1 /permissive-)
    # Статическая линковка с runtime библиотекой
    set_property(TARGET ${TARGET_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    # Флаг быстрой математики
    target_compile_options(${TARGET_NAME} PUBLIC -ffast-math)
    # Статическая линковка с runtime библиотекой
    set_property(TARGET ${TARGET_NAME} PROPERTY LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive")
endif()