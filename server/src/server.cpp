#include <algorithm>
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


// int main()
// {
//     httplib::Client account_client ("https://accounts.spotify.com");
//     httplib::Server svr;

//     Queue<std::string> access_token_ch;

//     svr.Get("/authorization_code/", [&account_client, &access_token_ch](httplib::Request const& req, httplib::Response &) {
//         auto code = req.params.find("code");
//         if (code == req.params.end())
//             return;

//         httplib::Headers headers {
//             { "Authorization", "Basic OTg5NjE2NmQ0NDZlNGI0NTkwZjI4ZjIyMzIxM2NhYmQ6ZDc1ZjZjMGQ0ZjY4NGQyMGE1ODdhZjNiMGYzYmY1MWU=" },
//         };

//         const std::string body {
//             "grant_type=authorization_code&code=" + code->second + "&redirect_uri=http://localhost:8080/authorization_code/"
//         };

//         const httplib::Result response { account_client.Post(
//             "/api/token",
//             headers,
//             body.data(),
//             body.size(),
//             "application/x-www-form-urlencoded"
//         ) };

//         if (response)
//         {
//             try
//             {
//                 auto obj { json::parse(response->body) };
//                 access_token_ch.send(std::string(obj[0]["access_token"]));
//             }
//             catch (...)
//             {

//             }
//         }
//     });

//     std::jthread client_thread { 
//         [&account_client, &access_token_ch] () 
//         {
//             httplib::Client client { "https://api.spotify.com" };

//             std::optional<std::string> redirect_link;
//             while (!redirect_link)
//             {
//                 if (redirect_link = reauthorize_user(account_client))
//                     std::cout << *redirect_link << '\n';
//                 else
//                     usleep(500'000);
//             }

//             while (true)
//             {
//                 std::optional<std::string> access_token;

//                 while (!access_token)
//                 {
//                     if (access_token = access_token_ch.receive())
//                         std::cout << "\nNew Access Token: " << *access_token << '\n';
//                     else
//                         usleep(500'000);
//                 }

//                 std::string recent_track_id;

//                 while (auto track_response = request_track_info(client, *access_token))
//                 {
//                     auto res { parse_current_track(*track_response) };

//                     if (auto* current_track = std::get_if<CurrentTrack>(&res))
//                     {
//                         std::stringstream track_info;

//                         track_info << "Now playing " << current_track->name
//                                    << "\nBy " << current_track->artist_names.front(); 
//                         for (auto const& name : std::ranges::subrange(current_track->artist_names.begin() + 1, current_track->artist_names.end()))
//                             track_info << ", " << name;

//                         track_info << "\n\nTrack Info\n\n" << get_track_analysis(client, *access_token, current_track->id);

//                         if (recent_track_id != current_track->id)
//                         {
//                             recent_track_id = current_track->id;
//                             display(track_info.str());
//                         }
//                     }
//                     else if (auto* err = std::get_if<ResponseError>(&res))
//                     {
//                         recent_track_id = "";
//                         switch (*err) 
//                         {
//                         case ResponseError::NoContent:
//                             display("No currently active track");
//                             break;
//                         case ResponseError::ParseFailure:
//                             display("Failed to parse current track response");
//                             break;
//                         default:
//                             display("InternalError: Unhandled ResponseError when requesting current track");
//                             break;
//                         }
//                     }

//                     usleep(500'000);
//                 }
//             } 
//         } 
//     };

//     svr.listen("127.0.0.1", 8080);

//     return 0;
// }


// C library headers
#include <stdio.h>
#include <string.h>

// Linux headers
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // write(), read(), close()

int main() 
{
    // Open the serial port. Change device path as needed (currently set to an standard FTDI USB-UART cable type device)
    int serial_port = open("/dev/ttyACM0", O_RDWR);
    if(serial_port < 0) {
        printf("Error %i from open: %s\n", errno, strerror(errno));
        return 1;
    }

    // Create new termios struct, we call it 'tty' for convention
    struct termios tty;

    // Read in existing settings, and handle any error
    if(tcgetattr(serial_port, &tty) != 0) {
        printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
        return 1;
    }

    tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
    tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
    tty.c_cflag &= ~CSIZE; // Clear all bits that set the data size 
    tty.c_cflag |= CS8; // 8 bits per byte (most common)
    tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
    tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO; // Disable echo
    tty.c_lflag &= ~ECHOE; // Disable erasure
    tty.c_lflag &= ~ECHONL; // Disable new-line echo
    tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
    // tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT PRESENT ON LINUX)
    // tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars (0x004) in output (NOT PRESENT ON LINUX)

    tty.c_cc[VTIME] = 10;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
    tty.c_cc[VMIN] = 0;

    // Set in/out baud rate to be 115200
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    // Save tty settings, also checking for error
    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
        printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
        return 1;
    }

    Queue<std::string> lines;

    std::jthread user_input_thread {
        [&lines] () 
        {
            while (true)
            {
                std::string line;
                std::cin >> line;
                line.append(1, '\0');
                lines.send(line);
            }
        }
    };

    while (true)
    {
        // Allocate memory for read buffer, set size according to your needs
        std::array<char, 256> read_buf;
        std::ranges::fill(read_buf, '\0');

        // Read bytes. The behaviour of read() (e.g. does it block?,
        // how long does it block for?) depends on the configuration
        // settings above, specifically VMIN and VTIME
        const int num_bytes = read(serial_port, read_buf.data(), read_buf.size());
        read_buf[std::clamp(0, 255, num_bytes)] = '\0';

        if (num_bytes > 0)
            printf("Echo: %s\n", read_buf.data());

        while (auto line = lines.receive())
        {
            std::array<char, 256> output { '\0' };
            std::ranges::copy_n(line->begin(), std::min(output.size(), line->size()), output.begin());
            write(serial_port, output.data(), output.size());
        }

        // n is the number of bytes read. n may be 0 if no bytes were received, and can also be -1 to signal an error.
        if (num_bytes < 0) {
            printf("Error reading: %s", strerror(errno));
            close(serial_port);
            return 1;
        }
    }

    close(serial_port);
    return 0; // success
}