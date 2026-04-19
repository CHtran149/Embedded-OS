#!/bin/bash

# 1. Tự động tìm Interface đang kết nối Internet (Cổng thoát)
# Lệnh này tìm interface nào đang giữ Default Route của hệ thống
WAN_IFACE=$(ip route | grep default | awk '{print $5}' | head -n 1)

# 2. Tự động tìm Interface nối với BBB (Cổng vào)
# Tìm interface bắt đầu bằng 'enx' hoặc 'usb'
LAN_IFACE=$(ip -o link show | awk -F': ' '{print $2}' | grep -E '^enx|^usb' | head -n 1)

# Kiểm tra nếu không tìm thấy thiết bị
if [ -z "$LAN_IFACE" ]; then
    echo "LỖI: Không tìm thấy BeagleBone Black. Kiểm tra cáp USB!"
    exit 1
fi

if [ -z "$WAN_IFACE" ]; then
    echo "LỖI: Máy tính chưa kết nối Wi-Fi/Internet!"
    exit 1
fi

echo "--- CẤU HÌNH MẠNG TỰ ĐỘNG ---"
echo "Internet (WAN): $WAN_IFACE"
echo "BeagleBone (LAN): $LAN_IFACE"

# 3. Gán IP tĩnh cho cổng nối với BBB
sudo ifconfig $LAN_IFACE 192.168.7.1 netmask 255.255.255.0 up

# 4. Bật chuyển tiếp IP (Forwarding)
sudo sysctl -w net.ipv4.ip_forward=1

# 5. Xóa các luật cũ để làm sạch bảng NAT/Forward
sudo iptables -F
sudo iptables -t nat -F
sudo iptables -P FORWARD ACCEPT
sudo iptables -t nat -A POSTROUTING -o $WAN_IFACE -j MASQUERADE

# 6. Thiết lập NAT động dựa trên WAN_IFACE vừa tìm được
sudo iptables -t nat -A POSTROUTING -o $WAN_IFACE -j MASQUERADE

# 7. Cho phép lưu lượng đi qua (Forwarding)
sudo iptables -A FORWARD -i $LAN_IFACE -o $WAN_IFACE -j ACCEPT
sudo iptables -A FORWARD -m state --state ESTABLISHED,RELATED -j ACCEPT

# 8. Khởi động lại Mosquitto
sudo systemctl restart mosquitto

echo "--- HOÀN TẤT: BBB có thể ra Internet qua $WAN_IFACE ---"
