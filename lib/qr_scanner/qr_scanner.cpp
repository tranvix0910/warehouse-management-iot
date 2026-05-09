#include "qr_scanner.h"
#include <string.h>

// Khớp FRAMESIZE_QQVGA — ít tải DMA/cam_task, tránh stack overflow trên cam_task (EV-EOF-OVF)
#define QR_FRAME_W 160
#define QR_FRAME_H 120

// QR code detection variables
struct quirc *q = NULL;
struct quirc_code code;
struct quirc_data data;
quirc_decode_error_t err;

// Statistics for QR detection
int totalQRDetected = 0;
int totalQRDecoded = 0;

void qrScannerInit() {
    Serial.println("Initializing QR Scanner...");
    
    // Camera configuration
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_GRAYSCALE; // grayscale cho quirc
    config.frame_size = FRAMESIZE_QQVGA; // 160x120 — ổn định cam_task hơn QVGA
    config.jpeg_quality = 12;
    config.fb_count = 1;                 // giảm áp lực buffer/DMA (có PSRAM vẫn nên thận trọng)
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM; // Use PSRAM for frame buffer
    
    // Initialize camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }
    
    Serial.println("Camera initialized successfully!");
    
    // Tune camera sensor for better QR detection
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_brightness(s, 0);       // -2 to 2
        s->set_contrast(s, 2);         // cạnh rõ hơn giúp quirc phân tách đen/trắng
        s->set_saturation(s, 0);       // -2 to 2
        s->set_sharpness(s, 2);        // -2 to 2
        s->set_denoise(s, 0);          // tắt làm mềm → cạnh QR sắc hơn
        s->set_gain_ctrl(s, 1);        // Auto gain
        s->set_exposure_ctrl(s, 1);    // Auto exposure
        s->set_ae_level(s, 0);         // -2 to 2
        s->set_whitebal(s, 1);         // Auto white balance
        s->set_awb_gain(s, 1);
        s->set_hmirror(s, 0);
        s->set_vflip(s, 0);
        s->set_lenc(s, 1);             // lens correction
    }

    // Initialize quirc for QR code detection
    q = quirc_new();
    if (!q) {
        Serial.println("Failed to create quirc object");
        return;
    }
    
    if (quirc_resize(q, QR_FRAME_W, QR_FRAME_H) < 0) {
        Serial.println("Failed to resize quirc");
        return;
    }
    
    Serial.println("QR Code Scanner Ready!");
}

/** Ngưỡng Otsu — tự chọn mức đen/trắng theo histogram (QR in/thiếu sáng). */
static uint8_t otsu_threshold(const uint8_t *p, size_t n) {
    static unsigned long hist[256];
    memset(hist, 0, sizeof(hist));
    for (size_t i = 0; i < n; i++) {
        hist[p[i]]++;
    }
    unsigned long total = n;
    double sum = 0;
    for (int i = 0; i < 256; i++) {
        sum += (double)i * hist[i];
    }
    double sumB = 0;
    unsigned long wB = 0;
    double maxVar = 0;
    int bestT = 128;
    for (int t = 0; t < 256; t++) {
        wB += hist[t];
        if (wB == 0) continue;
        unsigned long wF = total - wB;
        if (wF == 0) break;
        sumB += (double)t * hist[t];
        double mB = sumB / wB;
        double mF = (sum - sumB) / wF;
        double var = (double)wB * wF * (mB - mF) * (mB - mF);
        if (var >= maxVar) {
            maxVar = var;
            bestT = t;
        }
    }
    return (uint8_t)bestT;
}

/** Điền buffer quirc: raw (thr=0) hoặc nhị phân hóa (thr>0). */
static void fill_quirc_image(uint8_t *dst, const uint8_t *src, size_t n, uint8_t thr) {
    if (thr == 0) {
        memcpy(dst, src, n);
    } else {
        for (size_t k = 0; k < n; k++) {
            dst[k] = (src[k] > thr) ? 255 : 0;
        }
    }
}

/** Thử decode mọi mã tìm được trong buffer quirc hiện tại. */
static bool try_decode_all(struct quirc *qr, String &qrData, bool log_fail) {
    int ncodes = quirc_count(qr);
    for (int i = 0; i < ncodes && i < 4; i++) {
        quirc_extract(qr, i, &code);
        if (code.size < 21 || code.size > 177) {
            continue;
        }
        err = quirc_decode(&code, &data);
        if (err == QUIRC_SUCCESS) {
            totalQRDecoded++;
            qrData = String((const char *)data.payload, data.payload_len);
            qrData.trim();
            Serial.printf("✅ QR: %s (%d/%d)\n", qrData.c_str(), totalQRDecoded, totalQRDetected);
            return true;
        }
        if (log_fail) {
            static unsigned failCount = 0;
            if ((++failCount % 30) == 1) {
                Serial.printf("QR decode: %s (ảnh khác ngưỡng có thể đọc được)\n", quirc_strerror(err));
            }
        }
    }
    return false;
}

bool scanQRCode(String &qrData) {
    qrData = "";

    if (!q) {
        return false;
    }

    yield(); // bỏ delay(100) để quét nhanh hơn

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return false;
    }

    const size_t expected = (size_t)QR_FRAME_W * (size_t)QR_FRAME_H;
    if (fb->len < expected) {
        Serial.printf("Camera fb too small: %u < %u (cần QVGA grayscale)\n", (unsigned)fb->len, (unsigned)expected);
        esp_camera_fb_return(fb);
        return false;
    }
    const size_t copy_len = expected;
    const uint8_t *src = fb->buf;

    uint8_t otsu = otsu_threshold(src, copy_len);
    // Thứ tự: ảnh gốc (nhanh) → Otsu → các ngưỡng cố định (khi thiếu tương phản / lệch sáng)
    const uint8_t passes[] = {0, otsu, 128, 112, 144, 104, 136};
    int qw = 0, qh = 0;
    bool any_detection = false;

    for (unsigned pi = 0; pi < sizeof(passes); pi++) {
        uint8_t thr = passes[pi];
        uint8_t *image = quirc_begin(q, &qw, &qh);
        if (!image || qw <= 0 || qh <= 0) {
            Serial.println("Failed to get quirc buffer");
            quirc_end(q);
            break;
        }
        fill_quirc_image(image, src, copy_len, thr);
        quirc_end(q);

        int num_codes = quirc_count(q);
        if (num_codes == 0) {
            continue;
        }
        if (!any_detection) {
            totalQRDetected++;
            any_detection = true;
        }
        if (try_decode_all(q, qrData, pi == 0)) {
            esp_camera_fb_return(fb);
            return true;
        }
    }

    if (!any_detection) {
        static unsigned long lastNo = 0;
        if (millis() - lastNo > 5000) {
            Serial.println("No QR code found");
            lastNo = millis();
        }
    }

    esp_camera_fb_return(fb);
    return !qrData.isEmpty();
}

void qrScannerCleanup() {
    if (q) {
        quirc_destroy(q);
        q = NULL;
    }
    Serial.println("QR Scanner cleaned up");
}
