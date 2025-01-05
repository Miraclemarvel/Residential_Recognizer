#include "Recognizer.h"
#include <iostream>

Recognizer::Recognizer(const std::string& connStr){
	mpSQL = new PGSQL(connStr);
}

Recognizer::~Recognizer() {
	delete mpSQL;
}

// 识别
void Recognizer::recognize(const std::string& inFile, const std::string& outFile) {
	OGRRegisterAll();
	try {
		// 生成缓冲区
		GDALDataset* poDS = buffer(inFile);
		// 保存文件到数据库
		mpSQL->saveToDatabase(poDS);

	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}

	try {
		// 合并相交要素
		mpSQL->fliter(outFile);
	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}
	std::cout << "识别结果已保存至" << outFile << std::endl;
}

// 生成缓冲区
GDALDataset* Recognizer::buffer(const std::string& inFile) {
	const char* output = "./result/buffer.shp";
	// 注册所有驱动
	OGRRegisterAll();
	GDALDataset* mpoDataset = (GDALDataset*)GDALOpenEx(inFile.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
	if (mpoDataset == NULL) {
		std::cout << "Open failed.\n";
		return nullptr;
	}
	OGRLayer* layer = mpoDataset->GetLayer(0);
	if (layer == NULL) {
		std::cout << "GetLayer failed.\n";
		return nullptr;
	}
	// 创建缓冲区
	std::vector<OGRGeometry*> buffers;
	OGRFeature* poFeature;
	while ((poFeature = layer->GetNextFeature()) != NULL) {
		OGRGeometry* poGeom = poFeature->GetGeometryRef();
		if (poGeom != NULL) {
			//计算buffer
			OGRGeometry* poBuffer = poGeom->Buffer(0.00012);

			if (poBuffer != NULL) {
				buffers.push_back(poBuffer);
			}
		}
		OGRFeature::DestroyFeature(poFeature);
	}

	// 输出buffer图层
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
	GDALDataset* poDS = poDriver->Create(output, 0, 0, 0, GDT_Unknown, NULL);

	OGRLayer* poLayer = poDS->CreateLayer("contours", NULL, wkbPolygon, NULL);
	for (auto& buffer : buffers) {
		OGRFeature* poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
		poFeature->SetGeometry(buffer);
		poLayer->CreateFeature(poFeature);
		OGRFeature::DestroyFeature(poFeature);
	}
	return poDS;
}