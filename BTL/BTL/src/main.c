#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <linux/input.h>
#include <mosquitto.h>
#include "OLED_LCD_SSD1306.h"
#include "fonts.h"

// --- Cấu hình thiết bị ---
#define LED_DEVICE "/dev/led_test"
#define BUTTON_DEVICE "/dev/input/my_button" // Bạn hãy kiểm tra lại eventX bằng lệnh 'cat /proc/bus/input/devices'
// --- Cấu hình MQTT ---
#define MQTT_HOST "broker.emqx.io"//"7ae02c3c05de4372aaf8623029444fd0.s1.eu.hivemq.cloud"
#define MQTT_PORT 1883 // Bắt buộc dùng 8883 cho HiveMQ Cloud
#define MQTT_USER NULL//"chien_bbb"
#define MQTT_PASS NULL//"Chien149@" // Mật khẩu bạn vừa dùng trong ảnh
#define MQTT_TOPIC_PUB "pzem/data"
#define MQTT_TOPIC_SUB "pzem/config"
// --- Biến toàn cục ---
pthread_mutex_t data_lock = PTHREAD_MUTEX_INITIALIZER;
char pzem_data[128] = "Loading...";

float thresholds[] = {10.0, 30.0, 50.0, 100.0, 200.0, 300.0, 500.0}; 
int threshold_index = 0; 
float power_threshold = 10.0;
int is_overloaded = 0;         // Cờ báo quá tải
int fd_oled = -1;
SSD1306_Name myOLED;
// Biến lưu dữ liệu thô để gửi qua MQTT
char mqtt_payload[256];
struct mosquitto *global_mosq = NULL; // Thêm dòng này

void update_oled_display(float limit) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Limit:%.0fW", limit);
    
    // Vẽ đè lên dòng thứ 4 của OLED
    SSD1306_GotoXY(&myOLED, 0, 48);
    SSD1306_Puts(&myOLED, "                ", &Font_7x10, SSD1306_COLOR_WHITE); // Xóa dòng cũ
    SSD1306_GotoXY(&myOLED, 0, 48);
    SSD1306_Puts(&myOLED, buf, &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_UpdateScreen(&myOLED);
}

// --- Luồng 1: Đọc từ Driver PZEM ---
void* thread_read_pzem(void* arg) {
    int fd;
    char buf[128];
    while(1) {
        fd = open("/dev/pzem_sensor", O_RDONLY);
        if (fd >= 0) {
            memset(buf, 0, sizeof(buf));
            int n = read(fd, buf, sizeof(buf) - 1);
            if (n > 0) {
                for(int i = 0; i < n; i++) {
                    if(buf[i] == '\n' || buf[i] == '\r') buf[i] = ' ';
                }
                pthread_mutex_lock(&data_lock);
                strncpy(pzem_data, buf, sizeof(pzem_data) - 1);
                pzem_data[sizeof(pzem_data) - 1] = '\0'; // Đảm bảo luôn có điểm kết thúc
                pthread_mutex_unlock(&data_lock);
            }
            close(fd);
        }
        usleep(500000); 
    }
    return NULL;
}

// --- Luồng 2: Hiển thị OLED và Kiểm tra ngưỡng ---
void* thread_display_oled(void* arg) {
    char display_buf[128];
    char v_raw[16], i_raw[16], p_raw[16], f_raw[16];
    char i_fmt[16], p_fmt[16], limit_fmt[16];

    while(1) {
        pthread_mutex_lock(&data_lock);
        strncpy(display_buf, pzem_data, sizeof(display_buf));
        pthread_mutex_unlock(&data_lock);

        if (sscanf(display_buf, "U: %[^|] | I: %[^|] | P: %[^|] | F: %[^|]", 
                   v_raw, i_raw, p_raw, f_raw) >= 4) {

            float i_val = atof(i_raw);
            float p_val = atof(p_raw);
            
            // Kiểm tra quá tải
            is_overloaded = (p_val > power_threshold) ? 1 : 0;

            snprintf(i_fmt, sizeof(i_fmt), "%.2fA", i_val);
            snprintf(p_fmt, sizeof(p_fmt), "%.2fW", p_val);
            snprintf(limit_fmt, sizeof(limit_fmt), "Limit:%.0fW", power_threshold);

            SSD1306_Fill(&myOLED, SSD1306_COLOR_BLACK);
            
            // Hiển thị Tiêu đề hoặc Cảnh báo
            SSD1306_GotoXY(&myOLED, 10, 0);
            if (is_overloaded) {
                SSD1306_Puts(&myOLED, "!! OVERLOAD !!", &Font_7x10, SSD1306_COLOR_WHITE);
            } else {
                SSD1306_Puts(&myOLED, "POWER MONITOR", &Font_7x10, SSD1306_COLOR_WHITE);
            }

            // Dòng 2: V và I
            SSD1306_GotoXY(&myOLED, 0, 18);
            SSD1306_Puts(&myOLED, "V:", &Font_7x10, SSD1306_COLOR_WHITE);
            SSD1306_GotoXY(&myOLED, 15, 18);
            SSD1306_Puts(&myOLED, v_raw, &Font_7x10, SSD1306_COLOR_WHITE);

            SSD1306_GotoXY(&myOLED, 65, 18);
            SSD1306_Puts(&myOLED, "I:", &Font_7x10, SSD1306_COLOR_WHITE);
            SSD1306_GotoXY(&myOLED, 80, 18);
            SSD1306_Puts(&myOLED, i_fmt, &Font_7x10, SSD1306_COLOR_WHITE);

            // Dòng 3: P hiện tại
            SSD1306_GotoXY(&myOLED, 0, 33);
            SSD1306_Puts(&myOLED, "P:", &Font_7x10, SSD1306_COLOR_WHITE);
            SSD1306_GotoXY(&myOLED, 15, 33);
            SSD1306_Puts(&myOLED, p_fmt, &Font_7x10, SSD1306_COLOR_WHITE);

            // Dòng 4: Ngưỡng giới hạn (Dễ theo dõi khi nhấn nút)
            SSD1306_GotoXY(&myOLED, 0, 48);
            SSD1306_Puts(&myOLED, limit_fmt, &Font_7x10, SSD1306_COLOR_WHITE);

            SSD1306_UpdateScreen(&myOLED);
        }
        usleep(300000); 
    }
    return NULL;
}

// --- Luồng 3: Điều khiển LED nhấp nháy ---
void* thread_led_blink(void* arg) {
    int fd_led = open(LED_DEVICE, O_WRONLY);
    if (fd_led < 0) {
        perror("Lỗi mở LED device");
        return NULL;
    }

    while(1) {
        if (is_overloaded) {
            write(fd_led, "1", 1);
            usleep(200000);
            write(fd_led, "0", 1);
            usleep(200000);
        } else {
            write(fd_led, "0", 1);
            usleep(500000);
        }
    }
    close(fd_led);
    return NULL;
}

void* thread_button_handler(void* arg) {
    int fd_btn = open(BUTTON_DEVICE, O_RDONLY);
    struct input_event ev;
    int num_thresholds = sizeof(thresholds) / sizeof(thresholds[0]);

    if (fd_btn < 0) {
        // Dự phòng nếu udev chưa tạo kịp symlink
        fd_btn = open("/dev/input/event1", O_RDONLY);
        if (fd_btn < 0) {
            perror("Lỗi nghiêm trọng: Không mở được thiết bị nút nhấn");
            return NULL;
        }
    }

    printf("Luồng 2 Button sẵn sàng. Ngưỡng hiện tại: %.0fW\n", power_threshold);

    while(1) {
        ssize_t n = read(fd_btn, &ev, sizeof(struct input_event));
        if (n < (ssize_t)sizeof(struct input_event)) {
            if (n < 0) perror("Lỗi đọc event nút nhấn");
            continue; // Đọc lại nếu có lỗi nhỏ hoặc không đủ dữ liệu
        }
        
        // ev.value == 1: Nhấn xuống (đã được Driver debounce)
        if (ev.type == EV_KEY && ev.value == 1) {
            
            pthread_mutex_lock(&data_lock);
            
            if (ev.code == KEY_UP) { // Mã KEY_UP từ Driver mới của bạn
                if (threshold_index < num_thresholds - 1) {
                    threshold_index++;
                } else {
                    printf("-> [MAX] ");
                }
            } 
            else if (ev.code == KEY_DOWN) { // Mã KEY_DOWN
                if (threshold_index > 0) {
                    threshold_index--;
                } else {
                    printf("-> [MIN] ");
                }
            }

            power_threshold = thresholds[threshold_index];
            float current_val = power_threshold; // Copy giá trị ra để dùng ngoài lock
            
            pthread_mutex_unlock(&data_lock);
            
            // 1. Đồng bộ lên Cloud ngay lập tức
            if (global_mosq != NULL) {
                char payload[16];
                snprintf(payload, sizeof(payload), "%.1f", current_val);
                mosquitto_publish(global_mosq, NULL, "pzem/config/status", strlen(payload), payload, 0, false);
            }

            // 2. Cập nhật OLED ngay lập tức (Rất quan trọng)
            // Gọi hàm vẽ OLED của bạn ở đây
            update_oled_display(current_val); 

            printf("Nút nhấn OK! Ngưỡng mới: %.0fW (Index: %d)\n", current_val, threshold_index);
        }
    }
    close(fd_btn);
    return NULL;
}

void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg) {
    if (msg->payloadlen > 0) {
        char temp_buf[32];
        int len = (msg->payloadlen < 31) ? msg->payloadlen : 31;
        
        memcpy(temp_buf, msg->payload, len);
        temp_buf[len] = '\0'; // Đảm bảo là một chuỗi hợp lệ
        
        float new_limit = atof(temp_buf);
        
        if (new_limit > 0) {
            pthread_mutex_lock(&data_lock);
            power_threshold = new_limit;
            pthread_mutex_unlock(&data_lock);
            printf("MQTT: Da nhan nguong moi tu Cloud: %.1f W\n", new_limit);
        }
    }
}

void* thread_mqtt(void* arg) {
    int rc;

    // 1. Khởi tạo thư viện
    mosquitto_lib_init();

    // Tạo client ID duy nhất
    // Trong thread_mqtt, sau khi mosquitto_new:
    global_mosq = mosquitto_new("BBB_Chien_Device_ID_999@@@@", true, NULL);
    if (!global_mosq) {
        printf("MQTT: Khong the tao instance!\n");
        return NULL;
    }

    mosquitto_message_callback_set(global_mosq, on_message);

    mosquitto_username_pw_set(global_mosq, NULL, NULL);

    // 5. Kết nối tới Broker
    printf("MQTT: Dang ket noi den %s...\n", MQTT_HOST);
    rc = mosquitto_connect(global_mosq, MQTT_HOST, MQTT_PORT, 60);
    mosquitto_loop_start(global_mosq);
    if (rc != MOSQ_ERR_SUCCESS) {
        printf("MQTT: Ket noi that bai! Ma loi: %d\n", rc);
        mosquitto_destroy(global_mosq);
        return NULL;
    }

    mosquitto_subscribe(global_mosq, NULL, "pzem/config", 0);
    printf("MQTT: Da ket noi va dang lang nghe tren topic 'pzem/config'\n");

    sleep(2);
    // 8. Vòng lặp gửi dữ liệu định kỳ
    while(1) {
        char local_pzem_copy[128] = "";

        pthread_mutex_lock(&data_lock);
        if (strcmp(pzem_data, "Loading...") != 0) {
            strncpy(local_pzem_copy, pzem_data, sizeof(local_pzem_copy) - 1);
        }
        pthread_mutex_unlock(&data_lock); // MỞ KHÓA NGAY LẬP TỨC

        if (strlen(local_pzem_copy) > 0) {
            int pub_rc = mosquitto_publish(global_mosq, NULL, "pzem/data", strlen(local_pzem_copy), local_pzem_copy, 0, false);
            
            if (pub_rc != MOSQ_ERR_SUCCESS) {
                printf("MQTT: Loi gui data, dang thu lai...\n");
                mosquitto_reconnect(global_mosq);
            }
        }
        
        sleep(5); // Nghỉ 5 giây trước khi copy lượt tiếp theo
    }

    // Dọn dẹp (Lý thuyết vì vòng lặp while(1) chạy mãi)
    mosquitto_loop_stop(global_mosq, true);
    mosquitto_destroy(global_mosq);
    mosquitto_lib_cleanup();
    return NULL;
}

// --- Hàm Main ---
int main() {
    pthread_t t1, t2, t3, t4, t5;//t5

    fd_oled = open("/dev/oled_ssd1306", O_WRONLY);
    if (fd_oled < 0) {
        perror("Lỗi mở thiết bị OLED");
        return -1;
    }

    if (SSD1306_Init(&myOLED) != 0) {
        printf("Lỗi khởi tạo OLED!\n");
        close(fd_oled);
        return -1;
    }

    SSD1306_Fill(&myOLED, SSD1306_COLOR_BLACK);
    SSD1306_UpdateScreen(&myOLED);

    // Tạo 4 luồng
    pthread_create(&t1, NULL, thread_read_pzem, NULL);
    pthread_create(&t2, NULL, thread_display_oled, NULL);
    pthread_create(&t3, NULL, thread_led_blink, NULL);
    pthread_create(&t4, NULL, thread_button_handler, NULL);
    pthread_create(&t5, NULL, thread_mqtt, NULL);

    // Chờ các luồng (Thực tế app chạy vô tận)
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    pthread_join(t4, NULL);
    pthread_join(t5, NULL);

    close(fd_oled);
    return 0;
}