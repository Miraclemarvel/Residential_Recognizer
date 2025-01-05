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
	PGSQL(const std::string& connString);			 // 构造函数
	std::string getConnectionString() const;		 // 获取数据库连接
	void saveToDatabase(GDALDataset* poDS);			 // 保存数据到数据库
	void fliter(const std::string& outputFilePath);  // 过滤结果，输出最终分类文件
private:
	std::string msConnString;						 // 数据库连接字符串
	void dropTableIfExists(const std::string& tableName);// 删除表(已存在)
	void createSpatialIndex();						 // 创建空间索引
	void dissolve(const std::string& outputFilePath);// 合并相交要素
};

