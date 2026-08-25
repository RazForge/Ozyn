/**
 * Ozayn Core Engine — C/C++ Shared Library
 * High-performance ML, vision, system control, and crypto operations
 * Called from Python via ctypes bindings
 */

#ifndef OZAYN_CORE_H
#define OZAYN_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
    #define OZAYN_API __declspec(dllexport)
#else
    #define OZAYN_API __attribute__((visibility("default")))
#endif

/* ==================== Version ==================== */

#define OZAYN_CORE_VERSION "1.0.0"

/**
 * Get core library version string.
 * @return Version string (e.g. "1.0.0")
 */
OZAYN_API const char* ozayn_version(void);

/* ==================== ML Engine ==================== */

/**
 * Load a machine learning model from file.
 * Supports ONNX, TFLite, and custom binary formats.
 * @param path Path to model file
 * @return Model handle (NULL on failure), caller must free with ozayn_ml_free_model
 */
OZAYN_API void* ozayn_ml_load_model(const char* path);

/**
 * Run inference on a loaded model.
 * @param model Model handle from ozayn_ml_load_model
 * @param input Input data array
 * @param input_len Number of elements in input
 * @param output Output buffer (caller-allocated)
 * @param output_len Maximum elements in output buffer
 * @return Number of output elements written, or -1 on error
 */
OZAYN_API int ozayn_ml_predict(void* model, const float* input, int input_len,
                               float* output, int output_len);

/**
 * Free a loaded model.
 * @param model Model handle to free
 */
OZAYN_API void ozayn_ml_free_model(void* model);

/**
 * Get available ML model info.
 * @param buffer Output buffer for JSON model list
 * @param buffer_len Size of output buffer
 * @return Number of bytes written
 */
OZAYN_API int ozayn_ml_list_models(char* buffer, int buffer_len);

/* ==================== Vision ==================== */

/**
 * Capture the current screen and save to file.
 * @param output_path Path to save screenshot (PNG/JPG)
 * @return 0 on success, -1 on failure
 */
OZAYN_API int ozayn_vision_capture_screen(const char* output_path);

/**
 * Detect faces in an image.
 * @param image_path Path to input image
 * @param count Output: number of faces detected
 * @return 0 on success, -1 on failure
 */
OZAYN_API int ozayn_vision_detect_faces(const char* image_path, int* count);

/**
 * Analyze an image and return object/scene information as JSON.
 * @param image_path Path to input image
 * @param result Output buffer for JSON result
 * @param result_len Size of output buffer
 * @return Number of bytes written, or -1 on error
 */
OZAYN_API int ozayn_vision_analyze_image(const char* image_path, char* result, int result_len);

/**
 * Detect hand gestures from camera frame.
 * @param camera_id Camera device index (0 for default)
 * @param result Output buffer for JSON result
 * @param result_len Size of output buffer
 * @return Number of bytes written, or -1 on error
 */
OZAYN_API int ozayn_vision_detect_gesture(int camera_id, char* result, int result_len);

/**
 * Initialize vision subsystem (OpenCV, etc.).
 * @return 0 on success, -1 on failure
 */
OZAYN_API int ozayn_vision_init(void);

/**
 * Shutdown vision subsystem and free resources.
 */
OZAYN_API void ozayn_vision_shutdown(void);

/* ==================== System Control ==================== */

/**
 * Get current CPU usage percentage.
 * @return CPU usage 0-100, or -1 on error
 */
OZAYN_API int ozayn_sys_get_cpu_usage(void);

/**
 * Get current memory usage.
 * @param used_bytes Output: bytes used
 * @param total_bytes Output: total bytes available
 * @return 0 on success, -1 on failure
 */
OZAYN_API int ozayn_sys_get_memory_usage(long* used_bytes, long* total_bytes);

/**
 * Get disk usage for a path.
 * @param path Mount point or directory path
 * @param used_bytes Output: bytes used
 * @param total_bytes Output: total bytes
 * @return 0 on success, -1 on failure
 */
OZAYN_API int ozayn_sys_get_disk_usage(const char* path, long* used_bytes, long* total_bytes);

/**
 * List running processes as JSON.
 * @param sort_by Sort field: "cpu", "mem", "pid"
 * @param limit Max processes to return
 * @param buffer Output buffer for JSON
 * @param buffer_len Size of output buffer
 * @return Number of bytes written, or -1 on error
 */
OZAYN_API int ozayn_sys_list_processes(const char* sort_by, int limit,
                                       char* buffer, int buffer_len);

/**
 * Run a shell command with timeout.
 * @param cmd Command string to execute
 * @param timeout_seconds Max execution time
 * @param output Output buffer for command output
 * @param output_len Size of output buffer
 * @return Exit code, or -1 on error/timeout
 */
OZAYN_API int ozayn_sys_run_command(const char* cmd, int timeout_seconds,
                                    char* output, int output_len);

/**
 * Get system uptime in seconds.
 * @return Uptime in seconds, or -1 on error
 */
OZAYN_API long ozayn_sys_get_uptime(void);

/**
 * Get hostname.
 * @param buffer Output buffer
 * @param buffer_len Size of buffer
 * @return 0 on success, -1 on failure
 */
OZAYN_API int ozayn_sys_get_hostname(char* buffer, int buffer_len);

/**
 * List files in a directory as JSON.
 * @param path Directory path
 * @param show_hidden Include hidden files (1=yes, 0=no)
 * @param buffer Output buffer for JSON
 * @param buffer_len Size of output buffer
 * @return Number of bytes written, or -1 on error
 */
OZAYN_API int ozayn_sys_list_files(const char* path, int show_hidden,
                                   char* buffer, int buffer_len);

/* ==================== Crypto ==================== */

/**
 * Compute SHA-256 hash of input.
 * @param input Input string
 * @param output Output hex string buffer (64 chars + null)
 * @param output_len Size of output buffer (must be >= 65)
 * @return 0 on success, -1 on failure
 */
OZAYN_API int ozayn_crypto_hash(const char* input, char* output, int output_len);

/**
 * Encrypt data with AES-256-CBC.
 * @param data Plaintext data
 * @param key 32-byte encryption key
 * @param output Output buffer (base64 encoded)
 * @param output_len Size of output buffer
 * @return Number of bytes written, or -1 on error
 */
OZAYN_API int ozayn_crypto_encrypt(const char* data, const char* key,
                                   char* output, int output_len);

/**
 * Decrypt data with AES-256-CBC.
 * @param data Base64 encoded ciphertext
 * @param key 32-byte decryption key
 * @param output Output buffer (plaintext)
 * @param output_len Size of output buffer
 * @return Number of bytes written, or -1 on error
 */
OZAYN_API int ozayn_crypto_decrypt(const char* data, const char* key,
                                   char* output, int output_len);

/**
 * Generate a random hex string.
 * @param length Number of random bytes (output will be 2*length hex chars)
 * @param output Output buffer
 * @param output_len Size of output buffer
 * @return 0 on success, -1 on failure
 */
OZAYN_API int ozayn_crypto_random(int length, char* output, int output_len);

/* ==================== Utility ==================== */

/**
 * Get current timestamp as ISO 8601 string.
 * @param buffer Output buffer
 * @param buffer_len Size of buffer (minimum 26)
 * @return 0 on success, -1 on failure
 */
OZAYN_API int ozayn_util_timestamp(char* buffer, int buffer_len);

/**
 * Get system information as JSON.
 * @param buffer Output buffer
 * @param buffer_len Size of buffer
 * @return Number of bytes written, or -1 on error
 */
OZAYN_API int ozayn_util_system_info(char* buffer, int buffer_len);

#ifdef __cplusplus
}
#endif

#endif /* OZAYN_CORE_H */
