# TUẦN 1: BUILD TOOLCHAIN CROSS-COMPILE THỦ CÔNG BẰNG CROSSTOOL_NG

Tuần đầu tiên sẽ mô tả toàn bộ các bước chuẩn bị môi trường, xây dựng toolchain cross-compiling bằng Crosstool-NG với thư viện musl, và kiểm tra toolchain bằng QEMU.

## Mục tiêu:
- Biết cách cấu hình Crosstool-NG.
- Biết cách build toolchain cross-compile cho ARM.
- Sử dụng thư viện C musl.

## 1. Chuẩn bị dữ liệu thực hành (Lab Data)
- Các bài lab sử dụng một bộ dữ liệu đã được chuẩn bị sẵn (kernel image, kernel config, root filesystem,...).
- Thực hiện các lệnh sau trong terminal:
``` 
cd
wget https://bootlin.com/doc/training/embedded-linux-bbb/embedded-linux-bbb-labs.tar.xz
tar xvf embedded-linux-bbb-labs.tar.xz
```
- Sau khi giải nén, một thư mục `embedded-linux-bbb-labs` sẽ xuất hiện trong thư mục home `($HOME)`. Thư mục này sẽ được sử dụng xuyên suốt các bài lab, đồng thời là nơi lưu trữ các file sinh ra trong quá trình thực hành.

## 2. Cập nhật hệ thống
- Dể tránh các lỗi phát sinh trong quá trình cài đặt package, hãy cập nhật hệ thống:

```
sudo apt update
sudo apt dist-upgrade
```

## 3. Chuẩn bị môi trường Crosstool-NG
- Di chuyển vào thư mục toolchain:
` cd $HOME/embedded-linux-bbb-labs/toolchain`
- Yêu cầu hệ thống: RAM tối thiểu 4GB.
- Cài đặt các package cần thiết
```
sudo apt install build-essential git autoconf bison flex texinfo help2man gawk \
libtool-bin libncurses5-dev unzip
```
## 4. Tải Crosstool-NG
- Clone source code và checkout phiên bản đã được kiểm thử:
```
git clone https://github.com/crosstool-ng/crosstool-ng
cd crosstool-ng
git checkout crosstool-ng-1.26.0
```
## 5. Build và cài đặt Crosstool-NG
- Do build từ source git (không phải release archive), cần chạy bootstrap:
`./bootstrap`
- Cấu hình CCrosstool-NG thjeo cách cài đặt local:
```
./configure --enable-local
make
```
## 6. Cấu hình toolchain
- Sử dụng lệnh: `./ct-ng menuconfig`
- ### Các thiết lập quan trọng:
  - Path and misc options:
    - Enable: Try features marked as EXPERIMENTAL
    - Nếu hệ thống dùng wget2, bỏ tùy chọn --passive-ftp
  - Target options
    - Use specific FPU: vfpv3
    - Floating point: `hardware (FPU)`
  - Toolchain options
    - Tuple vendor string: `training`
    - Tuple alias: `arm-linux`
  - Operating System
    - Linux kernel headers: chọn phiên bản <= 6.6
  - C library
    - Chọn: `musl`
  - C compiler
    - GCC version: `13.2.0`
    - Enable C++ support
  - Debug facilities: Bỏ chọn toàn bộ
## 7. Build toolchain
- Chỉ cần chạy:
`./ct-ng build`
- Toolchain sẽ được cài đặt mặc định tại:
`$HOME/x-tools/`
- Quá trình build mất khá nhiều thời gian, khoảng 30 - 60 phút.

## 8. Kiểm tra toolchain
- Thêm toolchain vào PATH
`export PATH=$HOME/x-tools/arm-training-linux-musleabihf/bin:$PATH`
- Kiểm tra version toolchain:
`arm-training-linux-musleabihf-gcc --version`

## 9. Compile chương trình C với toolchain mới build được
- Compile chương trình test
`arm-training-linux-musleabihf-gcc hello.c -o hello_arm`
- Kiểm tra file binary
  - `file hello_arm`
hoặc
  - `arm-training-linux-musleabihf-readelf -h hello_arm`

## 10. Chạy chương trình ARM bằng QEMU
- Cài đặt QEMU user:
  `sudo apt install qemu-user`
- Chạy thử:
  `qemu-arm -L ~/x-tools/arm-training-linux-musleabihf/arm-training-linux-musleabihf/sysroot ./hello_arm`

## 11. Dọn dẹp (Tùy chọn)
- Nếu thiếu dung lượng ổ đĩa, có thể xóa các file trung gian (tiết kiệm ~9GB):
  `./ct-ng clean`
- Chỉ nên làm khi chắc chắn toolchain đã build thành công.