#!/bin/bash

# 检查是否有参数传入
if [ $# -eq 0 ]; then
    echo "No arguments provided."
    exit 1
fi

# 打印所有传入的参数
echo "Arguments provided:"
for arg in "$@"; do
    echo "  $arg"
done

# 检查是否至少有一个参数传入
if [ $# -lt 1 ]; then
    echo "Usage: $0 <filament_version>"
    exit 1
fi

# 获取第一个和第二个参数
new_filament_version=$1
echo "new_filament_version : $new_filament_version"
echo ""


# 正则表达式匹配旧版本号部分
version_pattern='[0-9]+\.[0-9]+\.[0-9]+'

old_filament_home=$FILAMENT_HOME
echo "old_filament_home         : $old_filament_home"
new_filament_home=$(echo "$old_filament_home" | sed -E "s/$version_pattern/$new_filament_version/")
echo "new_filament_home         : $new_filament_home"
## Debugging output to see the transformation step by step
#echo "Debug: Replacing version pattern in FILAMENT_HOME"
#echo "  Command: echo \"$old_filament_home\" | sed -E \"s/$version_pattern/$new_filament_version/\""
#echo "  Result: $(echo "$old_filament_home" | sed -E "s/$version_pattern/$new_filament_version/")"
# 检查目录是否存在，如果不存在则创建目录
if [ ! -d "$new_filament_home" ]; then
    echo "Directory $new_filament_home does not exist. Creating it now..."
    mkdir -p "$new_filament_home"
    if [ $? -eq 0 ]; then
        echo "Directory $new_filament_home created successfully."
    else
        echo "Failed to create directory $new_filament_home."
        exit 1
    fi
else
    echo "Directory $new_filament_home already exists."
fi

echo ""

old_filament_android_home=$FILAMENT_ANDROID_HOME
echo "old_filament_android_home : $old_filament_android_home"
new_filament_android_home=$(echo "$old_filament_android_home" | sed -E "s/$version_pattern/$new_filament_version/")
echo "new_filament_android_home : $new_filament_android_home"
## Debugging output to see the transformation step by step
#echo "Debug: Replacing version pattern in FILAMENT_ANDROID_HOME"
#echo "  Command: echo \"$old_filament_android_home\" | sed -E \"s/$version_pattern/$new_filament_version/\""
#echo "  Result: $(echo "$old_filament_android_home" | sed -E "s/$version_pattern/$new_filament_version/")"
# 检查目录是否存在，如果不存在则创建目录
if [ ! -d "$new_filament_android_home" ]; then
    echo "Directory $new_filament_android_home does not exist. Creating it now..."
    mkdir -p "$new_filament_android_home"
    if [ $? -eq 0 ]; then
        echo "Directory $new_filament_android_home created successfully."
    else
        echo "Failed to create directory $new_filament_android_home."
        exit 1
    fi
else
    echo "Directory $new_filament_android_home already exists."
fi

echo ""
./build.sh -p desktop,android -a release
echo ""

# 使用 uname 获取操作系统类型
os_type=$(uname)
echo "unzip..."
if [ "$os_type" == "Darwin" ]; then
  tar -xzvf out/filament-release-darwin.tgz -C $(dirname $new_filament_home)
  tar -xzvf out/filament-android-release-darwin.tgz -C $(dirname $new_filament_android_home)
elif [ "$os_type" == "Linux" ]; then
  tar -xzvf out/filament-release-linux.tgz -C $(dirname $new_filament_home)
  tar -xzvf out/filament-android-release-linux.tgz -C $(dirname $new_filament_android_home)
else
    echo "The current host system is not recognized. It is: $os_type"
fi