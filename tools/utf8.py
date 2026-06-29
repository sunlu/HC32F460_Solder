import os
import sys
import codecs
import argparse

# 定义需要处理的文件扩展名
TARGET_EXTENSIONS = {'.c', '.h', '.json', '.md', '.txt'}

def is_target_file(filename):
    """检查文件扩展名是否在目标列表中"""
    _, ext = os.path.splitext(filename)
    return ext.lower() in TARGET_EXTENSIONS

def convert_file_encoding(file_path, src_encodings=['gbk', 'gb2312'], dst_encoding='utf-8'):
    """
    尝试将文件从 src_encodings 转换为 dst_encoding
    如果文件已经是 dst_encoding，则跳过
    """
    try:
        # 1. 首先尝试以目标编码(UTF-8)读取
        # 如果成功，说明文件可能已经是 UTF-8，无需转换
        try:
            with open(file_path, 'r', encoding=dst_encoding) as f:
                f.read()
            # print(f"跳过 (已是 UTF-8): {file_path}")
            return False # 不需要转换
        except UnicodeDecodeError:
            pass # 不是 UTF-8，继续尝试源编码

        # 2. 尝试以源编码(GBK/GB2312)读取
        content = None
        used_encoding = None
        for enc in src_encodings:
            try:
                with open(file_path, 'r', encoding=enc) as f:
                    content = f.read()
                used_encoding = enc
                break
            except UnicodeDecodeError:
                continue
        
        if content is None:
            print(f"警告: 无法识别编码，跳过: {file_path}")
            return False

        # 3. 如果成功以 GBK 读取，则写入 UTF-8
        # 先创建备份
        backup_path = file_path + '.bak'
        try:
            # 复制原文件到备份
            with open(file_path, 'rb') as src:
                with open(backup_path, 'wb') as dst:
                    dst.write(src.read())
            
            # 写入 UTF-8 内容
            with open(file_path, 'w', encoding=dst_encoding) as f:
                f.write(content)
                
            print(f"转换成功: {file_path} (从 {used_encoding} -> {dst_encoding})")
            return True
            
        except Exception as e:
            print(f"错误: 写入文件失败 {file_path}: {e}")
            # 如果写入失败，可以尝试恢复备份（此处简化处理，仅提示）
            return False

    except Exception as e:
        print(f"错误: 处理文件失败 {file_path}: {e}")
        return False

def process_directory(root_dir):
    """递归处理目录下的所有目标文件"""
    converted_count = 0
    skipped_count = 0
    
    for root, dirs, files in os.walk(root_dir):
        for filename in files:
            if is_target_file(filename):
                file_path = os.path.join(root, filename)
                # 忽略备份文件本身，防止无限循环
                if file_path.endswith('.bak'):
                    continue
                
                if convert_file_encoding(file_path):
                    converted_count += 1
                else:
                    skipped_count += 1
                    
    print("-" * 30)
    print(f"处理完成。")
    print(f"转换文件数: {converted_count}")
    print(f"跳过/已转换文件数: {skipped_count}")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='将目录及子目录下的指定文件从 GBK/GB2312 转换为 UTF-8')
    parser.add_argument('directory', help='要处理的根目录路径')
    
    args = parser.parse_args()
    
    if not os.path.isdir(args.directory):
        print(f"错误: 目录 '{args.directory}' 不存在或无效。")
        sys.exit(1)
        
    print(f"开始扫描目录: {os.path.abspath(args.directory)}")
    print("注意：原文件将被备份为 .bak 文件")
    process_directory(args.directory)
