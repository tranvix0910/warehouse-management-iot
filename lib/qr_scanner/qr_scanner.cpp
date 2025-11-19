#include "qr_scanner.h"

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
    config.pixel_format = PIXFORMAT_GRAYSCALE; // keep grayscale for QR
    config.frame_size = FRAMESIZE_QQVGA; // 160x120 - smaller frame to reduce memory usage
    config.jpeg_quality = 12;
    config.fb_count = 1;                // single buffer to reduce memory usage
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // more conservative grab mode
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
        s->set_contrast(s, 2);         // -2 to 2
        s->set_saturation(s, 0);       // -2 to 2
        s->set_sharpness(s, 2);        // -2 to 2
        s->set_denoise(s, 0);          // 0/1 (off for sharper edges)
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
    
    if (quirc_resize(q, 160, 120) < 0) {
        Serial.println("Failed to resize quirc");
        return;
    }
    
    Serial.println("QR Code Scanner Ready!");
}

bool scanQRCode(String &qrData) {
    qrData = "";
    
    if (!q) {
        return false;
    }
    
    // Add delay to prevent stack overflow
    delay(100);
    
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return false;
    }
    
    // Copy raw grayscale into quirc buffer (fixed size 160x120)
    int qw = 0, qh = 0;
    uint8_t *image = quirc_begin(q, &qw, &qh);
    if (!image) {
        Serial.println("Failed to get image buffer");
        esp_camera_fb_return(fb);
        return false;
    }
    
    // Optimized memory copy with bounds checking
    size_t copy_len = (size_t)qw * (size_t)qh;
    if (copy_len > fb->len) {
        copy_len = fb->len;
    }
    memcpy(image, fb->buf, copy_len);
    
    // Process the image
    quirc_end(q);
    
    // Check for QR codes
    int num_codes = quirc_count(q);
    if (num_codes > 0) {
        totalQRDetected++;
        Serial.printf("Found %d QR code(s) - Total detected: %d\n", num_codes, totalQRDetected);
        
        bool qrDecoded = false;
        for (int i = 0; i < num_codes && i < 3; i++) { // Limit to 3 codes max
            quirc_extract(q, i, &code);
            
            // Simplified QR code validation
            if (code.size >= 21 && code.size <= 177) {
                // Single decode attempt per code
                err = quirc_decode(&code, &data);
                if (err == QUIRC_SUCCESS) {
                    totalQRDecoded++;
                    qrData = String((const char*)data.payload, data.payload_len);
                    qrData.trim();
                    Serial.printf("✅ QR Code: %s (Success rate: %d/%d = %.1f%%)\n", 
                               qrData.c_str(), totalQRDecoded, totalQRDetected, 
                               (float)totalQRDecoded/totalQRDetected*100);
                    qrDecoded = true;
                    break;
                } else {
                    Serial.printf("QR decode error: %d\n", err);
                }
            }
            
            if (qrDecoded) break;
        }

        // Simplified second pass: basic threshold for low-contrast QR
        if (!qrDecoded && num_codes > 0) {
            int qw2 = 0, qh2 = 0;
            uint8_t *th_img = quirc_begin(q, &qw2, &qh2);
            if (th_img && qw2 > 0 && qh2 > 0) {
                size_t n = (size_t)qw2 * (size_t)qh2;
                if (n <= fb->len) {
                    // Simple threshold at 128
                    uint8_t thr = 128;
                    for (size_t k = 0; k < n; k++) {
                        uint8_t px = ((uint8_t*)fb->buf)[k];
                        th_img[k] = (px > thr) ? 255 : 0;
                    }
                    quirc_end(q);

                    int num2 = quirc_count(q);
                    for (int i = 0; i < num2 && i < 2; i++) { // Limit to 2 codes max
                        quirc_extract(q, i, &code);
                        if (code.size >= 21 && code.size <= 177) {
                            err = quirc_decode(&code, &data);
                            if (err == QUIRC_SUCCESS) {
                                totalQRDecoded++;
                                qrData = String((const char*)data.payload, data.payload_len);
                                qrData.trim();
                                Serial.printf("✅ (TH) QR: %s (Success: %d/%d)\n", qrData.c_str(), totalQRDecoded, totalQRDetected);
                                qrDecoded = true;
                                break;
                            } else {
                                Serial.printf("QR decode error (TH): %d\n", err);
                            }
                        }
                    }
                } else {
                    quirc_end(q);
                }
            } else {
                quirc_end(q);
            }
        }
    } else {
        static unsigned long lastNo = 0;
        if (millis() - lastNo > 5000) { // Reduced frequency
            Serial.println("No QR code found");
            lastNo = millis();
        }
    }
    
    // Return camera frame buffer
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
