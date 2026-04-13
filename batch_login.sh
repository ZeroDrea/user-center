#!/bin/bash

# 配置
BASE_URL="http://127.0.0.1:8888"
PASSWORD="Test123456"
START=1
END=100
OUTPUT_FILE="tokens.txt"   # 可选：保存 token 的文件

# 颜色输出
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo "开始批量登录获取 Token (用户 ${START} - ${END})..."
echo "----------------------------------------"

# 清空输出文件（如果存在）
> "$OUTPUT_FILE"

for i in $(seq $START $END); do
    USERNAME="testuser_$i"
    
    # 发送登录请求
    RESPONSE=$(curl -s -X POST "$BASE_URL/user/login" \
        -H "Content-Type: application/json" \
        -d "{\"username\":\"$USERNAME\",\"password\":\"$PASSWORD\"}")
    
    # 解析 token（响应为 {"code":0,"msg":"Login success","token":"..."}）
    TOKEN=$(echo "$RESPONSE" | jq -r '.token')
    CODE=$(echo "$RESPONSE" | jq -r '.code // 1')
    
    if [ "$CODE" == "0" ] && [ "$TOKEN" != "null" ] && [ -n "$TOKEN" ]; then
        echo -e "${GREEN}✓ 登录成功: $USERNAME -> Token: ${TOKEN:0:32}...${NC}"
        echo "$USERNAME $TOKEN" >> "$OUTPUT_FILE"
    else
        echo -e "${RED}✗ 登录失败: $USERNAME (响应: $RESPONSE)${NC}"
    fi
    
    sleep 0.1
done

echo "----------------------------------------"
echo "批量登录完成。Token 已保存到: $OUTPUT_FILE"