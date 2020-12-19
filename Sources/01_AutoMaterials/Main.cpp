/**
 * Генератор материалов для импорта в Serious Modeller (Serious Engine 1). Консольная версия.
 * Copyright (C) 2020 by Alex "DarkWolf" Nem - https://github.com/darkoffalex
 */

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <algorithm>

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
 * \brief Точка входа
 * \param argc Кол-во аргументов
 * \param argv Аргументы
 * \return Код выполнения
 */
int main(int argc, char* argv[])
{
    // Если кол-во аргументов менее двух (первый агрумент - путь к исполняемому файлу)
    if(argc < 2){
        std::cout << "No file provided." << std::endl;
        return 1;
    }

    /** Ч Т Е Н И Е **/

    // Открыть файл для чтения
    std::ifstream in;
    in.open(argv[1],std::ifstream::in);

    // Если не удалось открыть
    if(in.fail()){
        std::cout << "Can't open file \"" << argv[1] << "\"." << std::endl;
        return 1;
    }

    // Переменная для хранения текущей строки файла
    std::string line;

    // Массив полигонов (массив массивов вершин)
    using Polygon = std::vector<Vertex>;
    std::vector<Polygon> polygons;

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
            polygons.push_back(p);
        }
    }

    // Если не удалось считать данные полигонов
    if(polygons.empty()){
        std::cout << "Can't ready polygon data from file." << std::endl;
        return 1;
    }

    /** Р А З Б И Е Н И Е  Н А  Г Р У П П Ы **/

    // Группы полигонов
    std::vector<Group> groups;

    // Пройтись по всем полигонам
    for(unsigned p = 0; p < polygons.size(); p++)
    {
        // Последняя группа в которую был добавлен текущий полигон
        int lastGroupPolyAdded = -1;

        // Пройтись по группам
        for(unsigned g = 0; g < groups.size(); g++)
        {
            // Если полигон должен принадлжать текущей группе
            if(groups[g].polygonBelongs(polygons[p]))
            {
                // Если была какая-то группа в которую этот же полигон был добавлен
                if(lastGroupPolyAdded != -1) {
                    groups[g].joinGroup(groups[lastGroupPolyAdded]);
                    groups[lastGroupPolyAdded].cleanGroup();
                }
                    // Если полигон не добавлялся ранее ни в какие группы
                else{
                    groups[g].addPolygon(p,polygons[p]);
                    lastGroupPolyAdded = static_cast<int>(g);
                }
            }
        }

        // Если полигон так и не был добавлен ни в одну из групп
        // создать новую группу и добавить туда полигон
        if(lastGroupPolyAdded == -1){
            Group group;
            group.addPolygon(p,polygons[p]);
            groups.push_back(group);
        }
    }

    // Удалить пустые группы
    groups.erase(std::remove_if(groups.begin(),groups.end(),[](const Group& g){return g.polygons.empty();}),groups.end());

    /** П О Д Г О Т О В К А  К  В Ы В О Д У **/

    // Имя выходного файла
    std::string outputFilename = argc < 3 ? "output" : argv[2];

    // Содержимое результирющих файлов
    std::vector<std::string> objFileText = {
            "# SED Auto Materials v1.0 OBJ File",
            "mtllib " + outputFilename + ".mtl"
    };
    std::vector<std::string> mtlFileText = {
            "# SED Auto Materials v1.0 MTL File",
            "# Material Count: " + std::to_string(groups.size())
    };

    // Вернуться к началу исходного .obj файла
    in.clear();
    in.seekg(0, std::ios::beg);

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
            else objFileText.push_back(line);
        }
    }

    // Закрыть файл
    in.close();

    // Пройтись по всем группам
    for(unsigned g = 0; g < groups.size(); g++)
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

        for(unsigned int p : groups[g].polygons)
        {
            const auto& polygon = polygons[p];
            std::string polygonStr = "f";

            for(const auto& v : polygon){
                polygonStr += " " + std::to_string(v.posIdx) + "/" + std::to_string(v.uvIdx) + "/" + std::to_string(v.normalIdx);
            }

            objFileText.push_back(polygonStr);
        }
    }

    /** В Ы В О Д **/

    // Запись в файл .obj
    std::ofstream outObj;
    outObj.open(outputFilename + ".obj", std::ios::out | std::ios::trunc);
    for(const std::string& str : objFileText){
        outObj << str << std::endl;
    }
    outObj.close();

    // Запись в файл .mtl
    std::ofstream outMtl;
    outMtl.open(outputFilename + ".mtl", std::ios::out | std::ios::trunc);
    for(const std::string& str : mtlFileText){
        outMtl << str << std::endl;
    }
    outMtl.close();

    return 0;
}