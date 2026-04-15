#!/bin/bash

# 批量获取用户信息脚本

TOKENS_FILE="${1:-tokens.txt}"   # 默认使用 tokens.txt
BASE_URL="http://127.0.0.1:8888"

if [ ! -f "$TOKENS_FILE" ]; then
    echo "错误: Token 文件 $TOKENS_FILE 不存在，请先运行 batch_login.sh 生成。"
    exit 1
fi

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo "开始批量获取用户信息..."
echo "----------------------------------------"

while IFS= read -r line; do
    # 跳过空行和注释
    [[ -z "$line" || "$line" =~ ^# ]] && continue
    # 提取用户名和 token（格式: username token）
    USERNAME=$(echo "$line" | awk '{print $1}')
    TOKEN=$(echo "$line" | awk '{print $2}')
    
    if [ -z "$TOKEN" ] || [ "$TOKEN" == "null" ]; then
        echo -e "${RED}✗ 跳过: $USERNAME (无有效 Token)${NC}"
        continue
    fi
    
    # 发送请求获取用户信息
    RESPONSE=$(curl -s -H "Authorization: Bearer $TOKEN" "$BASE_URL/user/info")
    
    CODE=$(echo "$RESPONSE" | jq -r '.code')
    if [ "$CODE" == "0" ]; then
        NICKNAME=$(echo "$RESPONSE" | jq -r '.data.nickname // .data.username')
        echo -e "${GREEN}✓ $USERNAME -> 昵称: $NICKNAME${NC}"
    else
        echo -e "${RED}✗ $USERNAME 获取失败 (响应: $RESPONSE)${NC}"
    fi

    sleep 0.1
done < "$TOKENS_FILE"

echo "----------------------------------------"
echo "批量获取完成。"