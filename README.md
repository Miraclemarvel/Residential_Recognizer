# Residential_Recognizer
基于建筑物矢量图层的居民区（建筑物密集区）识别

## 第三方库
- **GDAL** 3.9.0
- **libpqxx** 7.9.2

## 使用说明

1. **配置 PostgreSQL 连接**
   - 在 `main` 函数中提供 PostgreSQL 连接字符串，格式如下：
     ```cpp
     "PG:dbname=your_db_name user=your_user_name password=your_password hostaddr=your_host_address port=5432"
     ```

2. **输入文件路径与输出文件路径**
   - 输入文件路径：指定您的输入建筑物矢量图层的路径。
   - 输出文件路径：指定识别结果输出的文件路径。

3. **执行**
   - 在 `main` 函数中修改相应的路径，并根据需要运行程序。

## 示例
```cpp
std::string connStr = "PG:dbname=my_database user=my_user password=my_password hostaddr=127.0.0.1 port=5432";
std::string inputFilePath = "input_buildings.shp";
std::string outputFilePath = "output_residential_area.shp";

