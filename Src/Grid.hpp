#pragma once

#include <stdexcept>
#include <vector>

template<typename T>
class Grid : public std::vector<T> {
public:
	Grid(size_t width, size_t height, T value = 0) : std::vector<T>(width* height, value), width_(width), height_(height) {

	}

	Grid() = default;

	T* operator[](size_t index) {
		if (index >= width_ * height_ || index < 0) {
			//throw std::out_of_range("Grid: out of range.");

			return nullptr;
		}

		return std::vector<T>::operator[](index);
	}

	T* at(size_t x, size_t y) {
		if (x < 0 || x >= width_ || y < 0 || y >= height_) {
			//throw std::out_of_range("Grid: out of range.");

			return nullptr;
		}

		return &std::vector<T>::operator[](y* width_ + x);
	}

	void resize(size_t width, size_t height) {
		width_ = width;
		height_ = height;

		std::vector<T>::resize(width * height);
	}

	size_t getWidth() {
		return width_;
	}

	size_t getHeight() {
		return height_;
	}

private:
	size_t width_ = 0;
	size_t height_ = 0;
};