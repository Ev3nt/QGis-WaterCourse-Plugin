#pragma once

#include <string>

#include "Canvas.h"

typedef std::shared_ptr<Canvas<float>> CANVAS_FLOAT;
typedef std::shared_ptr<Canvas<uint8_t>> CANVAS_BYTE;

class Plugin {
public:
	static Plugin& getInstance() {
		static Plugin plugin;

		return plugin;
	}

	void process(const std::string& name, float scale, int threadsCount);

private:
	Plugin() = default;

	void directionProcess(CANVAS_FLOAT terrain, CANVAS_BYTE directions, int width, int height, int index, int threadsCount);

	std::optional<double> noData_;
	float scale_ = 1;
};