#include "pch.h"

#include <thread>

EXPORT_API int GetMaxThreadsCount() {
    return std::thread::hardware_concurrency();
}

EXPORT_API void Open(LPCSTR fileName) {
    //GDALAllRegister();
    //
    //// Открытие файла
    //GDALDataset* poDataset = (GDALDataset*)GDALOpen(fileName, GA_ReadOnly);
    //if (poDataset == nullptr) {
    //    std::cerr << "Не удалось открыть файл." << std::endl;

    //    return;
    //}

    //// Получение первого канала (band)
    //GDALRasterBand* poBand = poDataset->GetRasterBand(1);
    //if (poBand == nullptr) {
    //    std::cerr << "Не удалось получить канал." << std::endl;
    //    GDALClose(poDataset);

    //    return;
    //}

    //// Получение значения nodata
    //int bHasNoData;
    //double noDataValue = poBand->GetNoDataValue(&bHasNoData);

    //char buffer[MAX_PATH]{};

    //if (bHasNoData) {
    //    sprintf_s(buffer, "Значение nodata: %f", noDataValue);
    //}
    //else {
    //    sprintf_s(buffer, "Значение nodata не задано.");
    //}

    //MessageBox(NULL, buffer, "Message", MB_OK);

    //// Закрытие файла
    //GDALClose(poDataset);
}