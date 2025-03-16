#include "pch.h"

#include "Plugin.h"

#include <queue>
#include <fstream>

#include "Barrier.h"
#include "Timer.h"

int Plugin::getDirection(int i, int j) {
	return (j + 1) * 3 + (i + 1) + 1;
}

void Plugin::getOffsets(int direction, int* i, int* j) {
	direction = abs(direction) - 1;

	*i = direction % 3 - 1;
	*j = direction / 3 - 1;
}

int Plugin::calculateFlowDirection(CANVAS_FLOAT& terrain, int x, int y, int index) {
	auto from = terrain->at(x, y, index);
	double maxSlope = 0;
	int direction = 0;

	for (int j = -1; j <= 1; j++) {
		for (int i = -1; i <= 1; i++) {
			if (i == 0 && j == 0) {
				continue;
			}

			int nx = x + i;
			int ny = y + j;

			auto toHolder = terrain->at(nx, ny, -index);
			double to = toHolder.valid() ? toHolder : from - 0.0001; // Precision = 0.9999

			double deltaZ = from - to;
			if (deltaZ <= 0) {
				continue;
			}

			double distance = (abs(i) + abs(j) == 2) ? 1.41 : 1.;
			double slope = deltaZ / distance;

			if (slope > maxSlope) {
				maxSlope = slope;
				direction = getDirection(i, j); // No zero!
			}
		}
	}

	return direction;
}

int Plugin::calculateEnters(CANVAS_BYTE& directions, int x, int y, int index) {
	int enters = 0;

	for (int j = -1; j <= 1; j++) {
		for (int i = -1; i <= 1; i++) {
			if (i == 0 && j == 0) {
				continue;
			}

			int nx = x + i;
			int ny = y + j;

			auto neighbour = directions->at(nx, ny, -index);
			if (!neighbour.valid()) {
				continue;
			}

			if (getDirection(i, j) == 10 - abs(neighbour)) {
				enters++;
			}
		}
	}

	return enters;
}

void Plugin::process(const std::string& name, const std::string& output, int threadsCount) {
	TempManager temp;

	int availableThreads = std::thread::hardware_concurrency();

	threadsCount = min(availableThreads, threadsCount);

	std::cout << "Threads: " << threadsCount << "/" << availableThreads << " (" << (threadsCount * 100.f / availableThreads) << "%)" << std::endl;

	std::cout << "Opening: " << name << std::endl;

	GEOTIFF_READER terrainReader(new GdalTiffReader(name));

	std::cout << "Raster count: " << terrainReader->getRasterCount() << std::endl;

	RASTER_BAND terrainBand(terrainReader->getRasterBand(1));

	int width = terrainBand->getXSize(), height = terrainBand->getYSize();
	std::cout << "Raster size: " << width << "x" << height << std::endl;

	terrainNoData_ = terrainBand->getNoDataValue();
	std::cout << "Has nodata: " << (terrainNoData_ ? "yes (" + std::to_string(terrainNoData_.value()) + ")" : "no") << std::endl;

	std::string projection = terrainReader->getProjection();
	std::cout << "Projection: " << projection << std::endl;

	CANVAS_FLOAT terrain(new Canvas<float>(terrainBand, true));

	bool interrupted = false;

	size_t sourcesFileSize = 0;

	Timer timer;

	{
		std::fstream sourcesFile(temp.addFile("sources"), std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
		GEOTIFF_READER directionsReader(new GdalTiffReader(temp.addFile("directions").string(), width, height, 1));
		directionsReader->setProjection(projection);
		directionsReader->setGeoTransform(terrainReader->getGeoTransform());

		RASTER_BAND directionsBand(directionsReader->getRasterBand(1));
		CANVAS_BYTE directions(new Canvas<int8_t>(directionsBand, true, true));
		directionsBand->setNoDataValue(directionNoData_.value());

		std::vector<std::thread> threads;
		threads.reserve(threadsCount);

		std::cout << "---------------- FlowDirections Started! ----------------" << std::endl;

		std::map<int, std::fstream> sourcesFiles;

		Timer flowTimer;

		for (int i = 0; i < threadsCount; i++) {
			threads.emplace_back([this, &temp, &terrain, &directions, width, height, i, &sourcesFiles, threadsCount, &interrupted]() {
				try {
					std::string fileName = temp.addFile("source_" + std::to_string(i)).string();
					std::fstream& sources = sourcesFiles[i] = std::fstream(fileName, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);

					directionProcess(terrain, directions, width, height, i, sources, threadsCount);
				}
				catch (const std::runtime_error& exception) {
					progressCallback_ = [] { return 0; };

					std::cout << "<b>---------------- FlowDirections Failed! ----------------</b>" << std::endl;
					std::cout << "<b>Exception:</b> " << exception.what() << std::endl;

					interrupted = true;
				}
				catch (...) {

				}
				});
		}

		for (auto& thread : threads) {
			if (thread.joinable()) {
				thread.join();
			}
		}

		if (interrupted) {
			return;
		}

		for (auto& [key, source] : sourcesFiles) {
			source.seekg(0, std::ios::end);
			size_t sourceFileSize = source.tellg();
			source.seekg(0, std::ios::beg);
			std::vector<char> buffer(sourceFileSize);
			source.read(buffer.data(), sourceFileSize);

			sourcesFile.write(buffer.data(), sourceFileSize);
			sourcesFileSize += sourceFileSize;

			source.close();

			temp.deleteFile("source_" + std::to_string(key));
		}

		std::cout << "---------------- FlowDirections Finished! ----------------" << std::endl;
		std::cout << "Spent time: " << flowTimer.elapsedSeconds() << "s" << std::endl;
	}

	std::cout << std::endl;

	{
		std::filesystem::path result_file = output;
		GEOTIFF_READER accumulationReader(new GdalTiffReader(result_file.string(), width, height, 1, true));
		accumulationReader->setProjection(projection);
		accumulationReader->setGeoTransform(terrainReader->getGeoTransform());

		RASTER_BAND accumaltionBand(accumulationReader->getRasterBand(1));

		GEOTIFF_READER directionsReader(new GdalTiffReader(temp.getPath("directions").string()));
		RASTER_BAND directionsBand(directionsReader->getRasterBand(1));

		CANVAS_UINT32 accumaltion(new Canvas<uint32_t>(accumaltionBand, false, true));
		CANVAS_BYTE directions(new Canvas<int8_t>(directionsBand, true));

		std::queue<CHUNK_BORDERS> chunks;
		Spinlock chunkMutex;

		size_t totalSourceCount = sourcesFileSize / sizeof(Source);

		size_t numOfSourcesToRead = 100000;
		size_t sourcesSizeToRead = numOfSourcesToRead * sizeof(Source);
		for (size_t begin = 0; begin < sourcesFileSize; begin += sourcesSizeToRead) {
			size_t chunkSize = min(sourcesSizeToRead, sourcesFileSize - begin);
			CHUNK_BORDERS chunk = { begin, chunkSize };

			chunks.push(chunk);
		}

		std::cout << "Total source count: " << totalSourceCount << std::endl;
		std::cout << "Chunk count: " << chunks.size() << std::endl;

		std::vector<std::thread> threads;
		threads.reserve(threadsCount);

		std::cout << "---------------- FlowAccumulation Started! ----------------" << std::endl;

		Timer flowTimer;

		for (int i = 0; i < threadsCount; i++) {
			threads.emplace_back([this, &accumaltion, &directions, i, &chunks, &chunkMutex, threadsCount, &interrupted, &temp, totalSourceCount]() {
				try {
					std::fstream sourcesFile(temp.getPath("sources"), std::ios::in | std::ios::out | std::ios::binary);

					while (true) {
						CHUNK_BORDERS chunk;

						{
							std::unique_lock lock(chunkMutex);

							if (chunks.empty()) {
								break;
							}

							chunk = chunks.front();
							chunks.pop();
						}

						accumulationProcess(accumaltion, directions, i, sourcesFile, chunk, threadsCount, totalSourceCount);
					}
				}
				catch (const std::runtime_error& exception) {
					progressCallback_ = [] { return 0; };

					std::cout << "<b>---------------- FlowAccumulation Failed! ----------------</b>" << std::endl;
					std::cout << "<b>Exception:</b> " << exception.what() << std::endl;

					interrupted = true;
				}
				catch (...) {

				}
				});
		}

		for (auto& thread : threads) {
			if (thread.joinable()) {
				thread.join();
			}
		}

		if (interrupted) {
			return;
		}

		std::cout << "---------------- FlowAccumulation Finished! ----------------" << std::endl;
		std::cout << "Spent time: " << flowTimer.elapsedSeconds() << "s" << std::endl;
	}

	std::cout << "---------------- Finished ----------------" << std::endl;
	std::cout << "Spent time: " << timer.elapsedSeconds() << "s" << std::endl;
	std::cout << std::endl;
}

void Plugin::directionProcess(CANVAS_FLOAT& terrain, CANVAS_BYTE& directions, int width, int height, int index, std::fstream& sourcesFile, int threadsCount) {
	static Barrier syncPoint;
	static std::atomic_bool interrupted;
	static std::atomic_int counter;

	int heightPerThread = height / threadsCount;
	int residualHeight = height - (heightPerThread * threadsCount);
	int rowOffset = heightPerThread * index;
	Source source;

	interrupted = false;
	counter = 0;

	int tileHeight = terrain->getHeight();
	progressCallback_ = [tileHeight]() -> int {
			return int(counter.load() / float(tileHeight * 2) * 100);
		};

	height = heightPerThread;
	if (index == threadsCount - 1) {
		height += residualHeight;
	}

	std::cout << "Thread Created ID: " << index << " (0x" << std::setfill('0') << std::setw(8) << std::right << std::this_thread::get_id() << ")\t Row Offset: " << rowOffset << "\t Height: " << height << std::endl;

	for (int y = rowOffset; y < rowOffset + height; y++) {
		for (int x = 0; x < width; x++) {
			if (interrupted.load(std::memory_order_relaxed)) {
				throw std::exception();
			}
			
			int direction = calculateFlowDirection(terrain, x, y, index);

			if (!direction) {
				interrupted = true;

				throw std::runtime_error("FlowDirection: Invalid direction!");
			}

			directions->at(x, y, index) = direction;
		}
	
		counter.fetch_add(1, std::memory_order_relaxed);
	}

	syncPoint.wait(threadsCount, [] { return interrupted.load(std::memory_order_relaxed); });

	if (interrupted.load(std::memory_order_relaxed)) {
		throw std::exception();
	}

	std::cout << "Thread ID: " << index << " Looking for sources..." << std::endl;

	for (int y = rowOffset; y < rowOffset + height; y++) {
		for (int x = 0; x < width; x++) {
			int enters = calculateEnters(directions, x, y, index);

			auto direction = directions->at(x, y, index);

			if (!enters) {
				source = { x, y };

				sourcesFile.write((char*)&source, sizeof(Source));
			}
			else if (enters > 1) {
				direction = -direction;
			}
		}

		counter.fetch_add(1, std::memory_order_relaxed);
	}
}

void Plugin::accumulationProcess(CANVAS_UINT32& accumulation, CANVAS_BYTE& directions, int index, std::fstream& sourcesFile, CHUNK_BORDERS& chunk, int threadsCount, size_t totalSourceCount) {
	static Spinlock readMutex, writeMutex;
	static std::atomic_int64_t counter;

	size_t sourceCount = chunk.second / sizeof(Source);

	std::vector<Source> sources(sourceCount);

	sourcesFile.seekg(chunk.first);
	sourcesFile.read((char*)sources.data(), chunk.second);
	
	if (!chunk.first) {
		counter = 0;
	}

	progressCallback_ = [totalSourceCount]() -> int {
			return int(counter.load() / float(totalSourceCount) * 100);
		};

	std::cout << "Thread ID: " << index << " (0x" << std::setfill('0') << std::setw(8) << std::right << std::this_thread::get_id() << ")\t Start Point: " << chunk.first << "\t End Point: " << chunk.first + chunk.second << "\t Sources Count: " << sourceCount << std::endl;

	for (const auto& source : sources) {
		int x = source.x;
		int y = source.y;

		int32_t value = 1;

		while (true) {
			auto direction = directions->at(x, y, index);
			if (!direction.valid()) {
				break;
			}

			auto data = accumulation->at(x, y, index);
			if (data) {
				break;
			}

			std::unique_lock<Spinlock> lock;
			if (direction < 0) {
				lock = std::unique_lock(writeMutex);
				uint32_t tempValue = 0;
				bool isOwner = true;

				for (int j = -1; j <= 1; j++) {
					for (int i = -1; i <= 1; i++) {
						if (!isOwner) {
							break;
						}

						if (i == 0 && j == 0) {
							continue;
						}

						int nx = x + i;
						int ny = y + j;

						auto neighbourDirection = directions->at(nx, ny, -index);
						if (!neighbourDirection.valid()) {
							continue;
						}

						if (getDirection(i, j) != 10 - abs(neighbourDirection)) {
							continue;
						}

						auto neighbour = accumulation->at(nx, ny, -index);

						if (neighbour == 0) {
							isOwner = false;

							break;
						}
						else {
							tempValue += neighbour;
						}
					}
				}

				if (!isOwner) {
					break;
				}

				value = ++tempValue;
			}

			data = value++;

			int i, j;
			getOffsets(direction, &i, &j);

			x += i;
			y += j;
		}

		counter.fetch_add(1, std::memory_order_relaxed);
	}
}

int Plugin::getProgress() {
	return progressCallback_ ? progressCallback_() : 0;
}

EXPORT_API void Process(const char* name, const char* output, int threadsCount) {
	try {
		Plugin::getInstance().process(name, output, threadsCount);
	}
	catch (const std::runtime_error& exception) {
		std::cout << "<b>---------------- FlowDirections Failed! ----------------</b>" << std::endl;
		std::cout << "<b>Exception:</b> " << exception.what() << std::endl;
	}
}

EXPORT_API int GetProgress() {
	return Plugin::getInstance().getProgress();
}