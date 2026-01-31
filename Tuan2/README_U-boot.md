# TUẦN 2: BUILD VÀ BOOT U-BOOT CHO BEAGLEBONE BLACK

Tuần này tập trung vào việc biên dịch U-Boot bằng toolchain cross-compile , ghi U-Boot lên thẻ nhớ SD và kiểm tra quá trình khởi động thông qua cổng UART trên BeagleBone Black.

---

## 1. Mục tiêu

- Hiểu vai trò của U-Boot trong chuỗi khởi động Embedded Linux.
- Biết cách build U-Boot cho BeagleBone Black bằng toolchain cross-compile.
- Chuẩn bị thẻ nhớ SD để boot U-Boot.
- Kiểm tra U-Boot thông qua UART (minicom).

---

## 2. Chuẩn bị thư mục làm việc và thiết lập môi trường

- Tạo thư mục 
```
mkdir -p ~/Documents/Beaglebone_black
cd ~/Documents/Beaglebone_black
```
- Tải mã nguồn u-boot
```
git clone https://source.denx.de/u-boot/u-boot.git
cd u-boot
git checkout v2024.04
```
- Thiết lập toochain
```
# Thiết lập tiền tố cho trình biên dịch
export CROSS_COMPILE=arm-duong-linux-musleabihf-

# Thêm đường dẫn chứa toolchain vào hệ thống
export PATH=$HOME/x-tools/arm-duong-linux-musleabihf/bin:$PATH
```

---

## 3. Cài đặt thư viện bổ sung

### 3.1 Phần mềm hỗ trợ 
```
sudo apt update
sudo apt install libssl-dev device-tree-compiler swig python3-dev  python3-setuptools	
```
Phần mềm hỗ trợ này giúp quá trình build không bị lỗi.


### 3.2 Cấu hình và biên dịch
```
# Cấu hình mặc định cho chip AM335x (trên BBB)
make am335x_evm_defconfig

# Bắt đầu build với Device Tree của BeagleBone Black
make DEVICE_TREE=am335x-boneblack
```
Sau khi chạy 2 lệnh này thì trong thư mục hiện tại sẽ xuất hiện file MLO và u-boot.img
## 4. Chuẩn bị thẻ nhớ để boot u-boot
- Ban đầu  xoá hết dữ liệu hiện tại trong thẻ nhớ bằng cách cài gparted trên terminal.
```
sudo apt update
sudo apt install gparted
```
- Mở gparted áu đó trỏ đến /dev/sda rồi xoá hết dữ liệu đi.
- Tiếp theo là phân vung cho thẻ nhớ 
```
1.	Gõ lệnh: sudo fdisk /dev/sda
2.	Xóa/Tạo mới bảng phân vùng: Gõ o rồi Enter (để chọn kiểu DOS/MBR).
3.	Tạo phân vùng BOOT: - Gõ n -> Enter -> p -> Enter -> 1 -> Enter.
•	First sector: Enter.
•	Last sector: Gõ +128M rồi Enter.
4.	Tạo phân vùng ROOTFS:
•	Gõ n -> Enter -> p -> Enter -> 2 -> Enter.
•	First sector: Enter.
•	Last sector: Enter (để lấy hết phần còn lại).
5.	Thiết lập loại phân vùng và cờ Boot:
•	Gõ t -> 1 -> gõ c (Thiết lập sdb1 là W95 FAT32 LBA).
•	Gõ a -> 1 (Để có dấu sao * ở cột Boot, BBB rất cần cái này).
6.	Lưu lại: Gõ w rồi Enter
```
- Định dạng phân vùng để tạo ra "Hệ thống tệp"
```
# Format phân vùng BOOT
sudo mkfs.vfat -F 32 -n BOOT /dev/sda1

# Format phân vùng ROOTFS (để sau này chứa hệ điều hành)
sudo mkfs.ext4 -L ROOTFS /dev/sda2
```
## 5. Chuẩn bị BEAGLEBONE BLACK và USART để boot u-boot
-	Rút thẻ nhớ ra và cắm lại vào máy tính để Ubuntu tự động nhận diện (mount). Nó sẽ hiện ra trong trình quản lý file là BOOT và ROOTFS
- Mở terminal và chạy đoạn lệnh 
```
cd ~/Documents/Beaglebone_black/u-boot

# Copy MLO đầu tiên
cp MLO /media/rimuru/BOOT/

# Copy u-boot.img thứ hai
cp u-boot.img /media/rimuru/BOOT/

# Lệnh quan trọng để đảm bảo dữ liệu đã ghi xong
sync
```
- Rút an toàn và test 
```
1.	Chuột phải vào biểu tượng thẻ nhớ chọn Eject hoặc gõ: sudo umount /dev/sda1.
2.	Cắm thẻ vào BeagleBone Black.
3.	Giữ nút S2 (nút Boot) thật chắc.
4.	Cắm nguồn và nhìn màn hình terminal (Picocom/Putty).
```
Sau đó Nhìn vào minicom rồi ấn nút RESET của BBB thì ta sẽ được kết quả cuối cùng.




