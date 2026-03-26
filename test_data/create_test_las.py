import struct
import os

def create_test_las_file(filename, num_points=1000):
    """创建一个简单的测试LAS文件"""
    
    # LAS文件头结构
    header_size = 227
    offset_to_points = header_size
    
    # 计算数据范围
    x_min, x_max = 0.0, 100.0
    y_min, y_max = 0.0, 100.0
    z_min, z_max = 0.0, 50.0
    
    # 缩放因子和偏移量
    x_scale = 0.001
    y_scale = 0.001
    z_scale = 0.001
    x_offset = x_min
    y_offset = y_min
    z_offset = z_min
    
    with open(filename, 'wb') as f:
        # 写入文件签名
        f.write(b'LASF')
        
        # 文件源ID
        f.write(struct.pack('<H', 0))
        # 全局编码
        f.write(struct.pack('<H', 0))
        # GUID数据
        f.write(struct.pack('<I', 0))
        f.write(struct.pack('<H', 0))
        f.write(struct.pack('<H', 0))
        f.write(struct.pack('<8B', 0, 0, 0, 0, 0, 0, 0, 0))
        
        # 版本号
        f.write(struct.pack('<B', 1))  # 主版本
        f.write(struct.pack('<B', 2))  # 次版本
        
        # 系统标识符
        system_id = b'Test System' + b'\x00' * 20
        f.write(system_id)
        
        # 生成软件
        software = b'Test Generator' + b'\x00' * 18
        f.write(software)
        
        # 创建日期
        f.write(struct.pack('<H', 1))  # 一年中的第几天
        f.write(struct.pack('<H', 2024))  # 年份
        
        # 头部大小
        f.write(struct.pack('<H', header_size))
        
        # 点数据偏移量
        f.write(struct.pack('<I', offset_to_points))
        
        # 变长记录数量
        f.write(struct.pack('<I', 0))
        
        # 点数据格式ID (格式2 - 带RGB)
        f.write(struct.pack('<B', 2))
        
        # 点数据记录长度
        f.write(struct.pack('<H', 26))  # 26字节/点
        
        # 点数
        f.write(struct.pack('<I', num_points))
        
        # 各返回的点数
        f.write(struct.pack('<5I', num_points, 0, 0, 0, 0))
        
        # 缩放因子和偏移量
        f.write(struct.pack('<d', x_scale))
        f.write(struct.pack('<d', y_scale))
        f.write(struct.pack('<d', z_scale))
        f.write(struct.pack('<d', x_offset))
        f.write(struct.pack('<d', y_offset))
        f.write(struct.pack('<d', z_offset))
        
        # 最大值
        f.write(struct.pack('<d', x_max))
        f.write(struct.pack('<d', y_max))
        f.write(struct.pack('<d', z_max))
        
        # 最小值
        f.write(struct.pack('<d', x_min))
        f.write(struct.pack('<d', y_min))
        f.write(struct.pack('<d', z_min))
        
        # 写入点数据
        import random
        random.seed(42)  # 固定随机种子以便重现
        
        for i in range(num_points):
            # 生成随机点坐标
            x = random.uniform(x_min, x_max)
            y = random.uniform(y_min, y_max)
            z = random.uniform(z_min, z_max)
            
            # 转换为整数坐标
            x_int = int((x - x_offset) / x_scale)
            y_int = int((y - y_offset) / y_scale)
            z_int = int((z - z_offset) / z_scale)
            
            # 强度
            intensity = random.randint(0, 65535)
            
            # 返回信息
            return_byte = 0  # 第一个返回
            
            # 分类
            classification = random.randint(0, 5)
            
            # 扫描角度
            scan_angle = random.randint(-90, 90)
            
            # 用户数据
            user_data = 0
            
            # 点源ID
            point_source_id = 1
            
            # RGB颜色
            r = random.randint(0, 65535)
            g = random.randint(0, 65535)
            b = random.randint(0, 65535)
            
            # 写入点数据
            f.write(struct.pack('<i', x_int))
            f.write(struct.pack('<i', y_int))
            f.write(struct.pack('<i', z_int))
            f.write(struct.pack('<H', intensity))
            f.write(struct.pack('<B', return_byte))
            f.write(struct.pack('<B', classification))
            f.write(struct.pack('<b', scan_angle))
            f.write(struct.pack('<B', user_data))
            f.write(struct.pack('<H', point_source_id))
            f.write(struct.pack('<H', r))
            f.write(struct.pack('<H', g))
            f.write(struct.pack('<H', b))
    
    print(f"已创建测试LAS文件: {filename}")
    print(f"包含 {num_points} 个点")

if __name__ == "__main__":
    # 创建测试文件
    test_file = "test_pointcloud.las"
    create_test_las_file(test_file, 1000)