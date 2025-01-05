#pragma once
#include <string>
#include "PGSQL.h"

class Recognizer{
public:
	Recognizer(const std::string& connStr);
	~Recognizer();
	// �����ʶ��
	void recognize(const std::string& inFile, const std::string& outFile);
private:
	PGSQL* mpSQL;
	// ���ɻ�����
	GDALDataset* buffer(const std::string& inFile);
};

