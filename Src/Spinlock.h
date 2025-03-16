#pragma once

#include <atomic>
#include <thread>

class Spinlock {
	std::atomic_flag lock_flag;
public:
	Spinlock() {
		lock_flag.clear();
	}

	bool try_lock() {
		return !lock_flag.test_and_set(std::memory_order_acquire);
	}

	void lock() {
		for (size_t i = 0; !try_lock(); ++i) {
			if (i++ % 1000 == 0) {
				std::this_thread::yield();
			}
			else {
				_mm_pause();
			}
		}
	}

	void unlock() {
		lock_flag.clear(std::memory_order_release);
	}
};