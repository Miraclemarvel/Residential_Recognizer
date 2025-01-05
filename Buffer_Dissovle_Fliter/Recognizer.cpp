#include "Recognizer.h"
#include <iostream>

Recognizer::Recognizer(const std::string& connStr){
	mpSQL = new PGSQL(connStr);
}

Recognizer::~Recognizer() {
	delete mpSQL;
}

// ʶ��
void Recognizer::recognize(const std::string& inFile, const std::string& outFile) {
	OGRRegisterAll();
	try {
		// ���ɻ�����
		GDALDataset* poDS = buffer(inFile);
		// �����ļ������ݿ�
		mpSQL->saveToDatabase(poDS);

	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}

	try {
		// �ϲ��ཻҪ��
		mpSQL->fliter(outFile);
	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}
	std::cout << "ʶ�����ѱ�����" << outFile << std::endl;
}

// ���ɻ�����
GDALDataset* Recognizer::buffer(const std::string& inFile) {
	const char* output = "./result/buffer.shp";
	// ע����������
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
	// ����������
	std::vector<OGRGeometry*> buffers;
	OGRFeature* poFeature;
	while ((poFeature = layer->GetNextFeature()) != NULL) {
		OGRGeometry* poGeom = poFeature->GetGeometryRef();
		if (poGeom != NULL) {
			//����buffer
			OGRGeometry* poBuffer = poGeom->Buffer(0.00012);

			if (poBuffer != NULL) {
				buffers.push_back(poBuffer);
			}
		}
		OGRFeature::DestroyFeature(poFeature);
	}

	// ���bufferͼ��
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