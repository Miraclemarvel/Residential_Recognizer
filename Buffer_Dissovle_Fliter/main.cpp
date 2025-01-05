#include "Recognizer.h"

int main() {
	std::string connStr = "PG:dbname=your_db_name user=your_user_name password=your_password hostaddr=your_address port=5432";
	std::string inFile = "./data/your_input.shp";
	std::string outFile = "./result/residential.shp";
	
	Recognizer recognizer(connStr);
	recognizer.recognize(inFile, outFile);

	return 0;
}
