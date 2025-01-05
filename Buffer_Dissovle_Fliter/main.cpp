#include "Recognizer.h"

int main() {
	std::string connStr = "PG:dbname=db_test user=postgres password=190475 hostaddr=172.27.61.34 port=5432";
	std::string inFile = "./data/GF7_anyang_1_build.shp";
	std::string outFile = "./result/residential.shp";
	
	Recognizer recognizer(connStr);
	recognizer.recognize(inFile, outFile);

	return 0;
}