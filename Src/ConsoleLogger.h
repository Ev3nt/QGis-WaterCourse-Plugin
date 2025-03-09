#pragma once

#include <streambuf>
#include <condition_variable>
#include <mutex>
#include <queue>

class ConsoleLogger {
public:
	typedef void (*CallbackType)(const char*);

	static ConsoleLogger& getInstance() {
		static ConsoleLogger consoleLogger;
		
		return consoleLogger;
	}

	void setCallback(CallbackType callback);

private:
    ConsoleLogger();

    class ProxyBuffer : public std::streambuf {
    public:
        ProxyBuffer(std::streambuf* buffer) : buffer_(buffer), thread_(&ProxyBuffer::processQueue, this), keepRunning_(true) {}
        ~ProxyBuffer();

    protected:
        virtual int_type overflow(int_type ch);

        //virtual int sync();

    private:
        void processQueue();

        std::streambuf* buffer_;
        std::string string_;
        std::queue<std::string> queue_;

        std::mutex mutex_;
        std::atomic_bool keepRunning_;
        std::condition_variable processConditionVariable_, queueConditionVariable_;

        std::thread thread_;
        std::thread::id ownerThreadId_;
        std::atomic_bool isLocked_ = false;
    };

	CallbackType callback_ = nullptr;
    ProxyBuffer buffer_;
};