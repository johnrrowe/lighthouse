#include <cstdio>
#include <cstring>
#include <mutex>
#include <optional>
#include <queue>
#include <ranges>
#include <string>
#include <sstream>
#include <string_view>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>
#include <httplib.h>

// for convenience
using json = nlohmann::json;

size_t save_response(char * ptr, size_t size, size_t nmemb, void *userdata) 
{
    auto& buffer { *reinterpret_cast<std::stringstream*>(userdata) };
    buffer << std::string(ptr, size*nmemb);
    return size*nmemb;
}


template <typename T>
class Queue
{
public:

    void send(const T& msg)
    {
        std::lock_guard<std::mutex> lock { mtx };
        queue.push(msg);
    }

    std::optional<T> receive()
    {

        std::lock_guard<std::mutex> lock { mtx };

        if (queue.empty())
        {
            return std::nullopt;
        }
        else
        {
            T msg { queue.front() };
            queue.pop();
            return msg;
        }
    }

private:

    std::queue<T> queue;
    std::mutex mtx;
};


std::optional<std::string> reauthorize_user(httplib::Client& client)
{
    httplib::Params params {
        { "client_id", "9896166d446e4b4590f28f223213cabd&" },
        { "scope", "user-read-currently-playing&" },
        { "response_type", "code&" },
        { "redirect_uri", "http://localhost:8080/authorization_code/" }
    };

    const httplib::Result response { client.Get(
        "/authorize",
        params,
        httplib::Headers{}
    ) };
    if (!response)
        return std::nullopt;

    auto redirect_header { response->headers.find("location") }; 
    if (redirect_header == response->headers.end())
        return std::nullopt;

    std::string redirect { redirect_header->second };

    const std::string_view path = "/login?continue=";
    size_t path_start { redirect.find(path) };
    size_t delimiter { redirect.find_first_of(":/&?=", path_start + path.size()) };

    auto url_encode = [] (char c) -> std::string {
        switch (c)
        {
        case '&':
            return "26";
        case '/':
            return "2F";
        case ':':
            return "3A";
        case '=':
            return "3D";
        case '?':
            return "3F";
        default:
            throw std::runtime_error("Error failed to url encode this char: %c" + c);
        }
    }; 

    while (delimiter != std::string::npos)
    {
        redirect.insert(delimiter + 1, url_encode(redirect[delimiter]));
        redirect[delimiter] = '%';
        delimiter = redirect.find_first_of(":/&?=", delimiter);
    }

    return redirect;
}


std::optional<std::string> request_track_info(httplib::Client& client, std::string const& access_token)
{
    httplib::Result response { client.Get("/v1/me/player/currently-playing", httplib::Headers{
        { "Authorization", "Bearer " + access_token }
    }) };

    if (!response)
        return std::nullopt;

    if (response->body.empty())
        return "No active track";

    try 
    {
        json current_track = json::parse(response->body)["item"];
        std::stringstream info;

        std::string track_name { current_track["name"] };
        info << "Currently Playing " << track_name << '\n';

        auto& artists (current_track["artists"]);
        info << "By " << std::string((*artists.begin())["name"]);
        for (auto& artist : std::ranges::subrange(++artists.begin(), artists.end()))
            info << ", " << std::string(artist["name"]);

        return info.str();
    }
    catch (...)
    {
        return std::nullopt;
    }
}


int main()
{
    using namespace httplib;
    Client account_client ("https://accounts.spotify.com");
    Server svr;

    Queue<std::string> access_token_ch;

    svr.Get("/authorization_code/", [&account_client, &access_token_ch](Request const& req, Response &) {
        auto code = req.params.find("code");
        if (code == req.params.end())
            return;

        httplib::Headers headers {
            { "Authorization", "Basic OTg5NjE2NmQ0NDZlNGI0NTkwZjI4ZjIyMzIxM2NhYmQ6ZDc1ZjZjMGQ0ZjY4NGQyMGE1ODdhZjNiMGYzYmY1MWU=" },
        };

        const std::string body {
            "grant_type=authorization_code&code=" + code->second + "&redirect_uri=http://localhost:8080/authorization_code/"
        };

        const httplib::Result response { account_client.Post(
            "/api/token",
            headers,
            body.data(),
            body.size(),
            "application/x-www-form-urlencoded"
        ) };

        if (response)
        {
            try
            {
                auto obj { json::parse(response->body) };
                access_token_ch.send(std::string(obj[0]["access_token"]));
            }
            catch (...)
            {

            }
        }
    });

    std::jthread client_thread { 
        [&account_client, &access_token_ch] () 
        {
            if (auto redirect = reauthorize_user(account_client))
                printf("%s\n", redirect->c_str());

            httplib::Client client { "https://api.spotify.com" };
            std::optional<std::string> access_token;

            while (true)
            {
                if (access_token)
                {
                    if (auto info = request_track_info(client, *access_token))
                    {
                        std::system("clear");
                        printf("%s\n", info->c_str());
                    }
                    else
                    {
                        access_token = std::nullopt;
                    }
                }
                else
                {
                    if (access_token = access_token_ch.receive())
                        printf("\nNew Access Token: %s\n", access_token->c_str());
                }

                usleep(500'000);
            } 
        } 
    };

    svr.listen("127.0.0.1", 8080);

    return 0;
}