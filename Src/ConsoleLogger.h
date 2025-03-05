#pragma once

#include <streambuf>

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
        ProxyBuffer(std::streambuf* buffer) : buffer_(buffer) {}

    protected:
        virtual int_type overflow(int_type ch);

        virtual int sync();

    private:
        std::streambuf* buffer_;
        std::string string_;
    };

	CallbackType callback_ = nullptr;
    ProxyBuffer buffer_;
};