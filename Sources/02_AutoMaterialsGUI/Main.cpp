/**
 * Генератор материалов для импорта в Serious Modeller (Serious Engine 1). GUI версия.
 * Copyright (C) 2020 by Alex "DarkWolf" Nem - https://github.com/darkoffalex
 */

#include <Windows.h>
#include <stdexcept>
#include <cstring>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <string>
#include <sstream>
#include <fstream>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_GDI_IMPLEMENTATION

#include <nuklear/nuklear.h>
#include <nuklear/nuklear_gdi.h>

/// Дескриптор осноного окна отрисовки
HWND g_hwnd = nullptr;
/// Дескриптор контекста отрисовки
HDC g_hdc = nullptr;
/// Наименование класса
const char* g_strClassName = "MainWindowClass";
/// Заголовок окна
const char* g_strWindowCaption = "SED Auto Materials";
/// Контекст GUI Nuklear
struct nk_context *g_nkContext = nullptr;

/**
 * \brief Структура описывающая вершину
 *
 * \details Основной критений сравнения вершин - положение и текстурные координаты. Не обязательно хранить сами данные,
 * достаточно значть что индексы положения и текстурных координат совпадают
 */
struct Vertex
{
    unsigned posIdx = 0;
    unsigned uvIdx = 0;
    unsigned normalIdx = 0;

    bool operator==(const Vertex& v) const{
        return this->posIdx == v.posIdx && this->uvIdx == v.uvIdx;
    }

    struct Hash
    {
        size_t operator()(const Vertex& v) const{
            return std::hash<std::string>()(std::to_string(v.posIdx) + std::to_string(v.uvIdx));
        }
    };
};

/**
 * \brief Группа вершин
 *
 * \details Полигоны в группе объединены общими вершиными
 */
struct Group
{
    std::vector<unsigned> polygons;
    std::unordered_set<Vertex, Vertex::Hash> vertices;

    [[nodiscard]] bool polygonBelongs(const std::vector<Vertex>& polygonVertices) const
    {
        return std::any_of(polygonVertices.begin(),polygonVertices.end(),[&](const Vertex& v){
            return vertices.count(v);
        });
    }

    void addPolygon(const unsigned polygonIdx, const std::vector<Vertex>& polygonVertices)
    {
        polygons.push_back(polygonIdx);

        for(const auto& v : polygonVertices){
            vertices.insert(v);
        }
    }

    void joinGroup(Group& group)
    {
        polygons.insert(polygons.end(),group.polygons.begin(),group.polygons.end());
        vertices.merge(group.vertices);
    }

    void cleanGroup()
    {
        polygons.clear();
        vertices.clear();
    }
};

/**
 * Глобальное ссостояние приложения
 */
enum GlobalAppState
{
    // Файл не выбран
    eFileNotSelected,
    // Не удается открыть файл
    eCanNotOpen,
    // Файл поврежден или не соответствует формату
    eBadFile,
    // Данные прочитаны
    eFileRead,
    // Разбиение на группы проведено
    eDivided
};

/**
 * Способ деления геометрии на группы (материалы)
 */
enum DivisionMode
{
    // По материалу на каждую UV группу
    ePerUvGroup,
    // По материалу на каждый полигон
    ePerPolygon
};

/// Состояние приложения (изначально файл не выбран)
GlobalAppState g_eGlobalState = GlobalAppState::eFileNotSelected;

/// Путь к выбранному файлу
std::string g_strPathToFile;
/// Массив полигонов
std::vector<std::vector<Vertex>> g_vPolygons;
/// Основная информация .obj (вершины, нормали, uv-координаты, не включая данные о полигонах)
std::vector<std::string> g_objBaseData;
/// Массив групп
std::vector<Group> g_vGroups;
/// Текущий способ деления на группы
DivisionMode g_eDivisionMode = DivisionMode::ePerUvGroup;

/**
 * Обработчик оконных сообщений
 * @param hWnd Дескриптор окна
 * @param message Сообщение
 * @param wParam Параметр сообщения
 * @param lParam Параметр сообщения
 * @return Код выполнения
 */
LRESULT CALLBACK WindowProcedure(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

/**
 * Событие нажатия на копоку выбора файла
 */
void OnSelectFileButtonPressed();

/**
 * Событие нажатия на кнопку экспорта файла
 */
void OnExportFileButtonPressed();

/**
 * Событие выбора файла
 * @param filePath Путь к файлу
 */
void OnFileSelected(const std::string& filePath);

/**
 * Событие выбора пути файла для экспорта
 * @param filePath Путь к файлу
 */
void OnExportFileSelected(const std::string& filePath);

/**
 * Считать данные о полигонах из файла
 * @param in Объект открытого потока чтения
 */
void ReadPolygons(std::ifstream& in);

/**
 * Считать основную информацию .obj файла
 * @param in Объект открытого потока чтения
 */
void ReadBaseObjData(std::ifstream& in);

/**
 * Деление геометрии на материалы - по материалу на каждую UV группу
 */
void DivideForEachUv();

/**
 * Деление геометрии на материалы - по материалу на каждый полигон
 */
void DivideForEachPoly();

/**
 * Точка входа в Win32 приложение
 * @param hInstance Хендл модуля
 * @param hPrevInstance Не используется (устарело)
 * @param lpCmdLine Аргументы запуска
 * @param nShowCmd Изначальный вид окна (минимизировано/максимизировано/скрыто)
 * @return Код выполнения
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nShowCmd;

    try {
        // Информация о классе
        WNDCLASSEX classInfo;
        classInfo.cbSize = sizeof(WNDCLASSEX);
        classInfo.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        classInfo.cbClsExtra = 0;
        classInfo.cbWndExtra = 0;
        classInfo.hInstance = hInstance;
//        classInfo.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
        classInfo.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
        classInfo.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(1));
        classInfo.hCursor = LoadCursor(nullptr, IDC_ARROW);
        classInfo.hbrBackground = CreateSolidBrush(RGB(240, 240, 240));
        classInfo.lpszMenuName = nullptr;
        classInfo.lpszClassName = g_strClassName;
        classInfo.lpfnWndProc = WindowProcedure;

        // Пытаемся зарегистрировать оконный класс
        if (!RegisterClassEx(&classInfo)) {
            throw std::runtime_error("Can't register window class.");
        }

        // Создание окна
        g_hwnd = CreateWindow(
                g_strClassName,
                g_strWindowCaption,
                WS_OVERLAPPEDWINDOW &~WS_MAXIMIZEBOX,
                0, 0,
                640, 250,
                nullptr,
                nullptr,
                hInstance,
                nullptr);

        // Если не удалось создать окно
        if (!g_hwnd) {
            throw std::runtime_error("Can't create main application window.");
        }

        // Показать окно
        ShowWindow(g_hwnd, SW_SHOWNORMAL);

        // Получение контекста отрисовки
        g_hdc = GetDC(g_hwnd);

        // Размеры клиентской области окна
        RECT clientRect;
        GetClientRect(g_hwnd, &clientRect);

        /** N U K L E A R **/

        GdiFont* font1 = nk_gdifont_create("Arial", 15);
        GdiFont* font2 = nk_gdifont_create("Arial", 17);
        g_nkContext = nk_gdi_init(font1, g_hdc, clientRect.right, clientRect.bottom);

        /** MAIN LOOP **/

        // Оконное сообщение
        MSG msg = {};

        // Запуск цикла
        while (true)
        {
            // Обработка событий Nuklear
            nk_input_begin(g_nkContext);

            // Обработка оконных сообщений
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                DispatchMessage(&msg);

                if (msg.message == WM_QUIT) {
                    break;
                }
            }

            // Обработка событий Nuklear (завершение)
            nk_input_end(g_nkContext);

            // Если файл прочитан (деление на группы еще не произведено)
            if(g_eGlobalState == eFileRead)
            {
                // Поделить геометрию на группы нужным образом
                if(g_eDivisionMode == ePerUvGroup) DivideForEachUv();
                else DivideForEachPoly();

                // Деление произведено
                g_eGlobalState = eDivided;
            }

            /** G U I **/

            // Получение размеров текущей клиентской области окна
            GetClientRect(g_hwnd, &clientRect);

            // Панель выбора файла
            nk_gdi_set_font(font1);
            if(nk_begin(g_nkContext, "Select file",
                        nk_rect(5.0f,5.0f,static_cast<float>(clientRect.right-10),80.0f),
                        NK_WINDOW_BORDER|NK_WINDOW_TITLE|NK_WINDOW_NO_SCROLLBAR))
            {
                nk_layout_row_begin(g_nkContext, NK_DYNAMIC, 30, 2);

                nk_layout_row_push(g_nkContext, 0.25f);
                if (nk_button_label(g_nkContext, "Select .OBJ file")){
                    OnSelectFileButtonPressed();
                }

                nk_layout_row_push(g_nkContext, 0.75f);
                std::string fileLabel = g_strPathToFile.empty() ? "No file..." : g_strPathToFile;
                nk_label(g_nkContext,fileLabel.c_str(),nk_text_alignment::NK_TEXT_LEFT);

                nk_layout_row_end(g_nkContext);
            }
            nk_end(g_nkContext);

            // Если файл успешно прочитан либо деление на группы уже было произведено
            if(g_eGlobalState == GlobalAppState::eFileRead || g_eGlobalState == GlobalAppState::eDivided)
            {
                // Панель информации
                nk_gdi_set_font(font1);
                if(nk_begin(g_nkContext, "File information",
                            nk_rect(5.0f,90.0f,static_cast<float>(clientRect.right-10),80.0f),
                            NK_WINDOW_BORDER|NK_WINDOW_TITLE|NK_WINDOW_NO_SCROLLBAR))
                {
                    nk_gdi_set_font(font2);
                    nk_layout_row_dynamic(g_nkContext, 30, 1);
                    nk_label(g_nkContext, std::string("Loaded " + std::to_string(g_vPolygons.size()) + " polygons").c_str(), nk_text_alignment::NK_TEXT_LEFT);
                }
                nk_end(g_nkContext);

                // Панель настроек экспорта
                nk_gdi_set_font(font1);
                if(nk_begin(g_nkContext, "Export settings",
                            nk_rect(5.0f,178.0f,static_cast<float>(clientRect.right-10),160.0f),
                            NK_WINDOW_BORDER|NK_WINDOW_TITLE|NK_WINDOW_NO_SCROLLBAR))
                {
                    nk_layout_row_dynamic(g_nkContext, 30, 2);
                    if (nk_option_label(g_nkContext, "Per UV group", g_eDivisionMode == DivisionMode::ePerUvGroup))
                    {
                        if(g_eDivisionMode != DivisionMode::ePerUvGroup)
                        {
                            g_eGlobalState = GlobalAppState::eFileRead;
                            g_eDivisionMode = DivisionMode::ePerUvGroup;
                        }
                    }

                    std::string exportLabel = "Export (" + std::to_string(g_vGroups.size()) + " groups)";
                    if (nk_button_label(g_nkContext, exportLabel.c_str())){
                        OnExportFileButtonPressed();
                    }

                    if (nk_option_label(g_nkContext, "Per single polygon (HARDCORE)", g_eDivisionMode == DivisionMode::ePerPolygon))
                    {
                        if(g_eDivisionMode != DivisionMode::ePerPolygon)
                        {
                            g_eGlobalState = GlobalAppState::eFileRead;
                            g_eDivisionMode = DivisionMode::ePerPolygon;
                        }
                    }
                }
                nk_end(g_nkContext);
            }

            /** Рендеринг GUI **/
            nk_gdi_render(nk_rgb(30,30,30));
            nk_clear(g_nkContext);
        }
    }
    catch (std::exception& ex) {
        MessageBoxA(nullptr,ex.what(),"Error", MB_OK);
        return 1;
    }

    // Уничтожение окна
    DestroyWindow(g_hwnd);
    // Вырегистрировать класс окна
    UnregisterClass(g_strClassName, hInstance);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Обработчик оконных сообщений
 * @param hWnd Дескриптор окна
 * @param message Сообщение
 * @param wParam Параметр сообщения
 * @param lParam Параметр сообщения
 * @return Код выполнения
 */
LRESULT CALLBACK WindowProcedure(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if(message == WM_DESTROY){
        PostQuitMessage(0);
        return 0;
    }else if(message == WM_GETMINMAXINFO){
        auto* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 400;
        mmi->ptMinTrackSize.y = 500;
        mmi->ptMaxTrackSize.x = 500;
        mmi->ptMaxTrackSize.y = 500;
        return 0;
    }

    if (nk_gdi_handle_event(hWnd, message, wParam, lParam))
        return 0;

    return DefWindowProc(hWnd, message, wParam, lParam);
}

/**
 * Событие нажатия на копоку выбора файла
 */
void OnSelectFileButtonPressed()
{
    // Путь к файлу
    char szFileName[MAX_PATH] = "";

    // Структура открытия файлового диалога
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = "Wavefront .obj files (*.obj)\0*.obj\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;
    ofn.lpstrDefExt = "obj";

    // Открыть файловый диалог
    if(GetOpenFileName(&ofn)){
        g_strPathToFile = std::string(szFileName);
        OnFileSelected(g_strPathToFile);
    }

    // Хотфикс для Nuklear
    // При открытии файлового диалога данная часть кода не выполняется, из-за чего терубется лишний клик чтобы окно начало реагировать
    // Явное выполнение данного кода позволяет решить эту проблему
    nk_input_button(g_nkContext, NK_BUTTON_DOUBLE, (short)LOWORD(0), (short)HIWORD(0), 0);
    nk_input_button(g_nkContext, NK_BUTTON_LEFT, (short)LOWORD(0), (short)HIWORD(0), 0);
    ReleaseCapture();
}

/**
 * Событие нажатия на кнопку экспорта файла
 */
void OnExportFileButtonPressed()
{
    // Путь к файлу
    char szFileName[MAX_PATH] = "";

    // Структура открытия файлового диалога
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = "Wavefront .obj files (*.obj)\0*.obj\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER;
    ofn.lpstrDefExt = "obj";

    // Открыть файловый диалог
    if(GetSaveFileName(&ofn)){
        OnExportFileSelected(szFileName);
    }

    // Хотфикс для Nuklear
    // При открытии файлового диалога данная часть кода не выполняется, из-за чего терубется лишний клик чтобы окно начало реагировать
    // Явное выполнение данного кода позволяет решить эту проблему
    nk_input_button(g_nkContext, NK_BUTTON_DOUBLE, (short)LOWORD(0), (short)HIWORD(0), 0);
    nk_input_button(g_nkContext, NK_BUTTON_LEFT, (short)LOWORD(0), (short)HIWORD(0), 0);
    ReleaseCapture();
}

/**
 * Событие выбора файла
 * @param filePath Путь к файлу
 */
void OnFileSelected(const std::string& filePath)
{
    // Открыть файл для чтения
    std::ifstream in(filePath,std::ifstream::in);

    // Если не удается открыть
    if(in.fail()){
        g_eGlobalState = GlobalAppState::eCanNotOpen;
        MessageBoxA(nullptr,"Can't open file for reading.","Error", MB_OK);
        return;
    }

    // Прочесть данные полигонов
    ReadPolygons(in);
    if(g_vPolygons.empty()){
        g_eGlobalState = GlobalAppState::eBadFile;
        MessageBoxA(nullptr,"File format is wrong or file is corrupt.","Error", MB_OK);
        return;
    }

    // Прочесть и сохранить строки основных данных (кроме полигонов)
    ReadBaseObjData(in);

    // Файл прочитан
    g_eGlobalState = GlobalAppState::eFileRead;

    // Закрыть файл
    in.close();
}

/**
 * Событие выбора пути файла для экспорта
 * @param filePath Путь к файлу
 */
void OnExportFileSelected(const std::string &filePath)
{
    // Поделить путь к файлу на фрагменты
    char drive[10], directory[MAX_PATH], basename[MAX_PATH];
    _splitpath_s(filePath.c_str(), drive, 10, directory, MAX_PATH, basename, MAX_PATH, nullptr, 0);

    // Путь к результату (без расширения)
    std::string objOutputFilePath = std::string(drive) + std::string(directory) + std::string(basename);

    // П О Д Г О Т О В К А (Нахуй она воообще тут нужна?? Можно было писать в файл сразу...)

    // Содержимое результирющих файлов
    std::vector<std::string> objFileText = {
            "# SED Auto Materials v1.0 OBJ File",
            "mtllib " + std::string(basename) + ".mtl"
    };
    objFileText.insert(objFileText.end(),g_objBaseData.begin(),g_objBaseData.end());

    std::vector<std::string> mtlFileText = {
            "# SED Auto Materials v1.0 MTL File",
            "# Material Count: " + std::to_string(g_vGroups.size())
    };

    // Пройтись по всем группам
    for(unsigned g = 0; g < g_vGroups.size(); g++)
    {
        // Добавление данных .mtl
        mtlFileText.emplace_back("");
        mtlFileText.emplace_back("newmtl Material." + std::to_string(g));
        mtlFileText.emplace_back("Ns 225.000000");
        mtlFileText.emplace_back("Ka 1.000000 1.000000 1.000000");
        mtlFileText.emplace_back("Kd 0.800000 0.800000 0.800000");
        mtlFileText.emplace_back("Ks 0.500000 0.500000 0.500000");
        mtlFileText.emplace_back("Ke 0.000000 0.000000 0.000000");
        mtlFileText.emplace_back("Ni 1.450000");
        mtlFileText.emplace_back("d 1.000000");
        mtlFileText.emplace_back("illum 2");

        // Добавление данных .obj
        objFileText.emplace_back("usemtl Material." + std::to_string(g));
        objFileText.emplace_back("s off");

        for(unsigned int p : g_vGroups[g].polygons)
        {
            const auto& polygon = g_vPolygons[p];
            std::string polygonStr = "f";

            for(const auto& v : polygon){
                polygonStr += " " + std::to_string(v.posIdx) + "/" + std::to_string(v.uvIdx) + "/" + std::to_string(v.normalIdx);
            }

            objFileText.push_back(polygonStr);
        }
    }

    // З А П И С Ь  В  Ф А Й Л Ы

    // Запись в файл .obj
    std::ofstream outObj;
    outObj.open(objOutputFilePath + ".obj", std::ios::out | std::ios::trunc);
    if(outObj.fail()) throw std::runtime_error("Can't open .OBJ file for writing.");
    for(const std::string& str : objFileText){
        outObj << str << std::endl;
    }
    outObj.close();

    // Запись в файл .mtl
    std::ofstream outMtl;
    outMtl.open(objOutputFilePath + ".mtl", std::ios::out | std::ios::trunc);
    if(outMtl.fail()) throw std::runtime_error("Can't open .MTL file for writing.");
    for(const std::string& str : mtlFileText){
        outMtl << str << std::endl;
    }
    outMtl.close();

    // Сообщение об успехе
    MessageBoxA(g_hwnd,"Files successfully exported.","Done",MB_OK);
}

/**
 * Считать данные о полигонах из файла
 * @param in Объект открытого потока чтения
 */
void ReadPolygons(std::ifstream &in)
{
    // Очистить массив полигонов
    g_vPolygons.clear();

    // Вернуться к началу
    in.clear();
    in.seekg(0, std::ios::beg);

    // Переменная для хранения текущей строки файла
    std::string line;

    // Массив полигонов (массив массивов вершин)
    using Polygon = std::vector<Vertex>;

    // Пока не достигнут конец файла
    while(!in.eof())
    {
        // Получить строку
        std::getline(in, line);

        // Поток ввода/вывода для строки
        std::stringstream iss(line);

        // Для "мусорных данных"
        char cTrash;

        // Если строка начинается с подстроки "f "
        if(!line.compare(0, 2, "f "))
        {
            // Полигон (массив вершин)
            Polygon p;

            // Первый символ (f) вписывается в мусорную переменную
            iss >> cTrash;

            // Читать символы строки в структуру вершины пока строка не закончится
            Vertex v;
            while (iss >> v.posIdx >> cTrash >> v.uvIdx >> cTrash >> v.normalIdx){
                p.push_back(v);
            }

            // Добавить полигон
            g_vPolygons.push_back(p);
        }
    }
}

/**
 * Считать начало файла в массив строк
 * @param in Объект открытого потока чтения
 */
void ReadBaseObjData(std::ifstream &in)
{
    // Очистить массив строк
    g_objBaseData.clear();

    // Вернуться к началу
    in.clear();
    in.seekg(0, std::ios::beg);

    // Переменная для хранения текущей строки файла
    std::string line;

    // Пока не достигнут конец файла
    while(!in.eof())
    {
        // Получить строку
        std::getline(in, line);

        // Если строка не начинается с "#" или "mtlib"
        if(line.compare(0, 1, "#") && line.compare(0, 6, "mtllib"))
        {
            // Если строка начинается с "usemtl" - оборвать цикл
            if(!line.compare(0,6,"usemtl")) break;
            // Иначе добаить строку в массив стрк
            else g_objBaseData.push_back(line);
        }
    }
}

/**
 * Деление геометрии на материалы - по материалу на каждую UV группу
 */
void DivideForEachUv()
{
    // Очистить группы
    g_vGroups.clear();

    // Пройтись по всем полигонам
    for(unsigned p = 0; p < g_vPolygons.size(); p++)
    {
        // Последняя группа в которую был добавлен текущий полигон
        int lastGroupPolyAdded = -1;

        // Пройтись по группам
        for(unsigned g = 0; g < g_vGroups.size(); g++)
        {
            // Если полигон должен принадлжать текущей группе
            if(g_vGroups[g].polygonBelongs(g_vPolygons[p]))
            {
                // Если была какая-то группа в которую этот же полигон был добавлен
                if(lastGroupPolyAdded != -1) {
                    g_vGroups[g].joinGroup(g_vGroups[lastGroupPolyAdded]);
                    g_vGroups[lastGroupPolyAdded].cleanGroup();
                }
                // Если полигон не добавлялся ранее ни в какие группы
                else{
                    g_vGroups[g].addPolygon(p,g_vPolygons[p]);
                    lastGroupPolyAdded = static_cast<int>(g);
                }
            }
        }

        // Если полигон так и не был добавлен ни в одну из групп
        // создать новую группу и добавить туда полигон
        if(lastGroupPolyAdded == -1){
            Group group;
            group.addPolygon(p,g_vPolygons[p]);
            g_vGroups.push_back(group);
        }
    }

    // Удалить пустые группы
    g_vGroups.erase(std::remove_if(g_vGroups.begin(),g_vGroups.end(),[](const Group& g){return g.polygons.empty();}),g_vGroups.end());
}

/**
 * Деление геометрии на материалы - по материалу на каждый полигон
 */
void DivideForEachPoly()
{
    // Очистить группы
    g_vGroups.clear();

    // Пройтись по всем полигонам
    for(unsigned p = 0; p < g_vPolygons.size(); p++)
    {
        Group group;
        group.addPolygon(p,g_vPolygons[p]);
        g_vGroups.push_back(group);
    }
}
