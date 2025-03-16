#pragma once

#include <string>

#include "TempManager.h"
#include <functional>
#include "Canvas.h"

typedef std::shared_ptr<Canvas<float>> CANVAS_FLOAT;
typedef std::shared_ptr<Canvas<int8_t>> CANVAS_BYTE;
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

	int getDirection(int i, int j);
	void getOffsets(int direction, int* i, int* j);
	int calculateFlowDirection(CANVAS_FLOAT& terrain, int x, int y, int index);
	int calculateEnters(CANVAS_BYTE& directions, int x, int y, int index);

	void process(const std::string& name, const std::string& output, int threadsCount);

	int getProgress();

private:
	Plugin() = default;

	void directionProcess(CANVAS_FLOAT& terrain, CANVAS_BYTE& directions, int width, int height, int index, std::fstream& sourcesFile, int threadsCount);
	void accumulationProcess(CANVAS_UINT32& accumulation, CANVAS_BYTE& directions, int index, std::fstream& sourcesFile, CHUNK_BORDERS& chunk, int threadsCount, size_t totalSourceCount);

	std::optional<double> terrainNoData_;
	std::optional<int> directionNoData_ = 64;

	std::function<int()> progressCallback_;
};