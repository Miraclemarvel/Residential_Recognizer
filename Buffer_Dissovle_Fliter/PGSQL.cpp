#include "PGSQL.h"
#include <iostream>

// 构造函数
PGSQL::PGSQL(const std::string& connString) : msConnString(connString) {}

// 保存数据到数据库
void PGSQL::saveToDatabase(GDALDataset* poDS) {
	std::string connectionString = getConnectionString();
	// 打开 GDAL 数据集
	if (!poDS) {
		throw std::runtime_error("Failed to open input dataset");
	}

	GDALDriver* pgDriver = GetGDALDriverManager()->GetDriverByName("PostgreSQL");
	if (!pgDriver) {
		throw std::runtime_error("PostgreSQL driver not found");
	}

	// 删除表（如果已存在）
	dropTableIfExists("buffer");

	// 写入数据到数据库
	GDALDataset* outputDataset = pgDriver->CreateCopy(connectionString.c_str(), poDS, FALSE, nullptr, nullptr, nullptr);
	if (!outputDataset) {
		GDALClose(poDS);
		throw std::runtime_error("Failed to save dataset to database");
	}
	// 创建空间索引
	createSpatialIndex();

	GDALClose(outputDataset);
	GDALClose(poDS);
	//std::cout << "Data saved to database table: " << tableName << std::endl;
}

// 过滤结果，输出最终分类文件
void PGSQL::fliter(const std::string& outputFilePath) {
	std::string connectionString = getConnectionString();
	std::string tempTableName = "temp";
	std::string unionFilePath = "./result/union.shp";

	dissolve(unionFilePath);

	GDALAllRegister();
	// 打开 PostgreSQL 数据集
	std::string connString = connectionString;
	GDALDataset* poDS = static_cast<GDALDataset*>(GDALOpenEx(connString.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
	if (!poDS) {
		throw std::runtime_error("Failed to open PostgreSQL dataset");
	}

	// 打开 Shapefile 驱动
	GDALDriver* shpDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
	if (!shpDriver) {
		GDALClose(poDS);
		throw std::runtime_error("ESRI Shapefile driver not found");
	}

	// 构建 SQL 查询
	std::string sql = "SELECT * FROM " + tempTableName;

	// 设置导出选项
	char** papszOptions = nullptr;

	// 导出数据
	GDALDataset* midDataset = shpDriver->Create(unionFilePath.c_str(), 0, 0, 0, GDT_Unknown, papszOptions);
	if (!midDataset) {
		GDALClose(poDS);
		throw std::runtime_error("Failed to create output Shapefile");
	}

	// 创建图层
	OGRLayer* inputLayer = poDS->ExecuteSQL(sql.c_str(), nullptr, nullptr);
	if (!inputLayer) {
		GDALClose(midDataset);
		GDALClose(poDS);
		throw std::runtime_error("Failed to execute SQL query on PostgreSQL dataset");
	}

	OGRLayer* midLayer = midDataset->CopyLayer(inputLayer, tempTableName.c_str(), papszOptions);
	if (!midLayer) {
		GDALClose(midDataset);
		GDALClose(poDS);
		throw std::runtime_error("Failed to copy data to Shapefile");
	}
	GDALClose(poDS);
	// 过滤
	GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");

	GDALDataset* outputDataset = driver->Create(outputFilePath.c_str(), 0, 0, 0, GDT_Unknown, nullptr);

	OGRLayer* outputLayer = outputDataset->CreateLayer("split_output", nullptr, wkbPolygon, nullptr);
	if (outputLayer == nullptr) {
		std::cerr << "Failed to create output layer." << std::endl;
		GDALClose(outputDataset);
		GDALClose(midDataset);
	}
	// 读取输入的合并面
	midLayer->ResetReading();

	OGRFeature* feature = nullptr;
	feature = midLayer->GetNextFeature();

	OGRGeometry* mergedGeometry = feature->GetGeometryRef();

	//double sumArea = 0.0f;
	//int count = 0;
	// 处理拆分
	if (mergedGeometry->getGeometryType() == wkbMultiPolygon) {
		OGRMultiPolygon* multiPolygon = mergedGeometry->toMultiPolygon();
		for (int i = 0; i < multiPolygon->getNumGeometries(); i++) {
			OGRGeometry* geom = multiPolygon->getGeometryRef(i);
			// 过滤面积小于minArea的面
			auto poly = dynamic_cast<OGRPolygon*>(geom);

			//sumArea += poly->get_Area();
			//count++;
			if (poly->get_Area() <= 1e-05) {
				continue;
			}

			OGRFeature* newFeature = OGRFeature::CreateFeature(outputLayer->GetLayerDefn());
			newFeature->SetGeometry(geom->ConvexHull());
			outputLayer->CreateFeature(newFeature);
			OGRFeature::DestroyFeature(newFeature);
		}
	}
	else {
		// 如果是单一面，直接保存
		OGRFeature* newFeature = OGRFeature::CreateFeature(outputLayer->GetLayerDefn());
		newFeature->SetGeometry(mergedGeometry->ConvexHull());
		outputLayer->CreateFeature(newFeature);
		OGRFeature::DestroyFeature(newFeature);
	}
	//std::cout << "Average area of merged geometry: " << sumArea / count << std::endl;
	// 清理资源
	delete mergedGeometry;

	// 释放资源
	GDALClose(midDataset);
	GDALClose(outputDataset);
}

// 获取数据库连接
std::string PGSQL::getConnectionString() const {
	return msConnString;
}

// 删除表(覆盖表的接口有问题，只能先删除再创建）
void PGSQL::dropTableIfExists(const std::string& tableName) {

	pqxx::connection conn("dbname=db_test user=postgres password=190475");
	pqxx::work txn(conn);

	std::string sql = "DROP TABLE IF EXISTS " + tableName + " CASCADE;";
	txn.exec(sql);
	txn.commit();
	//std::cout << "Dropped table if it existed: " << tableName << std::endl;
}

// 创建空间索引
void PGSQL::createSpatialIndex() {
	std::string tableName = "buffer";

	pqxx::connection conn("dbname=db_test user=postgres password=190475");
	pqxx::work txn(conn);

	std::string sql = "CREATE INDEX " + tableName + "_geom_idx ON " + tableName + " USING GIST (wkb_geometry);";
	txn.exec(sql);
	txn.commit();
	//std::cout << "Spatial index created for table: " << tableName << std::endl;
}

// 合并相交要素
void PGSQL::dissolve(const std::string& outputFilePath) {
	std::string connectionString = getConnectionString();
	std::string tableName = "buffer";
	std::string tempTableName = "temp";
	
	try {
		// 连接到 PostgreSQL 数据库
		PGconn* conn = PQconnectdb(connectionString.substr(3, connectionString.length() - 3).c_str());
		if (PQstatus(conn) != CONNECTION_OK) {
			throw std::runtime_error("Failed to connect to the database: " + std::string(PQerrorMessage(conn)));
		}

		dropTableIfExists(tempTableName);

		// 创建临时表并合并相交要素
		std::string sqlMerge = R"(
			CREATE TABLE )" + tempTableName + R"( AS
			SELECT ST_Union(wkb_geometry) AS geom
			 FROM )" + tableName + R"()";

		PGresult* res = PQexec(conn, sqlMerge.c_str());
		if (PQresultStatus(res) != PGRES_COMMAND_OK) {
			std::string errorMsg = PQerrorMessage(conn);
			PQclear(res);
			PQfinish(conn);
			throw std::runtime_error("Failed to execute SQL: " + errorMsg);
		}
		PQclear(res);
		PQfinish(conn);

		// 打开 PostgreSQL 数据集
		GDALAllRegister();
		std::string connString = connectionString;
		GDALDataset* poDS = static_cast<GDALDataset*>(GDALOpenEx(connString.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
		if (!poDS) {
			throw std::runtime_error("Failed to open PostgreSQL dataset");
		}

		// 打开 Shapefile 驱动
		GDALDriver* shpDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
		if (!shpDriver) {
			GDALClose(poDS);
			throw std::runtime_error("ESRI Shapefile driver not found");
		}

		// 构建 SQL 查询
		std::string sqlQuery = "SELECT * FROM " + tempTableName;

		// 导出到 Shapefile
		char** papszOptions = nullptr;
		GDALDataset* outputDataset = shpDriver->Create(outputFilePath.c_str(), 0, 0, 0, GDT_Unknown, papszOptions);
		if (!outputDataset) {
			GDALClose(poDS);
			throw std::runtime_error("Failed to create output Shapefile");
		}

		// 执行查询并复制图层
		OGRLayer* inputLayer = poDS->ExecuteSQL(sqlQuery.c_str(), nullptr, nullptr);
		if (!inputLayer) {
			GDALClose(outputDataset);
			GDALClose(poDS);
			throw std::runtime_error("Failed to execute SQL query on PostgreSQL dataset");
		}

		OGRLayer* outputLayer = outputDataset->CopyLayer(inputLayer, tempTableName.c_str(), papszOptions);
		if (!outputLayer) {
			GDALClose(outputDataset);
			GDALClose(poDS);
			throw std::runtime_error("Failed to copy data to Shapefile");
		}

		// 释放资源
		poDS->ReleaseResultSet(inputLayer);
		GDALClose(outputDataset);
		GDALClose(poDS);
	}
	catch (const std::exception& e) {
		std::cout << "Error: " << e.what() << std::endl;
	}
}
