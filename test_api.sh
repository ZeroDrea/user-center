#!/bin/bash

BASE_URL="http://127.0.0.1:8888"
USERNAME="alice"
PASSWORD="123456"

# 颜色输出
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo "========== 用户中心 API 自动化测试 =========="

# 1. 登录
echo -n "1. 登录... "
LOGIN_RESP=$(curl -s -X POST "$BASE_URL/user/login" \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"$USERNAME\",\"password\":\"$PASSWORD\"}")

TOKEN=$(echo "$LOGIN_RESP" | jq -r '.token')
if [ "$TOKEN" == "null" ] || [ -z "$TOKEN" ]; then
    echo -e "${RED}失败${NC}"
    echo "响应: $LOGIN_RESP"
    exit 1
else
    echo -e "${GREEN}成功${NC}，获取到 Token: ${TOKEN:0:16}..."
fi

# 2. 获取用户信息（需要 Token）
echo -n "2. 获取用户信息... "
INFO_RESP=$(curl -s -H "Authorization: Bearer $TOKEN" "$BASE_URL/user/info")
CODE=$(echo "$INFO_RESP" | jq -r '.code // 200')
if [ "$CODE" == "200" ] || [ "$CODE" == "null" ]; then
    # 正常返回用户信息，没有 code 字段或 code=200
    USERNAME_RET=$(echo "$INFO_RESP" | jq -r '.username')
    if [ "$USERNAME_RET" == "$USERNAME" ]; then
        echo -e "${GREEN}成功${NC}，用户信息匹配"
    else
        echo -e "${RED}失败${NC}，用户名不匹配"
        echo "响应: $INFO_RESP"
        exit 1
    fi
else
    echo -e "${RED}失败${NC}，HTTP 未授权或错误"
    echo "响应: $INFO_RESP"
    exit 1
fi

# 3. 登出
echo -n "3. 登出... "
LOGOUT_RESP=$(curl -s -X POST -H "Authorization: Bearer $TOKEN" "$BASE_URL/user/logout")
LOGOUT_CODE=$(echo "$LOGOUT_RESP" | jq -r '.code')
if [ "$LOGOUT_CODE" == "0" ]; then
    echo -e "${GREEN}成功${NC}"
else
    echo -e "${RED}失败${NC}"
    echo "响应: $LOGOUT_RESP"
    exit 1
fi

# 4. 验证登出后 token 失效
echo -n "4. 验证登出后 token 失效... "
INFO_AFTER=$(curl -s -H "Authorization: Bearer $TOKEN" "$BASE_URL/user/info")
AFTER_CODE=$(echo "$INFO_AFTER" | jq -r '.code // 200')
if [ "$AFTER_CODE" == "401" ] || [[ "$INFO_AFTER" == *"Unauthorized"* ]]; then
    echo -e "${GREEN}成功${NC}（Token 已失效）"
else
    echo -e "${RED}警告${NC}（Token 可能仍然有效）"
fi

echo "========== 测试完成 =========="