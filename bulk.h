#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

namespace bulk {

typedef struct
{
    std::string _text;
    std::chrono::system_clock::time_point _inition_timestamp;
} command, *pcommand;

class ICommandProcessor
{
public:
    ICommandProcessor(ICommandProcessor* next_command_processor = nullptr) : _next_command_processor(next_command_processor) {}

    virtual ~ICommandProcessor() = default;

    virtual void start_block() {}
    virtual void finish_block() {}

    virtual void command_handler(const command& command) = 0;

protected:
    ICommandProcessor* _next_command_processor;
};

class ConsoleInput : public ICommandProcessor
{
public:
    ConsoleInput(ICommandProcessor* next_command_processor = nullptr) : ICommandProcessor(next_command_processor), _block_depth(0) {}

    void command_handler(const command& command) override
    {
        if (_next_command_processor)
        {
            if (command._text == "{")
            {
                if (_block_depth++ == 0) _next_command_processor->start_block();
            }
            else if (command._text == "}")
            {
                if (--_block_depth == 0) _next_command_processor->finish_block();
            }
            else if (command._text == "EOF")
            {
                _block_depth = 0;
                _next_command_processor->command_handler(command);
            }
            else _next_command_processor->command_handler(command);
        }
    }

private:
    size_t _block_depth;
};

class ConsoleOutput : public ICommandProcessor
{
public:
    ConsoleOutput(ICommandProcessor* next_command_processor = nullptr) : ICommandProcessor(next_command_processor) {}

    void command_handler(const command& command) override
    {
        std::cout << command._text << std::endl;
        if (_next_command_processor) _next_command_processor->command_handler(command);
    }
};

class FileOutput : public ICommandProcessor
{
public:
    FileOutput(ICommandProcessor* next_command_processor = nullptr) : ICommandProcessor(next_command_processor) {}

    void command_handler(const command& command) override
    {
        if(!command._text.empty())
        {
            std::ofstream file(create_filename(command), std::ofstream::out | std::ios::binary);
            file << command._text;
            if (_next_command_processor) _next_command_processor->command_handler(command);
        }
    }

private:
    std::string create_filename(const command& command)
    {
        return "bulk" + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(command._inition_timestamp.time_since_epoch()).count()) + ".log";
    }
};

class CommandProcessor : public ICommandProcessor
{
public:
    CommandProcessor(size_t bulk_max_depth, ICommandProcessor* next_command_processor) : ICommandProcessor(next_command_processor), _bulk_max_depth(bulk_max_depth), _used_block_command(false) {}

    ~CommandProcessor()
    {
        if (!_used_block_command) dump_commands();
    }

    void start_block() override
    {
        _used_block_command = true;
        dump_commands();
    }

    void finish_block() override
    {
        _used_block_command = false;
        dump_commands();
    }

    void command_handler(const command& command) override
    {
        _commands_pool.push_back(command);

        if (_used_block_command)
        {
            if (command._text == "EOF")
            {
                _used_block_command = false;
                _commands_pool.clear();
                return;
            }
        }
        else
        {
            if (command._text == "EOF" || _commands_pool.size() >= _bulk_max_depth)
            {
                dump_commands();
                return;
            }
        }
    }
private:
    void clear_commands_pool()
    {
        _commands_pool.clear();
    }

    void dump_commands()
    {
        if (_next_command_processor && !_commands_pool.empty())
        {
            auto commands_concat = concatenate_commands_pool();
            auto output =  !commands_concat.empty() ? "bulk: " + concatenate_commands_pool() : "";
            _next_command_processor->command_handler(command{output, _commands_pool[0]._inition_timestamp});
        }
        clear_commands_pool();
    }

    std::string concatenate_commands_pool()
    {
        std::string result;
        for(auto command: _commands_pool)
        {
            if (!command._text.empty() && !result.empty()) result += ", ";
            result += command._text;
        }
        return result;
    }

    size_t _bulk_max_depth;
    bool _used_block_command;
    std::vector<command> _commands_pool;
};

};

class Handler 
{
public:
    Handler(size_t bulk_max_depth)
    {
        bulk::FileOutput file_output;
        bulk::ConsoleOutput console_output(&file_output);
        bulk::CommandProcessor command_processor(bulk_max_depth, &console_output);
        bulk::ConsoleInput console_input(&command_processor);
        std::string command_text;
        while (std::getline(std::cin, command_text)) 
        {
            console_input.command_handler(bulk::command{command_text, std::chrono::system_clock::now()});
            std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(1000));
        }
    }
};
