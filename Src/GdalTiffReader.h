#pragma once

#include <string>
#include <optional>
#include <mutex>

#include "IGeoTiffReader.h"

class GdalRasterBand : public IRasterBand {
public:
	GdalRasterBand(void* rasterBand, bool ints64);
	~GdalRasterBand();

	int rasterByte(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize);
	int rasterInt(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize);
	int rasterUInt32(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize);
	int rasterUInt64(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize);
	int rasterFloat(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize);
	int rasterDouble(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize);

	int raster(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize);

	int getXSize();
	int getYSize();
	int getBand();

	std::optional<double> getNoDataValue();

private:
	std::mutex mutex_;
	void* rasterBand_ = nullptr;
	bool ints64_ = false;
};

class GdalTiffReader : public IGeoTiffReader {
public:
	GdalTiffReader(const std::string& fileName);
	GdalTiffReader(const std::string& fileName, int sizeX, int sizeY, int bandCount, bool ints64 = false);
	~GdalTiffReader();

	GdalRasterBand* getRasterBand(int num);
	int getRasterCount();

	std::string getProjection();
	void setProjection(const std::string& projection);

	std::vector<double> getGeoTransform();
	void setGeoTransform(const std::vector<double>& geoTransform);

private:
	char** options_ = nullptr;
	void* gdalDataset_ = nullptr;
	bool ints64_ = false;
};