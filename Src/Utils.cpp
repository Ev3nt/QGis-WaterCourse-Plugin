#include "pch.h"

#include <thread>

EXPORT_API int GetMaxThreadsCount() {
    return std::thread::hardware_concurrency();
}

EXPORT_API void Open(LPCSTR fileName) {
    //GDALAllRegister();
    //
    //// �������� �����
    //GDALDataset* poDataset = (GDALDataset*)GDALOpen(fileName, GA_ReadOnly);
    //if (poDataset == nullptr) {
    //    std::cerr << "�� ������� ������� ����." << std::endl;

    //    return;
    //}

    //// ��������� ������� ������ (band)
    //GDALRasterBand* poBand = poDataset->GetRasterBand(1);
    //if (poBand == nullptr) {
    //    std::cerr << "�� ������� �������� �����." << std::endl;
    //    GDALClose(poDataset);

    //    return;
    //}

    //// ��������� �������� nodata
    //int bHasNoData;
    //double noDataValue = poBand->GetNoDataValue(&bHasNoData);

    //char buffer[MAX_PATH]{};

    //if (bHasNoData) {
    //    sprintf_s(buffer, "�������� nodata: %f", noDataValue);
    //}
    //else {
    //    sprintf_s(buffer, "�������� nodata �� ������.");
    //}

    //MessageBox(NULL, buffer, "Message", MB_OK);

    //// �������� �����
    //GDALClose(poDataset);
}