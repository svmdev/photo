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
    cout << "A command is required, one of:" << endl
        
    // Построить индекс дескрипторов лиц на изображениях указанного каталога и его подкаталогов
    << bold_on << "index" << bold_off << " - Build an index of face descriptors on images of the specified directory and its subdirectories" << endl
    << "Command template: " << bold_on << "./photo index <images_directory_path> <index_file_name>" << bold_off << endl
    << '\t' << bold_on << "<images_directory_path>" << bold_off << " - Catalog and its subdirectories of source images" << endl
    << '\t' << bold_on << "<sample_images_entry>" << bold_off << " - File name for writing of calculated descriptors." << endl

    << bold_on << "search" << bold_off << " - Search for image files with faces:" << endl
    << "Command template: " << bold_on << "./photo search <search_images_entry> <sample_images_entry>" << bold_off << " , where: " << endl
    << '\t' << bold_on << "<search_images_entry>" << bold_off << " - Search location (Where are we looking?): Catalog and its subdirectories or pre-calculated descriptors file name." << endl
    << '\t' << bold_on << "<sample_images_entry>" << bold_off << " - Search object (What are we looking for?): Catalog and its subdirectories or pre-calculated descriptors file name." << endl;

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

