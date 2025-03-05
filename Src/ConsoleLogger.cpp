#include "pch.h"

#include "ConsoleLogger.h"

ConsoleLogger::ConsoleLogger(): buffer_(std::cout.rdbuf()) {
    std::cout.rdbuf(&buffer_);
}

void ConsoleLogger::setCallback(CallbackType callback) {
	callback_ = callback;
}

ConsoleLogger::ProxyBuffer::int_type ConsoleLogger::ProxyBuffer::overflow(int_type ch) {
    if (ch != traits_type::eof()) {
        string_.append(1, (char)ch);
    }

    return ch;
}

int ConsoleLogger::ProxyBuffer::sync() {
    CallbackType callback = getInstance().callback_;
    if (callback) {
        string_.pop_back();
        callback(string_.data());

        string_.clear();
    }
    
    return buffer_->pubsync();
}

EXPORT_API void SetLogOutputCallback(ConsoleLogger::CallbackType callback) {
	ConsoleLogger::getInstance().setCallback(callback);
}