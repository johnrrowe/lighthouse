#include <vector>
#include <array>
#include <iostream>
#include <numeric>
#include <optional>
#include <ranges>
#include <string>

#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <pico/util/queue.h>


namespace RPi
{
template <typename T>
class Queue 
{
public:

    Queue() = default;

    Queue(uint count)
    {
        queue_t h;
        queue_init(&h, sizeof(T), count);
        handle = h;
    }

    bool try_send(T const& msg)
    {
        return queue_try_add(&*handle, &msg);
    }

    T* try_receive()
    {
        if (queue_try_remove(&*handle, receive_buffer))
            return *reinterpret_cast<T*>(receive_buffer);
        else
            return nullptr;
    }

    void send(T const& msg)
    {
        queue_add_blocking(&*handle, &msg);
    }

    T& receive()
    {
        queue_remove_blocking(&*handle, receive_buffer);
        return *reinterpret_cast<T*>(receive_buffer);
    }

    ~Queue()
    {
        if (handle)
            queue_free(&*handle);
    }

private:

    alignas(T) unsigned char receive_buffer[sizeof(T)];
    std::optional<queue_t> handle;
}; 
}


const uint LED_PIN = PICO_DEFAULT_LED_PIN;
RPi::Queue<std::array<char, 256>> input_stream(5);


class Parser
{
public:

    std::vector<std::string> parse(std::string_view const line, std::string_view const delimiter)
    {
        buffer.append(line);

        std::vector<std::string> msgs;
        {
            auto convert_to_str = [] (auto const& v)
            {
                // std::string ctor requires begin() and end() to be the same type
                // to achieve this we convert to a common range 
                auto common = std::views::common(v);
                return std::string(common.begin(), common.end());
            };

            auto split_input = std::views::split(buffer, delimiter)
                    | std::views::transform(convert_to_str);

            std::copy(split_input.begin(), split_input.end(), std::back_inserter(msgs));
        }

        if (buffer.ends_with(delimiter))
        {
            buffer.clear();
            return msgs;
        }
        else
        {
            buffer = msgs.back();
            msgs.pop_back();
            return msgs;
        }
    }

private:

    std::string buffer;
};


void input_loop()
{
    while (true)
    {
        std::array<char, 256> input_buffer;
        fread(input_buffer.data(), 1, input_buffer.size()-1, stdin);
        input_buffer.back() = '\0';
        input_stream.try_send(input_buffer);
    }
}


void output_loop()
{
    Parser parser;
    
    auto convert_to_cmd = [] (std::string const& s)
    {
        if (s == "on")
            return true;
        else if (s == "off")
            return false;
        else
            std::cout << "Internal Device Error: tried to convert invalid input to a boolean" << std::flush;
        return false;
    };

    auto valid_input = [] (std::string const& s)
    {
        if (s == "on" || s == "off")
            return true;
        else 
            return false;
    };


    while (true)  
    {
        std::string_view line { input_stream.receive().data() };

        auto lines = parser.parse(line, "<:>");

        auto commands = lines
            | std::views::filter(valid_input)
            | std::views::transform(convert_to_cmd);

        if (!commands.empty())
        {
            gpio_put(LED_PIN, commands.back());
        }
    }
}


int main() 
{
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    stdio_init_all();

    // [[maybe_unused]]

    multicore_launch_core1(input_loop);

    output_loop();
}