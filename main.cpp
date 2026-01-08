#include<iostream>
#include <filesystem>
#include <cstring>

int index(char* faces_dir, char* index_file);
int search(char* search_dir, char* descriptors_entry);

using namespace std;

void usage_display()
{
    const std::string bold_on = "\e[1m";
    const std::string bold_off = "\e[0m";
    /*
        Commands:
    */
    // Требуется команда, одна из
    cout << "Требуется команда, одна из:" << endl
        
    // Построить индекс дескрипторов лиц на изображениях указанного каталога и его подкаталогов
    << bold_on << "index" << bold_off << " - Построить индекс дескрипторов лиц на изображениях указанного каталога и его подкаталогов" << endl
    << "Шаблон команды: " << bold_on << "./photo index <images_directory_path> <index_file_name>" << bold_off << endl
    << '\t' << bold_on << "<images_directory_path>" << bold_off << " - Каталог и его подкаталоги содержащие исходные изображения" << endl
    << '\t' << bold_on << "<sample_images_entry>" << bold_off << " - Имя файла для записи рассчитанных дескрипторов." << endl

    << bold_on << "search" << bold_off << " - Поиск файлов изображений с лицами:" << endl
    << "Шаблон команды: " << bold_on << "./photo search <search_images_entry> <sample_images_entry>" << bold_off << " , where: " << endl
    << '\t' << bold_on << "<search_images_entry>" << bold_off << " - Место поиска (Где мы ищем?): Каталог и его подкаталоги или имя файла с рассчитанными дескрипторами." << endl
    << '\t' << bold_on << "<sample_images_entry>" << bold_off << " - Объекст поиска (Что мы ищем?): Каталог и его подкаталоги или файла с рассчитанными дескрипторами." << endl;

    cout << endl;
}

int main(int argc, char** argv)
{
	if (argc < 2)
    {
        usage_display();
        return 1;
    }

    if(strcmp(argv[1], "index") == 0)
    {
        if (argc < 4)
        {
            usage_display();
            return 1;
        }
        index(argv[2], argv[3]);
    } 
    else if (strcmp(argv[1], "search") == 0)
    {
        if (argc < 4)
        {
            usage_display();
            return 1;
        }
        search(argv[2], argv[3]);
    }

	return 0;
}

