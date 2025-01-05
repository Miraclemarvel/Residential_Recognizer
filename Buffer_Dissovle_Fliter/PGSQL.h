#pragma once

#include <pqxx/pqxx>
#include <libpq-fe.h>
#include <ogrsf_frmts.h>
#include "ogr_geometry.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "cpl_conv.h" 
#include "gdal_priv.h"

class PGSQL{
public:
	PGSQL(const std::string& connString);			 // ���캯��
	std::string getConnectionString() const;		 // ��ȡ���ݿ�����
	void saveToDatabase(GDALDataset* poDS);			 // �������ݵ����ݿ�
	void fliter(const std::string& outputFilePath);  // ���˽����������շ����ļ�
private:
	std::string msConnString;						 // ���ݿ������ַ���
	void dropTableIfExists(const std::string& tableName);// ɾ����(�Ѵ���)
	void createSpatialIndex();						 // �����ռ�����
	void dissolve(const std::string& outputFilePath);// �ϲ��ཻҪ��
};

