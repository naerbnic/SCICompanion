#include "stdafx.h"
#include "Logger.h"

static thread_local Logger::Handler* curr_thread_handler = nullptr;


Logger::HandlerScope::HandlerScope(Handler* handler)
    : old_handler_(curr_thread_handler)
{
    curr_thread_handler = handler;
}

Logger::HandlerScope::~HandlerScope()
{
    curr_thread_handler = old_handler_;
}

void Logger::LogImpl(const LogMessage& message)
{
    if (curr_thread_handler)
    {
        curr_thread_handler->Log(message);
    }
}