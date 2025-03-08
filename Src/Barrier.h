#pragma once

#include <mutex>
#include <condition_variable>
#include <functional>

class Barrier {
public:
	Barrier() = default;
	
	template<typename _Predicate>
	void wait(int threadsCount, _Predicate pred) {
		std::unique_lock lock(mutex_);

		if (threadsCount_ == 0) {
			threadsCount_ = threadsCount;
			pred_ = pred;
		}

		threadsCount_--;

		cv_.wait(lock, [this] { return !threadsCount_.load(std::memory_order_relaxed) || pred_(); });
		cv_.notify_all();
	}

	void wait(int threadsCount) {
		std::unique_lock lock(mutex_);

		if (!threadsCount_) {
			threadsCount_ = threadsCount;
		}

		threadsCount_--;

		cv_.wait(lock, [this] { return !threadsCount_.load(std::memory_order_relaxed); });
		cv_.notify_all();
	}

private:
	std::mutex mutex_;
	std::condition_variable cv_;
	std::atomic_int threadsCount_ = 0;
	std::function<bool()> pred_;
};