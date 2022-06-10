#include <vector>
#include <array>
#include <iostream>
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

    bool try_send(const T& msg)
    {
        return queue_try_add(&*handle, &msg);
    }

    std::optional<T> try_receive()
    {
        std::array<char, sizeof(T)> buffer;

        if (queue_try_remove(&*handle, buffer.data()))
            return *reinterpret_cast<T*>(buffer.data());
        else
            return std::nullopt;
    }

    ~Queue()
    {
        if (handle)
            queue_free(&*handle);
    }

private:

    std::optional<queue_t> handle;
}; 
}


const uint LED_PIN = PICO_DEFAULT_LED_PIN;
RPi::Queue<char> input_stream;


class Parser
{
public:

    std::vector<std::string> parse(std::string_view const line)
    {
        buffer.append(line);

        std::string_view delim = "abc";

        size_t prev = 0;
        size_t next = buffer.find_first_of(delim);

        std::vector<std::string> data;

        while (next != std::string::npos)
        {
            data.push_back(buffer.substr(prev, next - prev));
            prev = next + delim.size();
            next = buffer.find_first_of(delim, prev);
        }

        if (prev != 0)
            buffer = buffer.substr(prev);

        std::cout << "Buffer: " << buffer << std::flush;

        return data;
    }

private:

    std::string buffer;
};


void input_loop()
{
    Parser parser;

    while (true)
    {
        std::array<char, 256> input_buffer;
        input_buffer.back() = '\0';
        fread(input_buffer.data(), 1, input_buffer.size()-1, stdin);
        std::string_view line { input_buffer.data() };

        for (auto const& msg : parser.parse(line))
        {
            std::cout << msg << std::flush;
            sleep_ms(100);
        }
    }
}


void output_loop()
{
    while (true)  
    {
        gpio_put(LED_PIN, 1);
        sleep_ms(750);
        gpio_put(LED_PIN, 0);
        sleep_ms(250);
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