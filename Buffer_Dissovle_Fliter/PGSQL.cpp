#include "PGSQL.h"
#include <iostream>

// ���캯��
PGSQL::PGSQL(const std::string& connString) : msConnString(connString) {}

// �������ݵ����ݿ�
void PGSQL::saveToDatabase(GDALDataset* poDS) {
	std::string connectionString = getConnectionString();
	// �� GDAL ���ݼ�
	if (!poDS) {
		throw std::runtime_error("Failed to open input dataset");
	}

	GDALDriver* pgDriver = GetGDALDriverManager()->GetDriverByName("PostgreSQL");
	if (!pgDriver) {
		throw std::runtime_error("PostgreSQL driver not found");
	}

	// ɾ��������Ѵ��ڣ�
	dropTableIfExists("buffer");

	// д�����ݵ����ݿ�
	GDALDataset* outputDataset = pgDriver->CreateCopy(connectionString.c_str(), poDS, FALSE, nullptr, nullptr, nullptr);
	if (!outputDataset) {
		GDALClose(poDS);
		throw std::runtime_error("Failed to save dataset to database");
	}
	// �����ռ�����
	createSpatialIndex();

	GDALClose(outputDataset);
	GDALClose(poDS);
	//std::cout << "Data saved to database table: " << tableName << std::endl;
}

// ���˽����������շ����ļ�
void PGSQL::fliter(const std::string& outputFilePath) {
	std::string connectionString = getConnectionString();
	std::string tempTableName = "temp";
	std::string unionFilePath = "./result/union.shp";

	dissolve(unionFilePath);

	GDALAllRegister();
	// �� PostgreSQL ���ݼ�
	std::string connString = connectionString;
	GDALDataset* poDS = static_cast<GDALDataset*>(GDALOpenEx(connString.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
	if (!poDS) {
		throw std::runtime_error("Failed to open PostgreSQL dataset");
	}

	// �� Shapefile ����
	GDALDriver* shpDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
	if (!shpDriver) {
		GDALClose(poDS);
		throw std::runtime_error("ESRI Shapefile driver not found");
	}

	// ���� SQL ��ѯ
	std::string sql = "SELECT * FROM " + tempTableName;

	// ���õ���ѡ��
	char** papszOptions = nullptr;

	// ��������
	GDALDataset* midDataset = shpDriver->Create(unionFilePath.c_str(), 0, 0, 0, GDT_Unknown, papszOptions);
	if (!midDataset) {
		GDALClose(poDS);
		throw std::runtime_error("Failed to create output Shapefile");
	}

	// ����ͼ��
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
	// ����
	GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");

	GDALDataset* outputDataset = driver->Create(outputFilePath.c_str(), 0, 0, 0, GDT_Unknown, nullptr);

	OGRLayer* outputLayer = outputDataset->CreateLayer("split_output", nullptr, wkbPolygon, nullptr);
	if (outputLayer == nullptr) {
		std::cerr << "Failed to create output layer." << std::endl;
		GDALClose(outputDataset);
		GDALClose(midDataset);
	}
	// ��ȡ����ĺϲ���
	midLayer->ResetReading();

	OGRFeature* feature = nullptr;
	feature = midLayer->GetNextFeature();

	OGRGeometry* mergedGeometry = feature->GetGeometryRef();

	//double sumArea = 0.0f;
	//int count = 0;
	// ������
	if (mergedGeometry->getGeometryType() == wkbMultiPolygon) {
		OGRMultiPolygon* multiPolygon = mergedGeometry->toMultiPolygon();
		for (int i = 0; i < multiPolygon->getNumGeometries(); i++) {
			OGRGeometry* geom = multiPolygon->getGeometryRef(i);
			// �������С��minArea����
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
		// ����ǵ�һ�棬ֱ�ӱ���
		OGRFeature* newFeature = OGRFeature::CreateFeature(outputLayer->GetLayerDefn());
		newFeature->SetGeometry(mergedGeometry->ConvexHull());
		outputLayer->CreateFeature(newFeature);
		OGRFeature::DestroyFeature(newFeature);
	}
	//std::cout << "Average area of merged geometry: " << sumArea / count << std::endl;
	// ������Դ
	delete mergedGeometry;

	// �ͷ���Դ
	GDALClose(midDataset);
	GDALClose(outputDataset);
}

// ��ȡ���ݿ�����
std::string PGSQL::getConnectionString() const {
	return msConnString;
		//"PG:dbname=db_test user=postgres password=190475 hostaddr=172.27.61.34 port=5432";
}

// ɾ����(���Ǳ�Ľӿ������⣬ֻ����ɾ���ٴ�����
void PGSQL::dropTableIfExists(const std::string& tableName) {

	pqxx::connection conn("dbname=db_test user=postgres password=190475");
	pqxx::work txn(conn);

	std::string sql = "DROP TABLE IF EXISTS " + tableName + " CASCADE;";
	txn.exec(sql);
	txn.commit();
	//std::cout << "Dropped table if it existed: " << tableName << std::endl;
}

// �����ռ�����
void PGSQL::createSpatialIndex() {
	std::string tableName = "buffer";

	pqxx::connection conn("dbname=db_test user=postgres password=190475");
	pqxx::work txn(conn);

	std::string sql = "CREATE INDEX " + tableName + "_geom_idx ON " + tableName + " USING GIST (wkb_geometry);";
	txn.exec(sql);
	txn.commit();
	//std::cout << "Spatial index created for table: " << tableName << std::endl;
}

// �ϲ��ཻҪ��
void PGSQL::dissolve(const std::string& outputFilePath) {
	std::string connectionString = getConnectionString();
	std::string tableName = "buffer";
	std::string tempTableName = "temp";
	
	try {
		// ���ӵ� PostgreSQL ���ݿ�
		PGconn* conn = PQconnectdb(connectionString.substr(3, connectionString.length() - 3).c_str());
		if (PQstatus(conn) != CONNECTION_OK) {
			throw std::runtime_error("Failed to connect to the database: " + std::string(PQerrorMessage(conn)));
		}

		dropTableIfExists(tempTableName);

		// ������ʱ���ϲ��ཻҪ��
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

		// �� PostgreSQL ���ݼ�
		GDALAllRegister();
		std::string connString = connectionString;
		GDALDataset* poDS = static_cast<GDALDataset*>(GDALOpenEx(connString.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
		if (!poDS) {
			throw std::runtime_error("Failed to open PostgreSQL dataset");
		}

		// �� Shapefile ����
		GDALDriver* shpDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
		if (!shpDriver) {
			GDALClose(poDS);
			throw std::runtime_error("ESRI Shapefile driver not found");
		}

		// ���� SQL ��ѯ
		std::string sqlQuery = "SELECT * FROM " + tempTableName;

		// ������ Shapefile
		char** papszOptions = nullptr;
		GDALDataset* outputDataset = shpDriver->Create(outputFilePath.c_str(), 0, 0, 0, GDT_Unknown, papszOptions);
		if (!outputDataset) {
			GDALClose(poDS);
			throw std::runtime_error("Failed to create output Shapefile");
		}

		// ִ�в�ѯ������ͼ��
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

		// �ͷ���Դ
		poDS->ReleaseResultSet(inputLayer);
		GDALClose(outputDataset);
		GDALClose(poDS);
	}
	catch (const std::exception& e) {
		std::cout << "Error: " << e.what() << std::endl;
	}
}