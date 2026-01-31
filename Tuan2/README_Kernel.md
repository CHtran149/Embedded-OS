# TUẦN 3: BUILD VÀ BOOT LINUX KERNEL CHO BEAGLEBONE BLACK

Tuần này tập trung vào việc tải mã nguồn Linux Kernel, cấu hình – biên dịch kernel cho BeagleBone Black bằng toolchain cross-compile, ghi kernel và device tree lên thẻ nhớ SD, và boot kernel thông qua U-Boot.

---

## 1. Mục tiêu

- Hiểu vai trò của Linux Kernel trong hệ thống Embedded Linux.
- Biết cách tải và quản lý mã nguồn Linux Kernel bằng Git.
- Biết cách cấu hình và build Linux Kernel cho BeagleBone Black.
- Boot kernel thủ công từ U-Boot.
- Kiểm tra kernel thông qua UART (minicom).

---

## 2. Chuẩn bị môi trường

### 2.1 Thiết lập biến môi trường (BẮT BUỘC mỗi lần mở terminal)
```
export ARCH=arm
export CROSS_COMPILE=arm-duong-linux-musleabihf-
export PATH=$HOME/x-tools/arm-duong-linux-musleabihf/bin:$PATH
```

---

## 3. Tải mã nguồn Linux Kernel
### 3.1 Tạo thư mục làm việc
```
mkdir -p ~/Documents/Beaglebone_black
cd ~/Documents/Beaglebone_black
```
### 3.2 Cấu hình và tải nhánh Stable
```
git remote add stable https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git
git fetch stable
git checkout stable/linux-5.4.y
```
- Cấu hình lại Kernel 
```
make multi_v7_defconfig
```
- Tiếp theo biên dịch 
```
make -j$(nproc) zImage dtbs
```
### 3.4 Copy Kernel và Device Tree vào thẻ nhớ
```
# 1. Di chuyển vào thư mục linux nếu chưa có ở đó
cd ~/Documents/Beaglebone_black/linux

# 2. Copy file zImage (Nhân Kernel)
cp arch/arm/boot/zImage /media/rimuru/BOOT/

# 3. Copy file Device Tree (Mô tả phần cứng cho BBB)
# Lưu ý: Với bản 5,4, file nằm trong thư mục dts
cp arch/arm/boot/dts/am335x-boneblack.dtb /media/rimuru/BOOT/
sync
```

## 4. Kiểm tra lại xem file đã nằm trong thẻ chưa
- Sau nước này kiểm tra "LS -l media/duong/BOOT"
![alt text](image.png)
 Như này tức là trong thẻ nhớ đã đủ file.
## 5. boot kerner bằng u-boot thông qua Minicom
### 5.1 mở Minicom 
```
sudo minicom -D /dev/ttyUSB0 -b 115200
```
### 5.2 Dừng autoboot 
```
Hit any key to stop autoboot:
```
- Thấy dòng này ấn enter để  vào trong thực hiện gõ lệnh của Minicom
### 5.3  Load và boot kernel thủ công
```
mmc list
mmc dev 0
ls mmc 0:1
load mmc 0:1 0x82000000 zImage
load mmc 0:1 0x88000000 am335x-boneblack.dtb
bootz 0x82000000 - 0x88000000
```
- Sau khi thực hiện xong lệnh này sẽ được kết quả cuối cùng.



