#include "pch.h"

#include "GdalTiffReader.h"

#include <stdexcept>

#include "gdal.h"
#include "gdal_priv.h"

GdalRasterBand::GdalRasterBand(void* rasterBand, bool ints64) : rasterBand_(rasterBand), ints64_(ints64) {
	if (!rasterBand) {
		throw std::runtime_error("Empty raster band.");
	}
}

GdalRasterBand::~GdalRasterBand() {
	//delete (GDALRasterBand*)rasterBand_;
}

int GdalRasterBand::getXSize() {
	GDALRasterBand* rasterBand = (GDALRasterBand*)rasterBand_;

	return rasterBand->GetXSize();
}

int GdalRasterBand::getYSize() {
	GDALRasterBand* rasterBand = (GDALRasterBand*)rasterBand_;

	return rasterBand->GetYSize();
}

int GdalRasterBand::getBand() {
	GDALRasterBand* rasterBand = (GDALRasterBand*)rasterBand_;

	return rasterBand->GetBand();
}

int GdalRasterBand::rasterByte(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize) {
	GDALRasterBand* rasterBand = (GDALRasterBand*)rasterBand_;
	std::unique_lock<std::mutex> lock;
	if (!rasterBand->GetDataset()->IsThreadSafe(GDAL_OF_RASTER)) {
		lock = std::unique_lock(mutex_);
	}

	return rasterBand->RasterIO(GF_Read, offsetX, offsetY, xSize, ySize, buffer, xBufferSize, yBufferSize, GDT_Byte, 0, 0);
}

int GdalRasterBand::rasterInt(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize) {
	GDALRasterBand* rasterBand = (GDALRasterBand*)rasterBand_;
	std::unique_lock<std::mutex> lock;
	if (!rasterBand->GetDataset()->IsThreadSafe(GDAL_OF_RASTER)) {
		lock = std::unique_lock(mutex_);
	}

	return rasterBand->RasterIO(GF_Read, offsetX, offsetY, xSize, ySize, buffer, xBufferSize, yBufferSize, GDT_Int32, 0, 0);
}

int GdalRasterBand::rasterUInt32(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize) {
	GDALRasterBand* rasterBand = (GDALRasterBand*)rasterBand_;
	std::unique_lock<std::mutex> lock;
	if (!rasterBand->GetDataset()->IsThreadSafe(GDAL_OF_RASTER)) {
		lock = std::unique_lock(mutex_);
	}

	return rasterBand->RasterIO(GF_Read, offsetX, offsetY, xSize, ySize, buffer, xBufferSize, yBufferSize, GDT_UInt32, 0, 0);
}

int GdalRasterBand::rasterUInt64(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize) {
	GDALRasterBand* rasterBand = (GDALRasterBand*)rasterBand_;
	std::unique_lock<std::mutex> lock;
	if (!rasterBand->GetDataset()->IsThreadSafe(GDAL_OF_RASTER)) {
		lock = std::unique_lock(mutex_);
	}

	return rasterBand->RasterIO(GF_Read, offsetX, offsetY, xSize, ySize, buffer, xBufferSize, yBufferSize, GDT_UInt64, 0, 0);
}

int GdalRasterBand::rasterFloat(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize) {
	GDALRasterBand* rasterBand = (GDALRasterBand*)rasterBand_;
	std::unique_lock<std::mutex> lock;
	if (!rasterBand->GetDataset()->IsThreadSafe(GDAL_OF_RASTER)) {
		lock = std::unique_lock(mutex_);
	}

	return rasterBand->RasterIO(GF_Read, offsetX, offsetY, xSize, ySize, buffer, xBufferSize, yBufferSize, GDT_Float32, 0, 0);
}

int GdalRasterBand::rasterDouble(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize) {
	GDALRasterBand* rasterBand = (GDALRasterBand*)rasterBand_;
	std::unique_lock<std::mutex> lock;
	if (!rasterBand->GetDataset()->IsThreadSafe(GDAL_OF_RASTER)) {
		lock = std::unique_lock(mutex_);
	}

	return rasterBand->RasterIO(GF_Read, offsetX, offsetY, xSize, ySize, buffer, xBufferSize, yBufferSize, GDT_Float64, 0, 0);
}

int GdalRasterBand::raster(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize) {
	GDALRasterBand* rasterBand = (GDALRasterBand*)rasterBand_;
	std::unique_lock lock(mutex_);

	return rasterBand->RasterIO(GF_Write, offsetX, offsetY, xSize, ySize, buffer, xBufferSize, yBufferSize, ints64_ ? GDT_UInt32 : GDT_Byte, 0, 0);
}

std::optional<double> GdalRasterBand::getNoDataValue() {
	GDALRasterBand* rasterBand = (GDALRasterBand*)rasterBand_;

	int hasNoData;
	double noDataValue = rasterBand->GetNoDataValue(&hasNoData);

	return hasNoData ? std::optional<double>(noDataValue) : std::nullopt;
}

int GdalRasterBand::setNoDataValue(double value) {
	GDALRasterBand* rasterBand = (GDALRasterBand*)rasterBand_;

	return rasterBand->SetNoDataValue(value);
}

GdalTiffReader::GdalTiffReader(const std::string& fileName) {
	GDALAllRegister();
	gdalDataset_ = GDALDataset::Open(fileName.data(), GDAL_OF_READONLY | GDAL_OF_RASTER | GDAL_OF_THREAD_SAFE);
	GDALDataset* poDataset = (GDALDataset*)gdalDataset_;
}

GdalTiffReader::GdalTiffReader(const std::string& fileName, int sizeX, int sizeY, int bandCount, bool ints64) : ints64_(ints64) {
	/*options_ = CSLSetNameValue(options_, "TILED", "YES");
	options_ = CSLSetNameValue(options_, "COMPRESS", "PACKBITS");*/
	options_ = CSLSetNameValue(options_, "BIGTIFF", "YES");
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
	gdalDataset_ = poDriver->Create(fileName.data(), sizeX, sizeY, bandCount, ints64 ? GDT_UInt32 : GDT_Byte, options_);
}

GdalTiffReader::~GdalTiffReader() {
	GDALClose((GDALDatasetH)gdalDataset_);

	if (options_) {
		CSLDestroy(options_);
	}
}

GdalRasterBand* GdalTiffReader::getRasterBand(int num) {
	return new GdalRasterBand(GDALDataset::FromHandle(gdalDataset_)->GetRasterBand(num), ints64_);
}

int GdalTiffReader::getRasterCount() {
	return GDALDataset::FromHandle(gdalDataset_)->GetRasterCount();
}

std::string GdalTiffReader::getProjection() {
	return GDALDataset::FromHandle(gdalDataset_)->GetProjectionRef();
}

void GdalTiffReader::setProjection(const std::string& projection) {
	GDALDataset::FromHandle(gdalDataset_)->SetProjection(projection.data());
}

std::vector<double> GdalTiffReader::getGeoTransform() {
	std::vector<double> geoTransform(6);

	GDALDataset::FromHandle(gdalDataset_)->GetGeoTransform(geoTransform.data());

	return geoTransform;
}

void GdalTiffReader::setGeoTransform(const std::vector<double>& geoTransform) {
	GDALDataset::FromHandle(gdalDataset_)->SetGeoTransform((double*)geoTransform.data());
}