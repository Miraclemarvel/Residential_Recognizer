#pragma once
#include <string>
#include "PGSQL.h"

class Recognizer{
public:
	Recognizer(const std::string& connStr);
	~Recognizer();
	// 居民地识别
	void recognize(const std::string& inFile, const std::string& outFile);
private:
	PGSQL* mpSQL;
	// 生成缓冲区
	GDALDataset* buffer(const std::string& inFile);
};

