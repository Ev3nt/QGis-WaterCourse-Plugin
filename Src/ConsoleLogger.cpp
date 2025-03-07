#include "pch.h"

#include "ConsoleLogger.h"

ConsoleLogger::ConsoleLogger(): buffer_(std::cout.rdbuf()) {
    std::cout.rdbuf(&buffer_);
}

void ConsoleLogger::setCallback(CallbackType callback) {
	callback_ = callback;
}

ConsoleLogger::ProxyBuffer::~ProxyBuffer() {
    keepRunning_ = false;
    cv_.notify_all();

    if (thread_.joinable()) {
        thread_.join();
    }
}

ConsoleLogger::ProxyBuffer::int_type ConsoleLogger::ProxyBuffer::overflow(int_type ch) {
    if (ch != traits_type::eof()) {
        std::thread::id threadId = std::this_thread::get_id();
        std::unique_lock<std::mutex> lock(mutex_);

        if (ownerThreadId_ != threadId) {
            cv_.wait(lock, [this] { return !isLocked_.load(std::memory_order_relaxed); });
        }

        ownerThreadId_ = threadId;
        isLocked_ = true;

        string_ += ch;
        
        if (ch == '\n') {
            CallbackType callback = getInstance().callback_;
            if (callback) {
                string_.pop_back();

                queue_.push(string_);
                cv_.notify_one();

                string_.clear();

                isLocked_ = false;
                ownerThreadId_ = std::thread::id();
                cv_.notify_all();
            }
        }
    }

    return ch;
}

void ConsoleLogger::ProxyBuffer::processQueue() {
    while (keepRunning_) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]{ return !queue_.empty() || !keepRunning_; });

        if (!keepRunning_) {
            break;
        }
        
        std::string message = queue_.front().data();
        queue_.pop();

        getInstance().callback_(message.data());

        cv_.notify_one();
    }
}

/*int ConsoleLogger::ProxyBuffer::sync() {
    CallbackType callback = getInstance().callback_;
    if (callback) {
        string_.pop_back();
        callback(string_.data());

        string_.clear();
    }
    
    return buffer_->pubsync();
}*/

EXPORT_API void SetLogOutputCallback(ConsoleLogger::CallbackType callback) {
	ConsoleLogger::getInstance().setCallback(callback);
}