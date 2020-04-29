#define MAX_FILE_SIZE 10000000


#include<thread>
#include<atomic>
#include <map>
#include <vector>
#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/locale.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <archive.h>
#include <archive_entry.h>
#include <boost/algorithm/string/replace.hpp>
#include "concurrent_hashmap.h"
#include "FileDoesNotExistsException.h"
#include "concurrent_hashmap.h"
#include "concurrent_queue.h"

template<typename K, typename V>
std::vector <std::pair<K, V>> map_to_vector(const std::map <K, V> &map) {
    return std::vector < std::pair < K, V >> (map.begin(), map.end());
}

bool sort_by_sec(const std::pair<std::string, int> &a,
                 const std::pair<std::string, int> &b) {
    return (a.second < b.second);
}

bool sort_by_first(const std::pair<std::string, int> &a,
                   const std::pair<std::string, int> &b) {
    return (a.first < b.first);
}

template<typename T>
int write_file(const std::string &filename, const T &counted_words) {
    std::ofstream file;
    file.open(filename, std::ios::out | std::ios::binary);
    for (auto &[word, count]: counted_words) {
        file << word << "              :         " << count << std::endl;
    }
    file.close();
    return 0;
}

inline std::chrono::high_resolution_clock::time_point get_current_time_fenced() {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    auto res_time = std::chrono::high_resolution_clock::now();
    std::atomic_thread_fence(std::memory_order_seq_cst);
    return res_time;
}

template<class D>
inline long long to_us(const D &duration) {
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

void stem_path(std::string &path) {
    path.erase(std::remove(path.begin(), path.end(), '"'), path.end());
}

void check_path_exists(std::string path) {
    if (!boost::filesystem::exists(path)) {
        throw FileDoesNotExistsException(path);
    }
}

std::map <std::string, std::string>
parseConfig(const std::string &filepath, const std::vector <std::string> &compulsory_keys) {
    std::ifstream file(filepath);
    std::string config((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

    std::istringstream is_file(config);

    std::map <std::string, std::string> configs;

    std::string line;
    while (std::getline(is_file, line)) {
        std::istringstream is_line(line);
        std::string key;
        if (std::getline(is_line, key, '=')) {
            std::string value;
            if (std::getline(is_line, value))
                configs.insert(std::make_pair(key, value));
        }
    }
    for (auto &compulsory_key : compulsory_keys) {
        if (configs.count(compulsory_key) == 0) {
            throw std::runtime_error(
                    "There is no compulsory key " + compulsory_key + " presented in configuration file");
        }
    }
    return configs;
}

std::string convert_to_normalized_utf_string(const std::string &str) {
    boost::locale::generator gen;
    gen.locale_cache_enabled(true);
    std::locale utf_locale = gen("UTF-8");
    auto system_locale = gen("");
    std::string result = boost::locale::conv::to_utf<char>(str, system_locale);
    result = boost::locale::normalize(result);
    result = boost::locale::fold_case(result);
    std::vector <std::string> escape_sequences = {"\a", "\b", "\f", "\n", "\r", "\t", "\v"};
    for (auto &escape_sequence : escape_sequences) {
        boost::replace_all_copy(result, escape_sequence, " ");
    }
    return result;
}


std::string read_file_into_memory(const std::string &current_file) {
    std::ifstream raw_file(current_file, std::ios::binary);
    auto buffer = [&raw_file] {
        std::ostringstream ss{};
        ss << raw_file.rdbuf();
        return ss.str();
    }();
    return buffer;
}

std::vector <std::string> get_archive_content(const std::string &file_buffer) {
    struct archive *archive;
    struct archive_entry *entry;
    archive = archive_read_new();
    archive_read_support_format_all(archive);
    const char *buffer = file_buffer.c_str();

    int reading_result = archive_read_open_memory(archive, buffer, file_buffer.size());
    if (reading_result != 0) {
        throw std::runtime_error("Error reading archive");
    }
    std::vector <std::string> contents;
    while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
        contents.emplace_back(archive_entry_pathname(entry));
        archive_read_data_skip(archive);
    }
    archive_read_free(archive);
    return contents;
}

std::string
get_archive_file_contents(const std::string &filename, std::vector <std::string> contents,
                          const std::string &file_buffer) {
    if (std::find(contents.begin(), contents.end(), filename) == contents.end()) {
        throw FileDoesNotExistsException(filename);
    }
    struct archive_entry *entry;
    struct archive *archive = archive_read_new();
    archive_read_support_format_all(archive);
    const char *buffer = file_buffer.c_str();
    int reading_result = archive_read_open_memory(archive, buffer, file_buffer.size());
    if (reading_result != 0) {
        throw std::runtime_error("Error reading archive");
    }
    void *buf;
    while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
        if (archive_entry_filetype(entry) == AE_IFREG) {
            int64_t length = archive_entry_size(entry);
            if (length < MAX_FILE_SIZE) {
                buf = operator new(length);
                if (!buf) {
                    archive_read_data_skip(archive);
                    continue;
                }
                archive_read_data(archive, buf, length);
            }
            break;

        }
    }
    archive_read_free(archive);
    std::string result = static_cast<char *>(buf);
    operator delete(buf);
    return result;
}

inline bool validate_string(std::string current_string) {

    size_t last_symbol = current_string.size() >= 2 ? 1 : 0;

    bool not_punct = !std::ispunct(static_cast<unsigned char>(current_string.at(0))) ||
                     !std::ispunct(static_cast<unsigned char>(current_string.at(last_symbol)));

    bool not_space = !std::isspace(static_cast<unsigned char>(current_string.at(0)));

    bool not_digit = !std::isdigit(static_cast<unsigned char>(current_string.at(0))) &&
                     !std::isdigit(static_cast<unsigned char>(current_string.at(last_symbol)));

    return not_punct && not_space && not_digit;
}

void word_count_to_map(const std::string &lines, ConcurrentHashmap <std::string, size_t> &concurrentMap) {

    boost::locale::boundary::segment_index <std::string::const_iterator> map(boost::locale::boundary::word,
                                                                             lines.begin(), lines.end());

    for (boost::locale::boundary::ssegment_index::iterator it = map.begin(), e = map.end(); it != e; ++it) {
        std::string current_string = (*it);

        if (validate_string(current_string)) {
            if (concurrentMap.keyExists(current_string)) {
                auto curVal = concurrentMap.get(current_string);
                concurrentMap.set_pair(std::make_pair(current_string, curVal + 1));
            } else
                concurrentMap.set_pair(std::make_pair(current_string, 1));
        }
    }
}

void enum_thread(const std::string &dir_path, concurrent_queue <std::string> &container) {

    for (auto &entry : boost::filesystem::recursive_directory_iterator(dir_path)) {

        if (!boost::filesystem::is_directory(entry) && boost::filesystem::file_size(entry) < MAX_FILE_SIZE) {
            std::string filename = entry.path().string();
            container.push_back(filename);
        }
    }
    container.push_back("");
}

void file_reading_thread(concurrent_queue <std::string> &file_names_q,
                         concurrent_queue <std::pair<std::string, std::string>> &raw_files_q) {

    while (true) {
        if (file_names_q.front().empty()) {
            break;
        }
        std::string current_file = file_names_q.pop();
        std::string extension = boost::filesystem::extension(current_file);
        std::string buffer = read_file_into_memory(current_file);
        auto file = std::make_pair(buffer, extension);
        raw_files_q.push_back(file);
    }
    raw_files_q.push_back(std::make_pair("", ""));
}

template<class Key, class T>
void indexing_thread(std::mutex &indexing_mutex,
                     concurrent_queue <std::pair<std::string, std::string>> &raw_files_q,
                     ConcurrentHashmap <Key, T> &concurrentMap) {
    while (true) {
        std::cout << "BBBBBBB" << std::endl;
        auto raw_file = raw_files_q.front();
        std::string file_buffer = raw_file.first;
        std::string ext = raw_file.second;
        if (file_buffer.empty() && ext.empty()) {
            break;
        }
        raw_files_q.pop();
        std::string file_content;
        if (ext == ".txt") {
            file_content = convert_to_normalized_utf_string(file_buffer);
            std::cout << file_content << "AAAA" << std::endl;
        } else if (ext == ".zip") {
            auto archive_contents = get_archive_content(file_buffer);
            for (int i = 0; i < archive_contents.size(); ++i) {
                auto cur_ext = boost::filesystem::extension(archive_contents[i]);
                if (cur_ext == ".txt") {
                    file_content = get_archive_file_contents(archive_contents[i], archive_contents, file_buffer);
                    file_content = convert_to_normalized_utf_string(file_content);
                }
            }
        }

        word_count_to_map(file_content, concurrentMap);
    }

}


int main(int argc, char *argv[]) {
    auto total_start = get_current_time_fenced();
    std::string filename;
    if (argc > 2) {
        std::cout << "Wrong number of command line arguments, got" + std::to_string(argc - 1) + ", needed 1"
                  << std::endl;
        return 1;
    } else if (argc == 1) {
        filename = "config.dat";
    } else {
        filename = argv[1];
    }
    stem_path(filename);
    try {
        check_path_exists(filename);
    } catch (FileDoesNotExistsException &ex) {
        std::cout << ex.what();
        return 2;
    }
    std::vector <std::string> compulsory_keys = {"indir", "out_by_value", "out_by_name", "indexing_threads",
                                                 "merging_threads", "max_names_queue_size", "max_files_queue_size",
                                                 "max_words_queue_size"};
    std::map <std::string, std::string> config;
    try {
        config = parseConfig(filename, compulsory_keys);
    } catch (std::runtime_error &error) {
        std::cout << error.what();
        return 5;
    }
    auto in_dir_path = config["indir"];
    auto names_queue_size = std::stoi(config["max_names_queue_size"]);
    auto files_queue_size = std::stoi(config["max_files_queue_size"]);
    auto words_queue_size = std::stoi(config["max_words_queue_size"]);
    stem_path(in_dir_path);
    try {
        check_path_exists(in_dir_path);
    } catch (FileDoesNotExistsException &ex) {
        std::cout << ex.what();
        return 2;
    }
    concurrent_queue <std::string> file_names_q(names_queue_size);
    concurrent_queue <std::pair<std::string, std::string>> raw_files_q(files_queue_size);
    auto concurrentMap = ConcurrentHashmap<std::string, size_t>();

    std::mutex indexing_mutex;

    std::vector <std::thread> merging_threads;
    std::vector <std::thread> indexing_threads;
    boost::locale::generator gen;
    gen.locale_cache_enabled(true);
    std::locale system_locale = gen("");
    std::locale::global(system_locale);
    auto indexing_threads_num = stoi(config["indexing_threads"]);
    int current_indexing_threads = indexing_threads_num;
    auto merging_threads_num = stoi(config["merging_threads"]);

    std::thread enum_t(enum_thread, in_dir_path, std::ref(file_names_q));
    std::thread read_t(file_reading_thread, std::ref(file_names_q), std::ref(raw_files_q));

    for (int i = 0; i < indexing_threads_num; ++i) {
        indexing_threads.emplace_back(indexing_thread<std::string, size_t>, std::ref(indexing_mutex),
                                      std::ref(raw_files_q), std::ref(concurrentMap));
    }


    enum_t.join();
    read_t.join();

    for (auto &indexing_thread : indexing_threads) {
        indexing_thread.join();
    }

    for (auto &merge_thread : merging_threads) {
        merge_thread.join();
    }

    auto result = concurrentMap.getMap();
    std::cout << result.size() << std::endl;

    auto name_sorted_ouf_file_path = config["out_by_name"];
    auto value_sorted_ouf_file_path = config["out_by_value"];

    stem_path(name_sorted_ouf_file_path);
    stem_path(value_sorted_ouf_file_path);

    auto vector_result = map_to_vector(result);

    auto total_finish = get_current_time_fenced();

    sort(vector_result.begin(), vector_result.end(), sort_by_first);
    write_file(name_sorted_ouf_file_path, vector_result);

    sort(vector_result.begin(), vector_result.end(), sort_by_sec);
    write_file(value_sorted_ouf_file_path, vector_result);

    std::cout << "Total: " << to_us(total_finish - total_start) << std::endl;

    return 0;
}