#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <ranges>
#include <string>
#include <sstream>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-default"
#pragma GCC diagnostic ignored "-Wcast-qual"
#include <httplib.h>
#pragma GCC diagnostic pop

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

    std::string redirect { std::move(redirect_header->second) };

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

    const std::string_view path = "/login?continue=";
    size_t path_start { redirect.find(path) };
    size_t delimiter { redirect.find_first_of(":/&?=", path_start + path.size()) };

    while (delimiter != std::string::npos)
    {
        redirect.insert(delimiter + 1, url_encode(redirect[delimiter]));
        redirect[delimiter] = '%';
        delimiter = redirect.find_first_of(":/&?=", delimiter);
    }

    return redirect;
}


std::optional<httplib::Response> request_track_info(httplib::Client& client, std::string const& access_token)
{
    httplib::Result response { client.Get("/v1/me/player/currently-playing", httplib::Headers{
        { "Authorization", "Bearer " + access_token }
    }) };

    if (response)
        return *response;
    else
        return std::nullopt;
}


struct CurrentTrack
{
    std::string name;
    std::string id;
    std::vector<std::string> artist_names;
};


enum class ResponseError
{
    NoContent,
    ParseFailure
};


std::variant<CurrentTrack, ResponseError> parse_current_track(httplib::Response const& response)
{
    if (response.body.empty())
        return ResponseError::NoContent;

    try 
    {
        json current_track (json::parse(response.body)["item"]);

        auto artists = current_track["artists"]
            | std::views::transform([] (auto const& a) { return std::string(a["name"]); });

        std::vector<std::string> artist_names; 
        std::ranges::copy(artists, std::back_inserter(artist_names));

        return CurrentTrack{
            .name { current_track["name"] },
            .id   { current_track["id"] },
            .artist_names { artist_names },
        };

        // std::string track_name {  };
        // info << "Currently Playing " << track_name << '\n';

        // auto const& artists (current_track["artists"]);
        // info << "By " << std::string((*artists.begin())["name"]);
        // for (auto& artist : std::ranges::subrange(++artists.begin(), artists.end()))
        //     info << ", " << std::string(artist["name"]);
    }
    catch (...)
    {
        return ResponseError::ParseFailure;
    }
}


void display(std::string const& str)
{
    std::system("clear");
    std::cout << str << '\n';
}


std::string get_track_analysis(httplib::Client& client, std::string const& access_token, std::string const& track_id)
{
    httplib::Result response { client.Get(
        ("/v1/audio-analysis/" + track_id).c_str(),
        httplib::Headers{ { "Authorization", "Bearer " + access_token } }
    ) };

    if (!response)
    {
        return "Failed to get track analysis";
    }

    try
    {
        json track_info (json::parse(response->body));

        auto stringify_segment = [] (auto const& seg)
        { 
            auto pitches { seg["pitches"] | std::views::transform([] (auto const& p) { return double(p); }) };
            auto timbre { seg["timbre"] | std::views::transform([] (auto const& p) { return double(p); }) };

            std::stringstream s;
            s << "pitches: ";
            std::ranges::copy(pitches, std::ostream_iterator<double>(s, " "));
            s << "\ntimbre: ";
            std::ranges::copy(timbre, std::ostream_iterator<double>(s, " "));
            return s.str();
        };

        auto segments { track_info["segments"] | std::views::transform(stringify_segment) }; 

        std::stringstream info;
        std::ranges::copy(segments, std::ostream_iterator<std::string>(info, "\n\n"));
        return info.str();
    }
    catch (std::exception const& e)
    {
        return "Failed to parse track info\n" + std::string(e.what()); 
    }
}


int main()
{
    httplib::Client account_client ("https://accounts.spotify.com");
    httplib::Server svr;

    Queue<std::string> access_token_ch;

    svr.Get("/authorization_code/", [&account_client, &access_token_ch](httplib::Request const& req, httplib::Response &) {
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
            httplib::Client client { "https://api.spotify.com" };

            std::optional<std::string> redirect_link;
            while (!redirect_link)
            {
                if (redirect_link = reauthorize_user(account_client))
                    std::cout << *redirect_link << '\n';
                else
                    usleep(500'000);
            }

            while (true)
            {
                std::optional<std::string> access_token;

                while (!access_token)
                {
                    if (access_token = access_token_ch.receive())
                        std::cout << "\nNew Access Token: " << *access_token << '\n';
                    else
                        usleep(500'000);
                }

                std::string recent_track_id;

                while (auto track_response = request_track_info(client, *access_token))
                {
                    auto res { parse_current_track(*track_response) };

                    if (auto* current_track = std::get_if<CurrentTrack>(&res))
                    {
                        std::stringstream track_info;

                        track_info << "Now playing " << current_track->name
                                   << "\nBy " << current_track->artist_names.front(); 
                        for (auto const& name : std::ranges::subrange(current_track->artist_names.begin() + 1, current_track->artist_names.end()))
                            track_info << ", " << name;

                        track_info << "\n\nTrack Info\n\n" << get_track_analysis(client, *access_token, current_track->id);

                        if (recent_track_id != current_track->id)
                        {
                            recent_track_id = current_track->id;
                            display(track_info.str());
                        }
                    }
                    else if (auto* err = std::get_if<ResponseError>(&res))
                    {
                        recent_track_id = "";
                        switch (*err) 
                        {
                        case ResponseError::NoContent:
                            display("No currently active track");
                            break;
                        case ResponseError::ParseFailure:
                            display("Failed to parse current track response");
                            break;
                        default:
                            display("InternalError: Unhandled ResponseError when requesting current track");
                            break;
                        }
                    }

                    usleep(500'000);
                }
            } 
        } 
    };

    svr.listen("127.0.0.1", 8080);

    return 0;
}