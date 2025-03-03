#include <iostream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <random>
#include <iterator>
#include <filesystem>
#include <chrono>
#include <thread>
#include <fstream>
#include <stdexcept>
#include <sstream>

namespace fs = std::filesystem;
#define LENGTH_OF_USER 5
#define AMOUNT_OF_USER 50
#define VALID_USER_FILE "valid_users.txt"
#define BIRTHDAY_REG "2002-02-02"
#define REQUEST_DELAY 350
// ^^ in milliseconds
using nlohmann::json;

size_t writeFunction(void* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

std::string send_request(const std::string& url) {
    auto curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response_string;
    std::string header_string;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_string);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("CURL request failed: " + std::string(curl_easy_strerror(res)));
    }

    curl_easy_cleanup(curl);
    return response_string;
}

bool check_user(const std::string& username, const std::string& birthday) {
    std::string url = "https://auth.roblox.com/v1/usernames/validate?request.username=" + username + "&request.birthday=" + birthday;
    std::string payload = send_request(url);

    try {
        json j = json::parse(payload);
        if (j.contains("code")) {
            int code = j["code"];
            return code != 0;
        }
    }
    catch (const json::parse_error& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
    }

    return false;
}

template<typename Iter, typename RandomGenerator>
Iter select_randomly(Iter start, Iter end, RandomGenerator& g) {
    std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
    std::advance(start, dis(g));
    return start;
}

template<typename Iter>
Iter select_randomly(Iter start, Iter end) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return select_randomly(start, end, gen);
}

std::string int_to_string(int to) {
    std::stringstream ss;
    ss << to;
    return ss.str();
}
int main() {
    std::string path = (fs::current_path() / fs::path(VALID_USER_FILE)).string();
    std::cout << "Using file " << path << "\n";
    std::ofstream result_file(fs::current_path() / fs::path(VALID_USER_FILE));
    if (!result_file.is_open()) {
        std::cerr << "Failed to open file\n";
        return 1;
    }

    std::vector<char> chars;
    for (char ch = 'a'; ch <= 'z'; ++ch) {
        chars.push_back(ch);
    }
    for (char ch = '0'; ch <= '9'; ++ch) {
        chars.push_back(ch);
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    std::cout << "[+] Starting with " <<int_to_string(AMOUNT_OF_USER) << " usernames " << int_to_string(LENGTH_OF_USER) << " characters long" << std::endl;

    int total_valid = 0;
    try {
        while (total_valid < AMOUNT_OF_USER) {
            std::string result_username;
            for (int i = 0; i < LENGTH_OF_USER; ++i) {
                result_username += *select_randomly(chars.begin(), chars.end());
            }

            bool taken = check_user(result_username, BIRTHDAY_REG);
            if (taken) {
                std::cout << "[-] Username " << result_username << " taken." << std::endl;
            }
            else {
                std::cout << "[+] Username " << result_username << " is available. Writing to file... (" << total_valid << "/" << AMOUNT_OF_USER << ")" << std::endl;
                result_file << result_username << "\n";
                result_file.flush();
                ++total_valid;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(REQUEST_DELAY));
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    std::cout << "[END] Found " << AMOUNT_OF_USER << " accounts. Exiting...\n";
    result_file.close();
    curl_global_cleanup();
    return 0;
}