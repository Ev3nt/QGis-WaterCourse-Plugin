#pragma once

#include <string>

#include <functional>
#include "Canvas.h"

typedef std::shared_ptr<Canvas<float>> CANVAS_FLOAT;
typedef std::shared_ptr<Canvas<uint8_t>> CANVAS_BYTE;
typedef std::shared_ptr<Canvas<uint32_t>> CANVAS_UINT32;
typedef std::pair<size_t, size_t> CHUNK_BORDERS;

class Plugin {
public:
	struct Source {
		int x;
		int y;
	};

	static Plugin& getInstance() {
		static Plugin plugin;

		return plugin;
	}

	void process(const std::string& name, float scale, int threadsCount);

	int getProgress();

private:
	Plugin() = default;

	void directionProcess(CANVAS_FLOAT& terrain, CANVAS_BYTE& directions, int width, int height, int index, std::ofstream& sources, int threadsCount);
	void accumulationProcess(CANVAS_UINT32& accumulation, CANVAS_BYTE& directions, int index, std::ifstream& sources, CHUNK_BORDERS& chunk, int threadsCount, size_t sourcesCount);

	std::optional<double> noData_;
	float scale_ = 1;

	std::function<int()> progressCallback_;
};