#include <filesystem>
#include <map>

#include <dlib/dnn.h>
#include <dlib/gui_widgets.h>
#include <dlib/clustering.h>
#include <dlib/string.h>
#include <dlib/image_io.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_transforms.h>
#include <dlib/opencv.h>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <string>
#include <limits.h>
#include <unistd.h>


using namespace dlib;
using namespace std;

template <template <int,template<typename>class,int,typename> class block, int N, template<typename>class BN, typename SUBNET>
using residual = add_prev1<block<N,BN,1,tag1<SUBNET>>>;

template <template <int,template<typename>class,int,typename> class block, int N, template<typename>class BN, typename SUBNET>
using residual_down = add_prev2<avg_pool<2,2,2,2,skip1<tag2<block<N,BN,2,tag1<SUBNET>>>>>>;

template <int N, template <typename> class BN, int stride, typename SUBNET> 
using block  = BN<con<N,3,3,1,1,relu<BN<con<N,3,3,stride,stride,SUBNET>>>>>;

template <int N, typename SUBNET> using ares      = relu<residual<block,N,affine,SUBNET>>;
template <int N, typename SUBNET> using ares_down = relu<residual_down<block,N,affine,SUBNET>>;

template <typename SUBNET> using alevel0 = ares_down<256,SUBNET>;
template <typename SUBNET> using alevel1 = ares<256,ares<256,ares_down<256,SUBNET>>>;
template <typename SUBNET> using alevel2 = ares<128,ares<128,ares_down<128,SUBNET>>>;
template <typename SUBNET> using alevel3 = ares<64,ares<64,ares<64,ares_down<64,SUBNET>>>>;
template <typename SUBNET> using alevel4 = ares<32,ares<32,ares<32,SUBNET>>>;

using anet_type = loss_metric<fc_no_bias<128,avg_pool_everything<
                            alevel0<
                            alevel1<
                            alevel2<
                            alevel3<
                            alevel4<
                            max_pool<3,3,2,2,relu<affine<con<32,7,7,2,2,
                            input_rgb_image_sized<150>
                            >>>>>>>>>>>>;

std::filesystem::path exe_parent_path()
{
  char result[ PATH_MAX ];
  ssize_t count = readlink( "/proc/self/exe", result, PATH_MAX );
  std::filesystem::path exe_path ( std::string( result, (count > 0) ? count : 0 ) );
  return exe_path.parent_path();
}

anet_type anet_t()
{
    std::filesystem::path dat_dir = exe_parent_path();
    // by http://dlib.net/files
    std::filesystem::path dat_file ("dat/dlib_face_recognition_resnet_model_v1.dat");
    std::filesystem::path dat_path = dat_dir / dat_file;
	anet_type net;
    deserialize(dat_path) >> net;
    return net;
}

shape_predictor shape_p()
{
    std::filesystem::path dat_dir = exe_parent_path();
    // by http://dlib.net/files
    // * "dat/shape_predictor_5_face_landmarks.dat"
    std::filesystem::path dat_file ("dat/shape_predictor_68_face_landmarks.dat");
    std::filesystem::path dat_path = dat_dir / dat_file;
	shape_predictor sp;
	deserialize(dat_path) >> sp;
	return sp;
}


/*
    Запись дескрипторов в указанный выходной поток
    @indexes - Множество <путь файла, дескрипторы файла>
    @outfile - поток сохранения
*/
int save_to_out(std::map<std::string, std::vector<matrix<float,0,1>>> indexes, std::fstream& outfile)
{
    for (const auto& [key, value] : indexes)
    {
        outfile << key << endl;
        outfile << value.size() << endl;
        for (int i = 0; i < value.size(); ++i)
        {
            for (const float & element : value[i])
            {
                outfile << element << " ";
            }
            outfile << endl;
        }
    }

    return 0;
}


/*
    Чтение дескрипторов из файла
    @index_file - имя файла
*/
int index_from_file(std::filesystem::path index_file, std::map<std::string, std::vector<matrix<float,0,1>>>& index)
{
    ifstream infile(index_file);
    if (!infile.is_open()) {
        cerr << "Failed to open file for reading.\n";
        return 1;
    }

    while(!infile.eof())
    {
        std::string key;
        std::getline(infile >> std::ws, key);
        //infile.ignore();  '\n'

        if (infile.eof())
        {
            break;
        }

        if (key.empty())
        {
            continue;
        }

        int value_size;
        infile >> value_size;

        std::vector<matrix<float,0,1>> value;
        for (int i = 0; i < value_size; ++i)
        {
            matrix<float,0,1> v;
            v.set_size(128);
            for (int j = 0; j < 128; ++j)
            {
                infile >> v(j);
            }
            value.push_back(v);
        }

        index.insert(std::pair{key, value});
    }

    infile.close();

    return 0;
}



/*
    Обновление индекса дескрипторов лиц, по файлам изображений указанного каталога.
    Правила обновления следующие:
    - По существующим в "index" ключам перерасчёт не выполняется;
    - Ключи из "index", не найденные в каталоге изображений, удаляются.
    Параметры:
    @faces_dir - каталог изображений лиц;
    @index - индекс обновления;
    @index_file - имя файла индекса, возможный дополнительный, для записи расчётов - если параметр не имеет значения, то запись в файл не производится
*/
int index_from_dir(const std::filesystem::path& faces_dir, std::map<std::string, std::vector<matrix<float,0,1>>>& index, const std::optional<std::filesystem::path> index_file)
{
    // Предварительное чтение "ключей"-имён файлов, из каталога изображений
    std::set<std::string> keys;
    for (const auto & entry : std::filesystem::recursive_directory_iterator(faces_dir))
    {
        if (entry.is_regular_file())
        {
            keys.insert(entry.path());
        }
    }

    // Удаление из индекса ключей, отсутствующих в новом наборе
    std::vector<std::string> erase_keys;
    for (const auto& [key, value] : index)
    {
        if (!keys.count(key))
        {
            erase_keys.push_back(key);
        }
    }
    for (size_t i = 0; i < erase_keys.size(); ++i)
    {
        cout << "Erase, by key: " << erase_keys[i] << endl;
        index.erase(erase_keys[i]);
    }

    frontal_face_detector detector = get_frontal_face_detector();
    shape_predictor sp = shape_p();
    anet_type net = anet_t();

    // Если дано значение файла для записи
    bool with_file = index_file.has_value();
    // , то, запись выполняется порциями, по ключам буфера, который заполняется в цикле расчёта
    std::vector<std::string> buf_keys;
    // Первая порция в новый файл, и следующие - добавлением в конец уже существующего.
    bool is_app = false;
    // Размер порции
    const int write_size = 10;

    for (const auto & key : keys)
    {
        // Управление записью текущей порции расчётов в файл
        if (with_file)
        {
            if (buf_keys.size() == write_size)
            {
                std::fstream outfile;
                if (is_app)
                {
                    outfile.open(index_file.value(), std::ios::app);
                }
                else
                {
                    outfile.open(index_file.value(), std::ios::out | std::ios::trunc);
                    is_app = true;
                }

                if (!outfile.is_open()) {
                    cerr << "Failed to open file for writing.\n";
                    return 1;
                }

                // Формирование данных для записи
                std::map<std::string, std::vector<matrix<float,0,1>>> buf_index;
                for (size_t i = 0; i < buf_keys.size(); ++i)
                {
                    buf_index.insert(std::pair{buf_keys[i], index[buf_keys[i]]});
                }

                save_to_out(buf_index, outfile);

                outfile.close();
                buf_keys.clear();
            }

            buf_keys.push_back(key);
        }

        // Существующие записи в индексе оставляем как есть, без обновления - Расчёты только для новых ключей
        if (index.count(key))
        {
            continue;
        }

        // Если файл не читается как изображение, пропускаем
        if (!cv::haveImageReader(key))
        {
            continue;
        }

        try
        {
            cout << key;

            // Изображение грузим через OpenCV. dlib::load_image неправильно грузит, в некоторых случаях - игнорируется EXIF metadata и, как следствие, неправильная ориентация.
            cv::Mat cv_mat = cv::imread(key, cv::IMREAD_COLOR);
            cv_image<bgr_pixel> img(cv_mat);

            std::vector<matrix<rgb_pixel>> faces;
            for (auto face : detector(img))
            {
                auto shape = sp(img, face);
                matrix<rgb_pixel> face_chip;
                extract_image_chip(img, get_face_chip_details(shape, 150, 0.25), face_chip);
                faces.push_back(move(face_chip));
            }

            std::vector<matrix<float,0,1>> cur_face_descriptors;
            if (faces.size())
            {
                cur_face_descriptors = net(faces);
            }
            index.insert(std::pair{key, cur_face_descriptors});
            cout << " > " << cur_face_descriptors.size() << endl;
        }
        catch (std::exception& e)
        {
            cerr << e.what() << endl;
        }
    }

    // Управление записью текущей порции расчётов в файл
    if (with_file && buf_keys.size())
    {
        std::fstream outfile;
        if (is_app)
        {
            outfile.open(index_file.value(), std::ios::app);
        }
        else
        {
            outfile.open(index_file.value(), std::ios::out | std::ios::trunc);
            is_app = true;
        }

        if (!outfile.is_open()) {
            cerr << "Failed to open file for writing.\n";
            return 1;
        }

        // Формирование данных для записи
        std::map<std::string, std::vector<matrix<float,0,1>>> buf_index;
        for (size_t i = 0; i < buf_keys.size(); ++i)
        {
            buf_index.insert(std::pair{buf_keys[i], index[buf_keys[i]]});
        }

        save_to_out(buf_index, outfile);

        outfile.close();
    }


    return 0;
}



/*
    Создание индекса дескрипторов лиц, по файлам изображений указанного каталога, и запись в указанный файл
    @faces_dir - каталог изображений лиц
    @index_file - имя файла, для записи
*/
int index(char* faces_dir, char* index_file)
{
    std::map<std::string, std::vector<matrix<float,0,1>>> index;

    // Если файл индекса существует, то читаем его для последующего обновления
    const std::filesystem::path index_path(index_file);
    if (std::filesystem::exists(index_path) && std::filesystem::is_regular_file(index_path))
    {
        index_from_file(index_path, index);
    }

    index_from_dir(faces_dir, index, index_path);

    return 0;
}


int search(char* search_entry, char* sample_entry)
{
    std::map<std::string, std::vector<matrix<float,0,1>>> sample_index;
    const std::filesystem::path sample_path(sample_entry);
    if (std::filesystem::is_directory(sample_path))
    {
        std::optional<std::filesystem::path> index_path; 
        index_from_dir(sample_path, sample_index, index_path);
    } 
    else if (std::filesystem::is_regular_file(sample_path))
    {
        index_from_file(sample_path, sample_index);
    }

    std::vector<std::filesystem::path> found;
    int count_all = 0;

    const std::filesystem::path search_path(search_entry);
    if (std::filesystem::is_directory(search_path))
    { 
        // Если search_entry - is_directory, значит поиск по исходным файлам изображений, найденным в каталоге и его подкаталогах.
        // Расчёт дескрипторов лиц в процессе...
        try
        {
            frontal_face_detector detector = get_frontal_face_detector();
            shape_predictor sp = shape_p();
            anet_type net = anet_t();

            // Порог дистанции
            float distance_threshold = net.loss_details().get_distance_threshold();

            for (const auto & entry : std::filesystem::recursive_directory_iterator(search_path))
            {
                if (entry.is_regular_file())
                {
                    try
                    {
                        // Изображение грузим через OpenCV. dlib::load_image неправильно грузит, в некоторых случаях - игнорируется EXIF metadata и, как следствие, неправильная ориентация.
                        cv::Mat cv_mat = cv::imread(entry.path(), cv::IMREAD_COLOR);
                        cv_image<bgr_pixel> img(cv_mat);

                        std::vector<matrix<rgb_pixel>> faces;
                        for (auto face : detector(img))
                        {
                            auto shape = sp(img, face);
                            matrix<rgb_pixel> face_chip;
                            extract_image_chip(img, get_face_chip_details(shape,150,0.25), face_chip);
                            faces.push_back(move(face_chip));
                        }

                        if (faces.size() > 0)
                        {
                            std::vector<matrix<float,0,1>> entry_descriptors = net(faces);

                            bool is_found = false;
                            for (const auto& [sample_file, sample_descriptors] : sample_index)
                            {
                                for (size_t i = 0; i < sample_descriptors.size() && !is_found; ++i)
                                {
                                    for (size_t j = i; j < entry_descriptors.size() && !is_found; ++j)
                                    {
                                        is_found = length(sample_descriptors[i]-entry_descriptors[j]) < distance_threshold;
                                    }
                                }

                                if (is_found)
                                {
                                    found.push_back(entry.path());
                                    break;
                                }
                            }
                        }
                    }
                    catch (std::exception& e)
                    {
                        //cout << entry.path() << endl << e.what() << endl;
                    }

                    std::cout << "\t" << found.size() << "/" << count_all++ << "        " << "\r" << std::flush;
                }
            }
            cout << endl;
        }
        catch (std::exception& e)
        {
            cerr << e.what() << endl;
            return 1;
        }

    } 
    else if (std::filesystem::is_regular_file(search_path))
    {
        // Если search_entry - is_regular_file, значит поиск по предварительно рассчитанным дескрипторам, записанным в этом файле
        std::map<std::string, std::vector<matrix<float,0,1>>> search_index;
        index_from_file(search_path, search_index);

       // Порог дистанции
        float distance_threshold = 0.61;    //net.loss_details().get_distance_threshold();

        for (const auto& [search_file, search_descriptors] : search_index)
        {
            for (const auto& [sample_file, sample_descriptors] : sample_index)
            {
                bool is_found = false;
                for (size_t i = 0; i < sample_descriptors.size() && !is_found; ++i)
                {
                    for (size_t j = i; j < search_descriptors.size() && !is_found; ++j)
                    {
                        is_found = length(sample_descriptors[i]-search_descriptors[j]) < distance_threshold;
                    }
                }
                if (is_found)
                {
                    found.push_back(search_file);
                    break;
                }
            }
            std::cout << "\t" << found.size() << "/" << ++count_all << "        " << "\r" << std::flush;
        }
        cout << endl;
    }

    for (size_t i = 0; i < found.size(); ++i)
    {
        cout << found[i].string() << endl;
    }

    return 0;
}