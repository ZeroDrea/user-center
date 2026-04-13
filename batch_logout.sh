#!/bin/bash

# 配置
BASE_URL="http://127.0.0.1:8888"
TOKEN_FILE="tokens.txt"   # 包含 token 的文件，每行格式 "username token"

# 颜色输出
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

if [ ! -f "$TOKEN_FILE" ]; then
    echo -e "${RED}错误: Token 文件 $TOKEN_FILE 不存在${NC}"
    exit 1
fi

echo "开始批量登出..."
echo "----------------------------------------"

while IFS= read -r line; do
    # 跳过空行
    [ -z "$line" ] && continue
    # 提取用户名和 token（假设格式 "username token"）
    USERNAME=$(echo "$line" | awk '{print $1}')
    TOKEN=$(echo "$line" | awk '{print $2}')
    
    if [ -z "$TOKEN" ]; then
        echo -e "${RED}✗ 跳过无效行: $line${NC}"
        continue
    fi
    
    # 发送登出请求
    RESPONSE=$(curl -s -X POST "$BASE_URL/user/logout" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json")
    
    # 解析响应中的 code
    CODE=$(echo "$RESPONSE" | jq -r '.code // 1')
    
    if [ "$CODE" == "0" ]; then
        echo -e "${GREEN}✓ 登出成功: $USERNAME${NC}"
    else
        echo -e "${RED}✗ 登出失败: $USERNAME - 响应: $RESPONSE${NC}"
    fi
    
    sleep 0.1
done < "$TOKEN_FILE"

echo "----------------------------------------"
echo "批量登出完成。"