#include "pch.h"

#include "Plugin.h"

#include <filesystem>

#include "Timer.h"

void Plugin::process(const std::string& name, float scale, int threadsCount) {
	scale_ = scale;

	int availableThreads = std::thread::hardware_concurrency();
	//std::cout << "Available threads: " << availableThreads << std::endl;

	threadsCount = min(availableThreads, threadsCount);
	//std::cout << "Will be used threads: " << threadsCount << std::endl;

	std::cout << "Threads: " << threadsCount << "/" << availableThreads << " (" << (threadsCount * 100.f / availableThreads) << "%)" << std::endl;

	std::cout << "Opening: " << name << std::endl;

	std::cout << "Scale: " << scale << std::endl;

	GEOTIFF_READER terrainReader(new GdalTiffReader(name));

	std::cout << "Raster count: " << terrainReader->getRasterCount() << std::endl;

	RASTER_BAND terrainBand(terrainReader->getRasterBand(1));

	int width = terrainBand->getXSize(), height = terrainBand->getYSize();
	std::cout << "Raster size: " << width << "x" << height << std::endl;

	noData_ = terrainBand->getNoDataValue();
	std::cout << "Has nodata: " << (noData_ ? "yes (" + std::to_string(noData_.value()) + ")" : "no") << std::endl;

	std::filesystem::path temp_file = /*std::filesystem::temp_directory_path() /*/ "D:\\Tiles\\Test\\temp_tif_file.tif";

	GEOTIFF_READER directionsReader(new GdalTiffReader(temp_file.string(), width, height, 1));
	directionsReader->setProjection(terrainReader->getProjection());
	directionsReader->setGeoTransform(terrainReader->getGeoTransform());

	RASTER_BAND directionsBand(directionsReader->getRasterBand(1));
	CANVAS_FLOAT terrain(new Canvas<float>(terrainBand));
	CANVAS_BYTE directions(new Canvas<uint8_t>(directionsBand, true));

	Timer timer;

	std::vector<std::thread> threads;
	for (int i = 0; i < threadsCount; i++) {
		threads.push_back(std::move(std::thread([this, terrain, directions, width, height, i, threadsCount]() {
			try {
				directionProcess(terrain, directions, width, height, i, threadsCount);
			}
			catch (const std::exception& exception) {
				std::cout << exception.what() << std::endl;
			}
			})));
	}

	for (auto& thread : threads) {
		if (thread.joinable()) {
			thread.join();
		}
	}

	std::cout << "---------------- Finished ----------------" << std::endl;
	std::cout << "Spent time: " << timer.elapsedSeconds() << "s" << std::endl;
}

void Plugin::directionProcess(CANVAS_FLOAT terrain, CANVAS_BYTE directions, int width, int height, int index, int threadsCount) {
	static std::mutex mutex;
	int heightPerThread = height / threadsCount;
	int residualHeight = height - (heightPerThread * threadsCount);
	int beginHeight = heightPerThread * index;

	height = heightPerThread;
	if (index == threadsCount - 1) {
		height += residualHeight;
	}

	{
		std::unique_lock lock(mutex);
		std::cout << "Thread ID: " << index << " Height: " << height << " Begin Height: " << beginHeight << std::endl;
	}

	for (int j = beginHeight; j < beginHeight + height; j++) {
		for (int i = 0; i < width; i++) {
			float from = terrain->at(i, j, index);
			auto dir = directions->at(i, j, index);

			if (noData_ && noData_ == from) {
				dir = 1;
			}
			else {
				dir = 3;
			}
		}
	}

	/*if (noData_ && noData_ == from) {
		return;
	}*/

	/*float precision = 0.001f;
	float border = 0.f;
	for (int j = beginHeight; j < beginHeight + height; j++) {
		for (int i = 0; i < width; i++) {
			float from = terrain->at(i, j, index);

			if (noData_ && noData_ == from) {
				continue;
			}

			auto dir = directions->at(i, j, index);
			float minSlope = 0;

			for (int dirY = 0; dirY < 3; dirY++) {
				for (int dirX = 0; dirX < 3; dirX++) {
					if (dirX == 1 && dirY == 1) {
						continue;
					}

					int curX = i + dirX - 1;
					int curY = j + dirY - 1;

					auto _to = terrain->at(curX, curY, -index);
					float to = 0;
					if (!_to.valid() || (noData_ && _to.data() == noData_)) {
						border = from - precision;
						to = border;
					}
					else {
						to = _to;
					}

					float slope = scale_;

					if (!(dirX % 2) && !(dirY % 2)) {
						slope *= 1.41f;
					}

					slope = (to - from) / slope;
					if (slope < minSlope) {
						minSlope = slope;
						dir = dirY * 3 + dirX + 1;
					}
				}
			}

			if (!minSlope) {
				throw std::runtime_error("Unexpected direction, min slope equals zero.");
			}
		}
	}*/
}

EXPORT_API void Process(const char* name, float scale, int threadsCount) {
	Plugin::getInstance().process(name, scale, threadsCount);
}