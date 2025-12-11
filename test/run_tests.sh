#!/bin/bash
# @file run_tests.sh
# @brief CDD 编译器测试脚本
# @author mozihe
#
# 批量运行编译器测试用例，验证词法、语法、语义和运行时功能。

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 计数器
TOTAL=0
PASSED=0
FAILED=0
SKIPPED=0

# 编译器路径
CDD_COMPILER="${CDD_COMPILER:-./build/driver/cdd}"

# 测试目录
TEST_DIR="./test"

# 输出目录
OUTPUT_DIR="./test_output"

# 详细输出标志
VERBOSE=0

# 使用方法
usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -h, --help      显示此帮助信息"
    echo "  -v, --verbose   详细输出模式"
    echo "  -c, --compiler  指定编译器路径 (默认: ./build/driver/cdd)"
    echo "  -d, --dir       指定测试目录 (默认: ./test)"
    echo "  -o, --output    指定输出目录 (默认: ./test_output)"
    echo "  -f, --file      只运行指定的测试文件"
    echo "  -p, --pattern   只运行匹配模式的测试文件"
    echo ""
    echo "Examples:"
    echo "  $0                           # 运行所有测试"
    echo "  $0 -v                        # 详细模式"
    echo "  $0 -f test/keywords/test_int.cdd  # 运行单个测试"
    echo "  $0 -p '*control*'            # 运行匹配模式的测试"
}

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_skip() {
    echo -e "${YELLOW}[SKIP]${NC} $1"
}

# 解析期望退出码
get_expected_exit_code() {
    local test_file="$1"
    local marker
    marker=$(grep -Eo "EXPECT_EXIT:\s*-?[0-9]+" "$test_file" | tail -n1)
    if [ -n "$marker" ]; then
        echo "$marker" | sed -E 's/.*EXPECT_EXIT:\s*//'
    fi
}

# 运行单个测试
run_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .cdd)
    local output_file="$OUTPUT_DIR/$test_name.out"
    local error_file="$OUTPUT_DIR/$test_name.err"
    local runtime_output_file="$OUTPUT_DIR/$test_name.run.out"
    local expected_exit
    expected_exit=$(get_expected_exit_code "$test_file")
    
    ((TOTAL++))
    
    # 检查编译器是否存在
    if [ ! -x "$CDD_COMPILER" ]; then
        log_skip "$test_name (编译器不存在: $CDD_COMPILER)"
        ((SKIPPED++))
        return
    fi
    
    # 创建输出目录
    mkdir -p "$OUTPUT_DIR"
    
    if [ -n "$expected_exit" ]; then
        # 编译并运行，验证退出码
        local bin_file="$OUTPUT_DIR/$test_name.bin"
        if [ $VERBOSE -eq 1 ]; then
            log_info "Compile & run: $CDD_COMPILER -c $test_file -o $bin_file (EXPECT_EXIT=$expected_exit)"
        fi
        
        "$CDD_COMPILER" -c "$test_file" -o "$bin_file" > "$output_file" 2> "$error_file"
        local compile_code=$?
        
        if [ $compile_code -ne 0 ]; then
            log_fail "$test_name (compile failed: $compile_code)"
            ((FAILED++))
            if [ $VERBOSE -eq 1 ]; then
                echo "  Error output:"
                cat "$error_file" | sed 's/^/    /'
            fi
            return
        fi
        
        chmod +x "$bin_file"
        "$bin_file" > "$runtime_output_file" 2>> "$error_file"
        local runtime_code=$?
        
        if [ $runtime_code -eq $expected_exit ]; then
            log_pass "$test_name (exit=$runtime_code)"
            ((PASSED++))
        else
            log_fail "$test_name (exit $runtime_code != expect $expected_exit)"
            ((FAILED++))
            if [ $VERBOSE -eq 1 ]; then
                echo "  Error output:"
                cat "$error_file" | sed 's/^/    /'
                if [ -s "$runtime_output_file" ]; then
                    echo "  Runtime output:"
                    cat "$runtime_output_file" | sed 's/^/    /'
                fi
            fi
        fi
    else
        # 编译并运行，默认期望退出码为0
        local bin_file="$OUTPUT_DIR/$test_name.bin"
        if [ $VERBOSE -eq 1 ]; then
            log_info "Compile & run: $CDD_COMPILER -c $test_file -o $bin_file (expect exit=0)"
        fi
        
        "$CDD_COMPILER" -c "$test_file" -o "$bin_file" > "$output_file" 2> "$error_file"
        local compile_code=$?
        
        if [ $compile_code -ne 0 ]; then
            log_fail "$test_name (compile failed: $compile_code)"
            ((FAILED++))
            if [ $VERBOSE -eq 1 ]; then
                echo "  Error output:"
                cat "$error_file" | sed 's/^/    /'
            fi
            return
        fi
        
        chmod +x "$bin_file"
        "$bin_file" > "$runtime_output_file" 2>> "$error_file"
        local runtime_code=$?
        
        if [ $runtime_code -eq 0 ]; then
            log_pass "$test_name (exit=0)"
            ((PASSED++))
        else
            log_fail "$test_name (exit $runtime_code != expect 0)"
            ((FAILED++))
            if [ $VERBOSE -eq 1 ]; then
                echo "  Error output:"
                cat "$error_file" | sed 's/^/    /'
                if [ -s "$runtime_output_file" ]; then
                    echo "  Runtime output:"
                    cat "$runtime_output_file" | sed 's/^/    /'
                fi
            fi
        fi
    fi
}

# 运行目录中的所有测试
run_tests_in_dir() {
    local dir="$1"
    local pattern="${2:-*.cdd}"
    
    if [ ! -d "$dir" ]; then
        log_warn "目录不存在: $dir"
        return
    fi
    
    # 查找所有测试文件
    while IFS= read -r -d '' test_file; do
        run_test "$test_file"
    done < <(find "$dir" -name "$pattern" -type f -print0 | sort -z)
}

# 运行单个错误测试（期望编译失败）
run_error_test() {
    local test_file="$1"
    local category="$2"
    local test_name=$(basename "$test_file" .cdd)
    local error_file="$OUTPUT_DIR/$test_name.err"
    
    ((TOTAL++))
    
    # 检查编译器是否存在
    if [ ! -x "$CDD_COMPILER" ]; then
        log_skip "$category/$test_name (编译器不存在: $CDD_COMPILER)"
        ((SKIPPED++))
        return
    fi
    
    # 创建输出目录
    mkdir -p "$OUTPUT_DIR"
    
    if [ $VERBOSE -eq 1 ]; then
        log_info "Error test: $CDD_COMPILER -c $test_file (期望编译失败)"
    fi
    
    # 运行编译器，期望失败（返回非零）
    local output
    output=$(timeout 5 "$CDD_COMPILER" -c "$test_file" -o /tmp/test_error_$$.bin 2>&1)
    local exit_code=$?
    
    # 清理临时文件
    rm -f /tmp/test_error_$$.bin /tmp/test_error_$$.bin.s
    
    if [ $exit_code -eq 124 ]; then
        log_fail "$category/$test_name (编译器超时/死循环)"
        ((FAILED++))
    elif [ $exit_code -ne 0 ]; then
        log_pass "$category/$test_name (正确检测到错误)"
        ((PASSED++))
        if [ $VERBOSE -eq 1 ]; then
            echo "  错误输出:"
            echo "$output" | sed 's/^/    /'
        fi
    else
        log_fail "$category/$test_name (应该报错但编译通过)"
        ((FAILED++))
    fi
}

# 运行目录中的所有错误测试
run_error_tests_in_dir() {
    local dir="$1"
    local category="$2"
    
    if [ ! -d "$dir" ]; then
        return
    fi
    
    # 查找所有测试文件
    while IFS= read -r -d '' test_file; do
        # 只运行带有 EXPECT_COMPILE_ERROR 标记的文件
        if grep -q "EXPECT_COMPILE_ERROR" "$test_file" 2>/dev/null; then
            run_error_test "$test_file" "$category"
        fi
    done < <(find "$dir" -name "*.cdd" -type f -print0 | sort -z)
}

# 打印测试摘要
print_summary() {
    echo ""
    echo "================================"
    echo "          测试摘要"
    echo "================================"
    echo -e "总计:  $TOTAL"
    echo -e "${GREEN}通过:  $PASSED${NC}"
    echo -e "${RED}失败:  $FAILED${NC}"
    echo -e "${YELLOW}跳过:  $SKIPPED${NC}"
    echo "================================"
    
    if [ $FAILED -gt 0 ]; then
        echo -e "${RED}测试未全部通过!${NC}"
        return 1
    else
        echo -e "${GREEN}所有测试通过!${NC}"
        return 0
    fi
}

# 解析命令行参数
SINGLE_FILE=""
PATTERN=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -c|--compiler)
            CDD_COMPILER="$2"
            shift 2
            ;;
        -d|--dir)
            TEST_DIR="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -f|--file)
            SINGLE_FILE="$2"
            shift 2
            ;;
        -p|--pattern)
            PATTERN="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# 主程序
main() {
    echo "================================"
    echo "    Cdd 编译器测试套件"
    echo "================================"
    echo ""
    log_info "编译器: $CDD_COMPILER"
    log_info "测试目录: $TEST_DIR"
    log_info "输出目录: $OUTPUT_DIR"
    echo ""
    
    if [ -n "$SINGLE_FILE" ]; then
        # 运行单个测试文件
        if [ -f "$SINGLE_FILE" ]; then
            run_test "$SINGLE_FILE"
        else
            log_fail "文件不存在: $SINGLE_FILE"
            exit 1
        fi
    elif [ -n "$PATTERN" ]; then
        # 运行匹配模式的测试
        run_tests_in_dir "$TEST_DIR" "$PATTERN"
    else
        # 运行所有测试
        log_info "运行关键字测试..."
        run_tests_in_dir "$TEST_DIR/keywords"
        
        echo ""
        log_info "运行字面量测试..."
        run_tests_in_dir "$TEST_DIR/literals"
        
        echo ""
        log_info "运行标点符号测试..."
        run_tests_in_dir "$TEST_DIR/punctuations"
        
        echo ""
        log_info "运行运算符测试..."
        run_tests_in_dir "$TEST_DIR/operators"
        
        echo ""
        log_info "运行复杂用例测试..."
        run_tests_in_dir "$TEST_DIR/complex"
        
        echo ""
        log_info "运行预处理器测试..."
        run_tests_in_dir "$TEST_DIR/preprocessor"
        
        echo ""
        log_info "运行运行期测试..."
        run_tests_in_dir "$TEST_DIR/runtime"
        
        # 运行错误测试（期望编译失败的测试）
        if [ -d "$TEST_DIR/err" ]; then
            echo ""
            log_info "运行错误检测测试..."
            
            if [ -d "$TEST_DIR/err/lexer" ]; then
                log_info "  词法分析错误测试..."
                run_error_tests_in_dir "$TEST_DIR/err/lexer" "lexer"
            fi
            
            if [ -d "$TEST_DIR/err/parser" ]; then
                echo ""
                log_info "  语法分析错误测试..."
                run_error_tests_in_dir "$TEST_DIR/err/parser" "parser"
            fi
            
            if [ -d "$TEST_DIR/err/semantic" ]; then
                echo ""
                log_info "  语义分析错误测试..."
                run_error_tests_in_dir "$TEST_DIR/err/semantic" "semantic"
            fi
        fi
    fi
    
    print_summary
    exit $?
}

main
