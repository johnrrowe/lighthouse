#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <sstream>
#include <string_view>
#include <vector>
#include <ranges>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

// for convenience
using json = nlohmann::json;


size_t save_response(char * ptr, size_t size, size_t nmemb, void *userdata) 
{
    auto& buffer { *reinterpret_cast<std::stringstream*>(userdata) };
    buffer << std::string(ptr, size*nmemb);
    return size*nmemb;
}


std::optional<std::string> refresh_token(CURL* curl)
{
    curl_easy_setopt(curl, CURLOPT_URL, "https://accounts.spotify.com/api/token");
    curl_easy_setopt(curl, CURLOPT_POST, 1L); 
    curl_slist *list = NULL;
    list = curl_slist_append(list, "Authorization: Basic OTg5NjE2NmQ0NDZlNGI0NTkwZjI4ZjIyMzIxM2NhYmQ6ZDc1ZjZjMGQ0ZjY4NGQyMGE1ODdhZjNiMGYzYmY1MWU=");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    
    const std::string_view body {
        "grant_type=refresh_token&"
        "refresh_token=AQCEPk6Vm62YLOxZiyFXSpZgmwUQUFgcRLffqOp39JaVamWJxHPV_EN3uzKbrwhqOXL_K4_zLGm7mFRkOUBt90uRH37quSxWPLUZs7iucmU-_uLS_ffl1ewvpuWXFPZnalI"
    };
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size()); 
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data()); 
    std::stringstream buffer;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, save_response);

    if (curl_easy_perform(curl) != CURLE_OK)
        return std::nullopt;
    
    try
    {
        auto obj { json::parse(buffer.str()) };
        return obj[0]["access_token"];
    }
    catch(const std::exception& e)
    {
        return std::nullopt;
    }
}


std::optional<std::string> request_track_info(CURL* curl, std::string const& access_token)
{
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.spotify.com/v1/me/player/currently-playing");
    curl_slist *list = NULL;
    list = curl_slist_append(list, ("Authorization: Bearer " + access_token).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    std::stringstream buffer;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, save_response);

    if (curl_easy_perform(curl) != CURLE_OK)
        return std::nullopt;

    try 
    {
        // json current_track = json::parse(buffer.str())["item"];
        // std::stringstream info;

        // std::string track_name { current_track["name"] };
        // info << "Currently Playing " << track_name << '\n';

        // auto& artists (current_track["artists"]);
        // info << "By " << std::string((*artists.begin())["name"]);
        // for (auto& artist : std::ranges::subrange(++artists.begin(), artists.end()))
        //     info << ", " << std::string(artist["name"]);

        // return info.str();
        return buffer.str();
    }
    catch (...)
    {
        return std::nullopt;
    }
}


int main()
{
    CURL *curl = curl_easy_init();
    if(curl) 
    {
        // {
        //     curl_easy_setopt(curl, CURLOPT_URL, 
        //         "https://accounts.spotify.com/authorize?"
        //         "client_id=9896166d446e4b4590f28f223213cabd&"
        //         "scope=user-read-currently-playing&"
        //         "response_type=code&"
        //         "redirect_uri=https://www.google.com/"
        //     );
        //     curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
        //     curl_easy_perform(curl);
        //     printf("Requested authorization\n\n");
        // }

        // curl_easy_reset(curl);

        // {
        //     curl_easy_setopt(curl, CURLOPT_URL, "https://accounts.spotify.com/api/token");
        //     curl_easy_setopt(curl, CURLOPT_POST, 1L); 
        //     curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
        //     curl_slist *list = NULL;
        //     list = curl_slist_append(list, "Authorization: Basic OTg5NjE2NmQ0NDZlNGI0NTkwZjI4ZjIyMzIxM2NhYmQ6ZDc1ZjZjMGQ0ZjY4NGQyMGE1ODdhZjNiMGYzYmY1MWU=");
        //     curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
        //     const std::string_view body {
        //         "grant_type=authorization_code&"
        //         "code=AQC7qVi9ZqqJs_qxl9yyYChnk4drWrfwYAQkkIpOg7ZOCRQxWJKrXabapvLeUY12hR0co9qhWGU0L6_-3CKSzF8EwX8NBH3WYNNBH2hVgsip_zcOofMhHpEbR89C5B4ePv4ZiOP-aczAlcITLTjrrdzEZz90xnMFaFdjnUWJiBwIKqWb4XdmNs4fuLXe--Pg0Q84NHuEbj4&"
        //         "redirect_uri=https://www.google.com/"
        //     };
        //     curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size()); 
        //     curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data()); 
        //     curl_easy_perform(curl);
        //     printf("\nRequested access token\n\n");
        // }

        // curl_easy_reset(curl);

        while (auto access_token = refresh_token(curl))
        {
            curl_easy_reset(curl);

            while (auto track_info = request_track_info(curl, *access_token))
            { 
                std::system("clear");

                if (!track_info->empty())
                    printf("%s\n", track_info->c_str());
                else 
                    printf("No currently active song\n");

                sleep(1);
            }

            std::system("clear");
            printf("Failed to request track info\n");
            curl_easy_reset(curl);
        }
        printf("\nFailed to refresh access token\n");
        // curl_free(body);
        curl_easy_cleanup(curl);
    }
    printf("\n");

    return 0;
}