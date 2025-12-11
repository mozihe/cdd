#!/bin/bash
#
# CDD Compiler Build Script
# 快速构建脚本
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
BUILD_DIR="$PROJECT_DIR/build"

# 默认选项
BUILD_TYPE="Release"
CLEAN_BUILD=false
INSTALL=false
RUN_TESTS=false
VERBOSE=false

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

usage() {
    echo "CDD Compiler Build Script"
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -d, --debug     Debug build (default: Release)"
    echo "  -c, --clean     Clean build directory first"
    echo "  -i, --install   Install after build"
    echo "  -t, --test      Run tests after build"
    echo "  -v, --verbose   Verbose output"
    echo "  -h, --help      Show this help"
    echo ""
    echo "Examples:"
    echo "  $0              # Release build"
    echo "  $0 -d           # Debug build"
    echo "  $0 -c -t        # Clean build and run tests"
    echo "  $0 -i           # Build and install"
}

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -i|--install)
            INSTALL=true
            shift
            ;;
        -t|--test)
            RUN_TESTS=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            usage
            exit 1
            ;;
    esac
done

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  CDD Compiler Build${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "Build type:  ${GREEN}$BUILD_TYPE${NC}"
echo -e "Clean build: ${GREEN}$CLEAN_BUILD${NC}"
echo -e "Install:     ${GREEN}$INSTALL${NC}"
echo -e "Run tests:   ${GREEN}$RUN_TESTS${NC}"
echo ""

# 清理
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# 创建构建目录
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 配置
echo -e "${YELLOW}Configuring...${NC}"
cmake_args="-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
if [ "$VERBOSE" = true ]; then
    cmake_args="$cmake_args -DCMAKE_VERBOSE_MAKEFILE=ON"
fi
cmake .. $cmake_args

# 构建
echo -e "${YELLOW}Building...${NC}"
NPROC=$(nproc 2>/dev/null || echo 4)
make -j$NPROC

echo ""
echo -e "${GREEN}Build successful!${NC}"

# 测试
if [ "$RUN_TESTS" = true ]; then
    echo ""
    echo -e "${YELLOW}Running tests...${NC}"
    cd "$PROJECT_DIR"
    ./test/run_tests.sh
fi

# 安装
if [ "$INSTALL" = true ]; then
    echo ""
    echo -e "${YELLOW}Installing...${NC}"
    cd "$BUILD_DIR"
    sudo make install
    echo -e "${GREEN}Installation complete!${NC}"
fi

echo ""
echo -e "${GREEN}Done!${NC}"
