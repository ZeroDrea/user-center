#!/bin/bash

# 配置
BASE_URL="http://127.0.0.1:8888"
PASSWORD="Test123456"
START=1
END=100

# 颜色输出
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo "开始批量注册用户 (${START} - ${END})..."

for i in $(seq $START $END); do
    USERNAME="testuser_$i"
    EMAIL="testuser_$i@example.com"
    
    # 发送注册请求
    RESPONSE=$(curl -s -X POST "$BASE_URL/user/register" \
        -H "Content-Type: application/json" \
        -d "{\"username\":\"$USERNAME\",\"password\":\"$PASSWORD\",\"email\":\"$EMAIL\"}")
    
    # 解析响应中的 code 字段（假设返回 JSON 包含 "code"）
    CODE=$(echo "$RESPONSE" | jq -r '.code // 0')
    
    if [ "$CODE" == "0" ]; then
        echo -e "${GREEN}✓ 注册成功: $USERNAME ($EMAIL)${NC}"
    else
        echo -e "${RED}✗ 注册失败: $USERNAME - 响应: $RESPONSE${NC}"
    fi
    
    # 添加短暂延迟，避免瞬间压力过大
    sleep 0.1
done

echo "批量注册完成。"