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

float thresholds[] = {10.0, 30.0, 50.0, 100.0, 200.0}; // Các mức ngưỡng bạn muốn
int threshold_index = 0; // Chỉ số hiện tại trong mảng
float power_threshold = 10.0; // Ngưỡng công suất mặc định (100W)
int is_overloaded = 0;         // Cờ báo quá tải
int fd_oled = -1;
SSD1306_Name myOLED;
// Biến lưu dữ liệu thô để gửi qua MQTT
char mqtt_payload[256];

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
                strncpy(pzem_data, buf, sizeof(pzem_data));
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
    int fd_btn = open("/dev/input/event1", O_RDONLY); // Đảm bảo đúng event1 như bạn đã test
    struct input_event ev;
    int num_thresholds = sizeof(thresholds) / sizeof(thresholds[0]);

    if (fd_btn < 0) {
        perror("Lỗi mở Button device");
        return NULL;
    }

    printf("Luồng Button sẵn sàng (Dùng mảng xoay vòng %d mức)\n", num_thresholds);

    while(1) {
        if (read(fd_btn, &ev, sizeof(struct input_event)) > 0) {
            // Chỉ xử lý khi nhấn xuống (value == 1)
            if (ev.type == EV_KEY && ev.code == KEY_A && ev.value == 1) {
                
                pthread_mutex_lock(&data_lock);
                
                // Tăng chỉ số và xoay vòng bằng toán tử %
                threshold_index = (threshold_index + 1) % num_thresholds;
                power_threshold = thresholds[threshold_index];
                
                pthread_mutex_unlock(&data_lock);
                
                printf("Nút nhấn OK! Chỉ số: %d, Ngưỡng mới: %.0fW\n", 
                        threshold_index, power_threshold);
            }
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
    struct mosquitto *mosq = NULL;
    int rc;

    // 1. Khởi tạo thư viện
    mosquitto_lib_init();

    // Tạo client ID duy nhất
    mosq = mosquitto_new("BBB_Chien_Device_ID_999@@@@", true, NULL);
    if (!mosq) {
        printf("MQTT: Khong the tao instance!\n");
        return NULL;
    }

    // 2. Đăng ký hàm callback xử lý tin nhắn đến
    mosquitto_message_callback_set(mosq, on_message);

    // 3. Thiết lập bảo mật TLS (Phải có file .crt trên BBB)
    // if (mosquitto_tls_set(mosq, "/etc/ssl/certs/ca-certificates.crt", NULL, NULL, NULL, NULL) != MOSQ_ERR_SUCCESS) {
    //     printf("MQTT: Loi thiet lap TLS! Kiem tra file /etc/ssl/certs/ca-certificates.crt\n");
    // }
    //mosquitto_tls_insecure_set(mosq, true);
    // 4. Thiết lập User/Pass
    mosquitto_username_pw_set(mosq, NULL, NULL);

    // 5. Kết nối tới Broker
    printf("MQTT: Dang ket noi den %s...\n", MQTT_HOST);
    rc = mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 60);
    mosquitto_loop_start(mosq);
    if (rc != MOSQ_ERR_SUCCESS) {
        printf("MQTT: Ket noi that bai! Ma loi: %d\n", rc);
        mosquitto_destroy(mosq);
        return NULL;
    }

    // 6. KÍCH HOẠT LUỒNG XỬ LÝ MẠNG NGẦM (Sửa lỗi 4 triệt để)
    //mosquitto_loop_start(mosq);

    // 7. Đăng ký topic nhận cấu hình (pzem/config)
    mosquitto_subscribe(mosq, NULL, "pzem/config", 0);
    printf("MQTT: Da ket noi va dang lang nghe tren topic 'pzem/config'\n");

    sleep(2);
    // 8. Vòng lặp gửi dữ liệu định kỳ
    while(1) {
        pthread_mutex_lock(&data_lock);
        
        // Chỉ gửi khi đã có dữ liệu thực từ PZEM
        if (strcmp(pzem_data, "Loading...") != 0) {
            // Gửi dữ liệu lên topic DATA (pzem/data)
            int pub_rc = mosquitto_publish(mosq, NULL, "pzem/data", strlen(pzem_data), pzem_data, 0, false);
            
            if (pub_rc != MOSQ_ERR_SUCCESS) {
                        printf("MQTT: Gui that bai (Ma: %d). Dang thu ket noi lai...\n", pub_rc);
                        // Nếu lỗi 4, ép thư viện kết nối lại
                        mosquitto_reconnect(mosq);
            }else {
                printf("MQTT: Da gui du lieu len pzem/data\n");
            }
        }
        
        pthread_mutex_unlock(&data_lock);

        // Gửi mỗi 2 giây để tránh spam Cloud và bị block
        sleep(5); 
    }

    // Dọn dẹp (Lý thuyết vì vòng lặp while(1) chạy mãi)
    mosquitto_loop_stop(mosq, true);
    mosquitto_destroy(mosq);
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