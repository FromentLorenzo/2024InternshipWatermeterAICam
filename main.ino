#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include "Arduino.h"
#include "FS.h"
#include "SD_MMC.h"
#include "esp32-hal-ledc.h"

// Wi-Fi credentials
const char* ssid = "Khu S";
const char* password = "khu@s2022";

// Define GPIO pin numbers for the AI-THINKER camera module
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define LED_GPIO_NUM       4
#define LED_LEDC_CHANNEL 2


WebServer server(80);

// Define cropping coordinates for five different regions of interest
// Each region is defined by its minimum and maximum x and y coordinates
int crop1_x_min = 195, crop1_y_min = 138, crop1_x_max = 255, crop1_y_max = 212;
int crop2_x_min = 273, crop2_y_min = 134, crop2_x_max = 331, crop2_y_max = 212;
int crop3_x_min = 353, crop3_y_min = 133, crop3_x_max = 408, crop3_y_max = 208;
int crop4_x_min = 432, crop4_y_min = 130, crop4_x_max = 487, crop4_y_max = 206;
int crop5_x_min = 513, crop5_y_min = 128, crop5_x_max = 565, crop5_y_max = 204;

// HTML response message to indicate successful image capture
String htmlBuffer = "<html><body><h1>Capture Successful!</h1></body></html>";


void handleRoot() {
  // Send an HTTP response to the client with a simple HTML page
  server.send(200, "text/html", 
              "<button id=\"captureBtn\">Capture Image</button>" // Button to trigger image capture
              "<div id=\"imageContainer\"></div>" // Container to display images
              "<div id=\"clickCoordinates\"></div>" // Container to display click coordinates
              "<script>"
              "document.getElementById('captureBtn').addEventListener('click', function() {" // Add click event listener to the button
              "  var xhr = new XMLHttpRequest();" // Create a new XMLHttpRequest object
              "  xhr.open('POST', '/capture', true);" // Configure it to send a POST request to the '/capture' endpoint
              "  xhr.onreadystatechange = function() {" // Function to handle changes in request state
              "    if (xhr.readyState == 4 && xhr.status == 200) {" // Check if the request is complete and successful
              "      var timestamp = new Date().getTime();" // Generate a timestamp to prevent image caching
              "      var originalImage = '<div>Original Image:<br><img id=\"capturedImage\" src=\"/captured_image.jpg?time=' + timestamp + '\" /></div>';" // HTML for displaying the original captured image
              "      var croppedImages = '';" // Variable to hold HTML for displaying cropped images
              "      for (var i = 1; i <= 5; i++) {" // Loop to generate HTML for each cropped image
              "        croppedImages += '<div>Cropped Image ' + i + ':<br><img src=\"/cropped_image_' + i + '.jpg?time=' + timestamp + '\" /></div>';" // Add HTML for each cropped image
              "      }"
              "      document.getElementById('imageContainer').innerHTML = originalImage + croppedImages;" // Update the image container with the new images
              "      document.getElementById('capturedImage').addEventListener('click', function(e) {" // Add click event listener to the original image
              "        var rect = e.target.getBoundingClientRect();" // Get the position of the image on the screen
              "        var x = e.clientX - rect.left;" // Calculate the x-coordinate of the click
              "        var y = e.clientY - rect.top;" // Calculate the y-coordinate of the click
              "        document.getElementById('clickCoordinates').innerHTML = 'Clicked at: (' + x + ', ' + y + ')';" // Display the coordinates of the click
              "      });"
              "    }"
              "  };"
              "  xhr.send();" // Send the request
              "});"
              "</script>");
}



camera_fb_t* rotate_image_180(camera_fb_t* src_fb) {
    int width = src_fb->width;
    int height = src_fb->height;
    int len = src_fb->len;

    // Allocate memory for the rotated image
    camera_fb_t* rotated_fb = (camera_fb_t*)malloc(sizeof(camera_fb_t));
    if (!rotated_fb) {
        Serial.println("Failed to allocate memory for rotated image");
        return NULL;
    }

    rotated_fb->buf = (uint8_t*)malloc(len);
    if (!rotated_fb->buf) {
        Serial.println("Failed to allocate memory for rotated image buffer");
        free(rotated_fb);
        return NULL;
    }

    rotated_fb->format = src_fb->format;
    rotated_fb->width = width;
    rotated_fb->height = height;
    rotated_fb->len = len;

    // Perform the 180-degree rotation
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int src_index = (y * width + x) * 3;
            int rotated_index = ((height - y - 1) * width + (width - x - 1)) * 3;

            rotated_fb->buf[rotated_index] = src_fb->buf[src_index];
            rotated_fb->buf[rotated_index + 1] = src_fb->buf[src_index + 1];
            rotated_fb->buf[rotated_index + 2] = src_fb->buf[src_index + 2];
        }
    }

    return rotated_fb;
}


void convert_to_grayscale(uint8_t* src, uint8_t* dest, int width, int height) {
    // Iterate through each pixel in the image
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            // Calculate the index for the current pixel in the source buffer
            int index = (i * width + j) * 3;

            // Extract the RGB components from the source image
            uint8_t r = src[index];     // Red component
            uint8_t g = src[index + 1]; // Green component
            uint8_t b = src[index + 2]; // Blue component

            // Calculate the grayscale value using standard luminance formula
            // The weights 299, 587, and 114 approximate human perception of color brightness
            uint8_t gray = (r * 299 + g * 587 + b * 114) / 1000;

            // Store the grayscale value in the destination buffer
            dest[i * width + j] = gray;
        }
    }
}


void normalize_image(uint8_t* src, float* dest, int width, int height) {
    // Iterate through each pixel in the image
    for (int i = 0; i < width * height; i++) {
        // Normalize pixel value from 0-255 range to 0.0-1.0 range
        dest[i] = src[i] / 255.0f;
    }
}


void createGaussianKernel(float kernel[5][5], float sigma) {
    int size = 5; // Kernel size is 5x5
    float sum = 0.0f; // To store the sum of all elements for normalization
    int halfSize = size / 2; // Half the size of the kernel
    float twoSigmaSquare = 2.0f * sigma * sigma; // Precompute the value for efficiency

    // Loop through each element in the 5x5 kernel
    for (int y = -halfSize; y <= halfSize; y++) {
        for (int x = -halfSize; x <= halfSize; x++) {
            // Calculate the exponent for the Gaussian function
            float exponent = -((x * x + y * y) / twoSigmaSquare);
            // Calculate the kernel value using the Gaussian formula
            kernel[y + halfSize][x + halfSize] = exp(exponent) / (M_PI * twoSigmaSquare);
            // Sum up all kernel values for normalization
            sum += kernel[y + halfSize][x + halfSize];
        }
    }

    // Normalize the kernel so that the sum of all its elements equals 1
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            kernel[y][x] /= sum;
        }
    }
}


void applyGaussianBlur(uint8_t* src, uint8_t* dest, int width, int height, float sigma) {
    float kernel[5][5]; // Array to store the Gaussian kernel
    createGaussianKernel(kernel, sigma); // Create the Gaussian kernel based on sigma

    int halfSize = 2; // Half the size of the 5x5 kernel, which is 2
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.0f; // To accumulate the weighted sum of pixels
            float norm = 0.0f; // To accumulate the normalization factor (not used here)

            // Apply the kernel to the current pixel
            for (int ky = -halfSize; ky <= halfSize; ky++) {
                for (int kx = -halfSize; kx <= halfSize; kx++) {
                    int iy = y + ky; // Y coordinate of the pixel in the kernel
                    int ix = x + kx; // X coordinate of the pixel in the kernel

                    // Check if the kernel pixel is within image bounds
                    if (ix >= 0 && ix < width && iy >= 0 && iy < height) {
                        // Accumulate the weighted sum of pixels
                        sum += src[iy * width + ix] * kernel[ky + halfSize][kx + halfSize];
                        norm += kernel[ky + halfSize][kx + halfSize]; // Accumulate the normalization factor
                    }
                }
            }
            // Set the blurred pixel value
            dest[y * width + x] = (uint8_t)sum;
        }
    }
}



void binarize_image(uint8_t* src, uint8_t* dest, int width, int height) {
    // Find the Otsu threshold
    int hist[256] = {0}; // Histogram array to count pixel occurrences for each grayscale level
    for (int i = 0; i < width * height; i++) {
        hist[src[i]]++; // Populate the histogram with pixel values
    }

    int total = width * height; // Total number of pixels in the image
    float sum = 0; // Sum of all pixel values multiplied by their occurrence
    for (int i = 0; i < 256; i++) {
        sum += i * hist[i]; // Calculate the weighted sum of pixel values
    }

    float sumB = 0; // Weighted sum of background pixel values
    float wB = 0; // Weight of background pixels
    float wF = 0; // Weight of foreground pixels
    float varMax = 0; // Maximum between-class variance
    float threshold = 0; // Otsu threshold

    for (int t = 0; t < 256; t++) {
        wB += hist[t]; // Update weight of background pixels
        if (wB == 0) continue; // Skip if background weight is zero

        wF = total - wB; // Weight of foreground pixels
        if (wF == 0) break; // Break if foreground weight is zero

        sumB += (float)(t * hist[t]); // Update weighted sum of background pixel values

        float mB = sumB / wB; // Mean of background pixels
        float mF = (sum - sumB) / wF; // Mean of foreground pixels

        float varBetween = wB * wF * (mB - mF) * (mB - mF); // Between-class variance

        if (varBetween > varMax) {
            varMax = varBetween; // Update maximum variance
            threshold = t; // Update threshold
        }
    }

    // Binarize the image using the Otsu threshold
    for (int i = 0; i < width * height; i++) {
        dest[i] = (src[i] > threshold) ? 255 : 0; // Apply the threshold to binarize the image
    }
}



// Function to resize an image using nearest neighbor interpolation
void resizeImage(uint8_t* src, uint8_t* dest, int srcWidth, int srcHeight, int destWidth, int destHeight) {
    float xRatio = static_cast<float>(srcWidth) / destWidth; // Calculate the ratio for width scaling
    float yRatio = static_cast<float>(srcHeight) / destHeight; // Calculate the ratio for height scaling
    float px, py; // Coordinates in the source image
    int x, y; // Integer coordinates in the source image

    for (int i = 0; i < destHeight; i++) { // Loop through each row of the destination image
        for (int j = 0; j < destWidth; j++) { // Loop through each column of the destination image
            px = j * xRatio; // Calculate the x-coordinate in the source image
            py = i * yRatio; // Calculate the y-coordinate in the source image
            x = static_cast<int>(px); // Convert floating-point coordinates to integer
            y = static_cast<int>(py); // Convert floating-point coordinates to integer

            int srcIndex = (y * srcWidth + x) * 3; // Calculate index in the source image
            int destIndex = (i * destWidth + j) * 3; // Calculate index in the destination image

            // Copy pixel values from the source image to the destination image
            dest[destIndex] = src[srcIndex];
            dest[destIndex + 1] = src[srcIndex + 1];
            dest[destIndex + 2] = src[srcIndex + 2];
        }
    }
}



camera_fb_t* crop_image(camera_fb_t* src_fb, int x_min, int y_min, int x_max, int y_max) {
    int src_width = src_fb->width; // Width of the source image
    int src_height = src_fb->height; // Height of the source image

    // Ensure crop coordinates are within the bounds of the source image
    if (x_min < 0) x_min = 0;
    if (y_min < 0) y_min = 0;
    if (x_max > src_width) x_max = src_width;
    if (y_max > src_height) y_max = src_height;

    int crop_width = x_max - x_min; // Width of the cropped image
    int crop_height = y_max - y_min; // Height of the cropped image

    // Allocate memory for the cropped image
    camera_fb_t* cropped_fb = (camera_fb_t*)malloc(sizeof(camera_fb_t));
    if (!cropped_fb) {
        Serial.println("Failed to allocate memory for cropped image");
        return NULL;
    }

    cropped_fb->format = PIXFORMAT_RGB888; // Set the image format to RGB888
    cropped_fb->width = crop_width; // Set the width of the cropped image
    cropped_fb->height = crop_height; // Set the height of the cropped image
    cropped_fb->len = crop_width * crop_height * 3; // Calculate the length of the cropped image buffer
    cropped_fb->buf = (uint8_t*)malloc(cropped_fb->len); // Allocate memory for the cropped image buffer

    if (!cropped_fb->buf) {
        Serial.println("Failed to allocate memory for cropped image buffer");
        free(cropped_fb); // Free previously allocated memory
        return NULL;
    }

    // Copy pixel values from the source image to the cropped image
    for (int y = 0; y < crop_height; y++) {
        for (int x = 0; x < crop_width; x++) {
            int src_index = ((y + y_min) * src_width + (x + x_min)) * 3; // Calculate index in the source image
            int cropped_index = (y * crop_width + x) * 3; // Calculate index in the cropped image
            cropped_fb->buf[cropped_index] = src_fb->buf[src_index];
            cropped_fb->buf[cropped_index + 1] = src_fb->buf[src_index + 1];
            cropped_fb->buf[cropped_index + 2] = src_fb->buf[src_index + 2];
        }
    }

    return cropped_fb; // Return the cropped image
}



// Function to convert a JPEG image to RGB format
camera_fb_t* convert_jpeg_to_rgb(camera_fb_t* fb) {
    // Allocate memory for the RGB image buffer
    uint8_t* rgb_buf = (uint8_t*)malloc(fb->width * fb->height * 3);
    if (!rgb_buf) {
        Serial.println("Memory allocation failed for RGB image");
        return NULL;
    }

    // Convert JPEG image to RGB format
    if (!fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb_buf)) {
        Serial.println("JPEG to RGB conversion failed");
        free(rgb_buf); // Free the allocated memory if conversion fails
        return NULL;
    }

    // Allocate memory for the camera framebuffer structure
    camera_fb_t* rgb_fb = (camera_fb_t*)malloc(sizeof(camera_fb_t));
    if (!rgb_fb) {
        Serial.println("Memory allocation failed for camera framebuffer");
        free(rgb_buf); // Free the allocated memory if framebuffer allocation fails
        return NULL;
    }

    // Set up the framebuffer with RGB image data
    rgb_fb->buf = rgb_buf; // Pointer to the RGB image buffer
    rgb_fb->len = fb->width * fb->height * 3; // Length of the RGB image buffer
    rgb_fb->width = fb->width; // Width of the image
    rgb_fb->height = fb->height; // Height of the image
    rgb_fb->format = PIXFORMAT_RGB888; // Set the format to RGB888

    return rgb_fb; // Return the framebuffer containing the RGB image
}



// Function to enable or disable the LED with a specific intensity
void enable_led(bool en)
{
    uint8_t intensity = 4; // Set the LED intensity level

    // Check if the LED should be enabled
    if (en) {
        ledcWrite(LED_LEDC_CHANNEL, intensity); // Set the LED intensity
    } else {
        ledcWrite(LED_LEDC_CHANNEL, 0); // Turn off the LED
    }

    // Print the LED intensity to the serial monitor
    Serial.println("Set LED intensity to");
    Serial.println(intensity);
}




// Function to handle the image capture process
void handleCapture() {
    Serial.println("Capture request received"); // Log the capture request
    enable_led(true); // Turn on the LED to indicate capture process
    vTaskDelay(150 / portTICK_PERIOD_MS); // Delay to allow LED to turn on

    // Capture the image from the camera
    camera_fb_t* fb = esp_camera_fb_get();
    enable_led(false); // Turn off the LED after capture
    if (!fb) {
        Serial.println("Camera capture failed"); // Log failure
        server.send(500, "text/plain", "Camera capture failed"); // Send error response
        return;
    }

    Serial.println("Camera capture successful"); // Log success

    // Convert the captured JPEG image to RGB format for further processing
    camera_fb_t* rgb_fb = convert_jpeg_to_rgb(fb);
    if (!rgb_fb) {
        Serial.println("JPEG to RGB conversion failed"); // Log failure
        server.send(500, "text/plain", "JPEG to RGB conversion failed"); // Send error response
        esp_camera_fb_return(fb); // Return the frame buffer to free memory
        return;
    }

    // Rotate the RGB image by 180 degrees
    camera_fb_t* rotated_fb = rotate_image_180(rgb_fb);
    if (!rotated_fb) {
        Serial.println("Image rotation failed"); // Log failure
        server.send(500, "text/plain", "Image rotation failed"); // Send error response
        free(rgb_fb->buf); // Free memory
        free(rgb_fb); // Free memory
        esp_camera_fb_return(fb); // Return the frame buffer to free memory
        return;
    }

    // Save the rotated image to the SD card
    String path = "/captured_image.jpg";
    fs::FS &fs = SD_MMC;
    File file = fs.open(path.c_str(), FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing"); // Log failure
        server.send(500, "text/plain", "Failed to open file for writing"); // Send error response
        free(rotated_fb->buf); // Free memory
        free(rotated_fb); // Free memory
        esp_camera_fb_return(fb); // Return the frame buffer to free memory
        return;
    }

    size_t jpeg_buf_len = 0;
    uint8_t *jpeg_buf = NULL;
    if (fmt2jpg(rotated_fb->buf, rotated_fb->len, rotated_fb->width, rotated_fb->height, PIXFORMAT_RGB888, 80, &jpeg_buf, &jpeg_buf_len)) {
        file.write(jpeg_buf, jpeg_buf_len); // Write the JPEG image to file
        free(jpeg_buf); // Free memory
    }
    file.close(); // Close the file
    Serial.println("Image saved to SD card"); // Log success

    // Crop and save each region of interest
    camera_fb_t* cropped_fb1 = crop_image(rotated_fb, crop1_x_min, crop1_y_min, crop1_x_max, crop1_y_max);
    saveCroppedImage(cropped_fb1, "/cropped_image_1.jpg");

    camera_fb_t* cropped_fb2 = crop_image(rotated_fb, crop2_x_min, crop2_y_min, crop2_x_max, crop2_y_max);
    saveCroppedImage(cropped_fb2, "/cropped_image_2.jpg");

    camera_fb_t* cropped_fb3 = crop_image(rotated_fb, crop3_x_min, crop3_y_min, crop3_x_max, crop3_y_max);
    saveCroppedImage(cropped_fb3, "/cropped_image_3.jpg");

    camera_fb_t* cropped_fb4 = crop_image(rotated_fb, crop4_x_min, crop4_y_min, crop4_x_max, crop4_y_max);
    saveCroppedImage(cropped_fb4, "/cropped_image_4.jpg");

    // Add the 5th cropped image
    camera_fb_t* cropped_fb5 = crop_image(rotated_fb, crop5_x_min, crop5_y_min, crop5_x_max, crop5_y_max);
    saveCroppedImage(cropped_fb5, "/cropped_image_5.jpg");

    // Free memory allocated for the images
    free(rotated_fb->buf); // Free memory of the rotated image
    free(rotated_fb); // Free memory of the rotated image structure
    free(rgb_fb->buf); // Free memory of the RGB image
    free(rgb_fb); // Free memory of the RGB image structure

    // Return the camera frame buffer to free memory
    esp_camera_fb_return(fb);

    // Send an HTML response to the client
    server.send(200, "text/html", htmlBuffer); // Send success response
    Serial.println("HTML response sent"); // Log response sent
}





// Function to save a cropped image after processing
void saveCroppedImage(camera_fb_t* cropped_fb, const char* path) {
    const int RESIZED_WIDTH = 25; // Width for resizing
    const int RESIZED_HEIGHT = 25; // Height for resizing

    // Resize the cropped image to 25x25 pixels
    uint8_t* resized_buf = (uint8_t*)malloc(RESIZED_WIDTH * RESIZED_HEIGHT * 3);
    if (!resized_buf) {
        Serial.println("Memory allocation failed for resized image"); // Log failure
        free(cropped_fb->buf); // Free allocated memory
        free(cropped_fb); // Free allocated memory
        return;
    }
    resizeImage(cropped_fb->buf, resized_buf, cropped_fb->width, cropped_fb->height, RESIZED_WIDTH, RESIZED_HEIGHT);

    // Convert the resized image to grayscale
    uint8_t* grayscale_buf = (uint8_t*)malloc(RESIZED_WIDTH * RESIZED_HEIGHT);
    if (!grayscale_buf) {
        Serial.println("Memory allocation failed for grayscale image"); // Log failure
        free(resized_buf); // Free allocated memory
        free(cropped_fb->buf); // Free allocated memory
        free(cropped_fb); // Free allocated memory
        return;
    }
    convert_to_grayscale(resized_buf, grayscale_buf, RESIZED_WIDTH, RESIZED_HEIGHT);

    // Apply Gaussian blur to the grayscale image
    uint8_t* blurred_buf = (uint8_t*)malloc(RESIZED_WIDTH * RESIZED_HEIGHT);
    if (!blurred_buf) {
        Serial.println("Memory allocation failed for blurred image"); // Log failure
        free(grayscale_buf); // Free allocated memory
        free(resized_buf); // Free allocated memory
        free(cropped_fb->buf); // Free allocated memory
        free(cropped_fb); // Free allocated memory
        return;
    }
    applyGaussianBlur(grayscale_buf, blurred_buf, RESIZED_WIDTH, RESIZED_HEIGHT, 1.0f); // Apply Gaussian blur with sigma=1.0

    // Binarize the blurred image
    uint8_t* binarized_buf = (uint8_t*)malloc(RESIZED_WIDTH * RESIZED_HEIGHT);
    if (!binarized_buf) {
        Serial.println("Memory allocation failed for binarized image"); // Log failure
        free(blurred_buf); // Free allocated memory
        free(grayscale_buf); // Free allocated memory
        free(resized_buf); // Free allocated memory
        free(cropped_fb->buf); // Free allocated memory
        free(cropped_fb); // Free allocated memory
        return;
    }
    binarize_image(blurred_buf, binarized_buf, RESIZED_WIDTH, RESIZED_HEIGHT);

    // Normalize the binarized image
    float* normalized_buf = (float*)malloc(RESIZED_WIDTH * RESIZED_HEIGHT * sizeof(float));
    if (!normalized_buf) {
        Serial.println("Memory allocation failed for normalized image"); // Log failure
        free(binarized_buf); // Free allocated memory
        free(blurred_buf); // Free allocated memory
        free(grayscale_buf); // Free allocated memory
        free(resized_buf); // Free allocated memory
        free(cropped_fb->buf); // Free allocated memory
        free(cropped_fb); // Free allocated memory
        return;
    }
    normalize_image(binarized_buf, normalized_buf, RESIZED_WIDTH, RESIZED_HEIGHT);

    // Convert the normalized image to RGB format
    uint8_t* rgb_normalized_buf = (uint8_t*)malloc(RESIZED_WIDTH * RESIZED_HEIGHT * 3);
    if (!rgb_normalized_buf) {
        Serial.println("Memory allocation failed for RGB normalized image"); // Log failure
        free(normalized_buf); // Free allocated memory
        free(binarized_buf); // Free allocated memory
        free(blurred_buf); // Free allocated memory
        free(grayscale_buf); // Free allocated memory
        free(resized_buf); // Free allocated memory
        free(cropped_fb->buf); // Free allocated memory
        free(cropped_fb); // Free allocated memory
        return;
    }

    // Fill the RGB buffer with the normalized values
    for (int i = 0; i < RESIZED_WIDTH * RESIZED_HEIGHT; i++) {
        uint8_t value = (uint8_t)(normalized_buf[i] * 255.0f); // Convert normalized value [0.0, 1.0] to [0...255]
        rgb_normalized_buf[i * 3] = value;
        rgb_normalized_buf[i * 3 + 1] = value;
        rgb_normalized_buf[i * 3 + 2] = value;
    }

    // Save the processed image to the SD card
    File file = SD_MMC.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing"); // Log failure
        free(rgb_normalized_buf); // Free allocated memory
        free(normalized_buf); // Free allocated memory
        free(binarized_buf); // Free allocated memory
        free(blurred_buf); // Free allocated memory
        free(grayscale_buf); // Free allocated memory
        free(resized_buf); // Free allocated memory
        free(cropped_fb->buf); // Free allocated memory
        free(cropped_fb); // Free allocated memory
        return;
    }

    size_t jpeg_buf_len = 0;
    uint8_t *jpeg_buf = NULL;
    if (fmt2jpg(rgb_normalized_buf, RESIZED_WIDTH * RESIZED_HEIGHT * 3, RESIZED_WIDTH, RESIZED_HEIGHT, PIXFORMAT_RGB888, 80, &jpeg_buf, &jpeg_buf_len)) {
        file.write(jpeg_buf, jpeg_buf_len); // Write the JPEG image to the file
        free(jpeg_buf); // Free JPEG buffer
    }

    file.close(); // Close the file
    Serial.println("Cropped and processed image saved to SD card"); // Log success

    // Free all allocated memory
    free(rgb_normalized_buf); // Free RGB buffer
    free(normalized_buf); // Free normalized buffer
    free(binarized_buf); // Free binarized buffer
    free(blurred_buf); // Free blurred buffer
    free(grayscale_buf); // Free grayscale buffer
    free(resized_buf); // Free resized buffer
    free(cropped_fb->buf); // Free cropped image buffer
    free(cropped_fb); // Free cropped image structure
}



// Function to handle file read requests and serve files from the SD card
void handleFileRead(String path) {
    Serial.println("handleFileRead: " + path); // Log the file path being requested

    // If the path ends with a '/', append 'index.htm' to serve the default file
    if (path.endsWith("/")) {
        path += "index.htm";
    }

    // Determine the content type based on the file extension
    String contentType = "text/html";
    if (path.endsWith(".jpg")) contentType = "image/jpeg"; // Set content type for JPEG images

    // Check if the requested file exists on the SD card
    if (SD_MMC.exists(path)) {
        File file = SD_MMC.open(path, FILE_READ); // Open the file for reading
        server.streamFile(file, contentType); // Stream the file content to the client
        file.close(); // Close the file after streaming
    } else {
        server.send(404, "text/plain", "File Not Found"); // Send a 404 error if the file does not exist
    }
}


// Setup function to initialize hardware, configure the camera, SD card, and Wi-Fi, and start the HTTP server
void setup() {
    Serial.begin(115200); // Initialize serial communication at 115200 baud
    Serial.setDebugOutput(true); // Enable debug output on serial
    Serial.println(); // Print an empty line for clarity

    // Configure camera settings
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0; // LEDC channel for controlling the camera
    config.ledc_timer = LEDC_TIMER_0; // LEDC timer for the camera
    config.pin_d0 = Y2_GPIO_NUM; // Data pins for the camera
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM; // XCLK pin for the camera
    config.pin_pclk = PCLK_GPIO_NUM; // PCLK pin for the camera
    config.pin_vsync = VSYNC_GPIO_NUM; // VSYNC pin for the camera
    config.pin_href = HREF_GPIO_NUM; // HREF pin for the camera
    config.pin_sccb_sda = SIOD_GPIO_NUM; // SCCB SDA pin for the camera
    config.pin_sccb_scl = SIOC_GPIO_NUM; // SCCB SCL pin for the camera
    config.pin_pwdn = PWDN_GPIO_NUM; // Power down pin for the camera
    config.pin_reset = RESET_GPIO_NUM; // Reset pin for the camera
    config.xclk_freq_hz = 20000000; // XCLK frequency
    config.frame_size = FRAMESIZE_SVGA; // Frame size
    config.pixel_format = PIXFORMAT_JPEG; // Pixel format for the camera
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // Grab mode for the camera
    config.fb_location = CAMERA_FB_IN_PSRAM; // Frame buffer location
    config.jpeg_quality = 12; // JPEG quality
    config.fb_count = 1; // Number of frame buffers

    // Adjust camera configuration based on PSRAM availability
    if (psramFound()) {
        config.jpeg_quality = 10; // Higher quality with PSRAM
        config.fb_count = 2; // Use two frame buffers with PSRAM
        config.grab_mode = CAMERA_GRAB_LATEST; // Grab the latest frame
    } else {
        config.fb_location = CAMERA_FB_IN_DRAM; // Use DRAM if PSRAM is not available
    }

    // Initialize the camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err); // Print error if initialization fails
        return;
    }

    // Initialize SD card
    if (!SD_MMC.begin("/sdcard", true)) {
        Serial.println("SD Card Mount Failed"); // Print error if SD card mounting fails
        return;
    }
    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD Card attached"); // Print error if no SD card is detected
        return;
    }

    // Configure LED
    ledcSetup(LED_LEDC_CHANNEL, 5000, 8); // Setup LEDC with a frequency of 5000 Hz and 8-bit resolution
    ledcAttachPin(LED_GPIO_NUM, LED_LEDC_CHANNEL); // Attach LED pin to the LEDC channel

    // Initialize Wi-Fi
    WiFi.begin(ssid, password); // Start Wi-Fi connection with provided SSID and password
    while (WiFi.status() != WL_CONNECTED) {
        delay(500); // Wait for Wi-Fi connection
        Serial.print("."); // Print a dot to indicate progress
    }
    Serial.println(""); // Print a new line when connected
    Serial.println("WiFi connected"); // Print confirmation of successful Wi-Fi connection
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP()); // Print the assigned IP address

    // Set up HTTP server routes
    server.on("/", HTTP_GET, handleRoot); // Handle root path
    server.on("/capture", HTTP_POST, handleCapture); // Handle capture requests
    server.onNotFound([]() {
        handleFileRead(server.uri()); // Handle 404 errors by attempting to read the requested file
    });

    server.begin(); // Start the HTTP server
    Serial.println("HTTP server started"); // Print confirmation of server startup
}


void loop() {
  server.handleClient();
}