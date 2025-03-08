#include "pch.h"

#include "ConsoleLogger.h"

ConsoleLogger::ConsoleLogger(): buffer_(std::cout.rdbuf()) {
    std::cout.rdbuf(&buffer_);
}

void ConsoleLogger::setCallback(CallbackType callback) {
	callback_ = callback;
}

ConsoleLogger::ProxyBuffer::~ProxyBuffer() {
    queueConditionVariable_.notify_all();
}

ConsoleLogger::ProxyBuffer::int_type ConsoleLogger::ProxyBuffer::overflow(int_type ch) {
    if (ch != traits_type::eof()) {
        std::thread::id threadId = std::this_thread::get_id();
        std::unique_lock<std::mutex> lock(mutex_);

        if (ownerThreadId_ != threadId) {
            queueConditionVariable_.wait(lock, [this] { return !isLocked_.load(std::memory_order_relaxed); });
        }

        ownerThreadId_ = threadId;
        isLocked_ = true;

        string_ += ch;
        
        if (ch == '\n') {
            CallbackType callback = getInstance().callback_;
            if (callback) {
                string_.pop_back();

                getInstance().callback_(string_.data());

                string_.clear();

                isLocked_ = false;
                ownerThreadId_ = std::thread::id();
                queueConditionVariable_.notify_one();
            }
        }
    }

    return ch;
}

EXPORT_API void SetLogOutputCallback(ConsoleLogger::CallbackType callback) {
	ConsoleLogger::getInstance().setCallback(callback);
}