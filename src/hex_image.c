#include "hex_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
#include <wincrypt.h>
#endif

static void set_error(char* error, size_t error_size, const char* message) {
    if (error_size != 0u) {
        snprintf(error, error_size, "%s", message);
    }
}

static bool read_file(const char* path, uint8_t** bytes, size_t* size) {
    FILE* file = fopen(path, "rb");
    long length;
    if (file == NULL || fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) <= 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        if (file != NULL) {
            fclose(file);
        }
        return false;
    }
    *bytes = malloc((size_t)length + 1u);
    if (*bytes == NULL || fread(*bytes, 1u, (size_t)length, file) != (size_t)length) {
        free(*bytes);
        *bytes = NULL;
        fclose(file);
        return false;
    }
    fclose(file);
    (*bytes)[length] = 0u;
    *size = (size_t)length;
    return true;
}

#ifdef _WIN32
static bool decrypt_image(uint8_t* bytes, size_t* size) {
    static const BYTE secret[] = "!!endorfanatec!!";
    static const char provider_name[] =
        "Microsoft Enhanced RSA and AES Cryptographic Provider";
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    HCRYPTKEY key = 0;
    DWORD length;
    bool result = false;
    if (*size > UINT32_MAX || (*size & 15u) != 0u ||
        !CryptAcquireContextA(&provider, "", provider_name, PROV_RSA_AES, 0) ||
        !CryptCreateHash(provider, CALG_MD5, 0, 0, &hash) ||
        !CryptHashData(hash, secret, (DWORD)(sizeof(secret) - 1u), 0) ||
        !CryptDeriveKey(provider, CALG_AES_128, hash, 128u << 16u, &key)) {
        goto done;
    }
    length = (DWORD)*size;
    if (!CryptDecrypt(key, 0, TRUE, 0, bytes, &length)) {
        goto done;
    }
    *size = length;
    bytes[length] = 0u;
    result = true;
done:
    if (key != 0) {
        CryptDestroyKey(key);
    }
    if (hash != 0) {
        CryptDestroyHash(hash);
    }
    if (provider != 0) {
        CryptReleaseContext(provider, 0);
    }
    return result;
}
#else
static bool decrypt_image(uint8_t* bytes, size_t* size) {
    (void)bytes;
    (void)size;
    return false;
}
#endif

static size_t first_record(const uint8_t* bytes, size_t size) {
    size_t index;
    bool line_start = true;
    for (index = 0u; index < size; index++) {
        if (line_start && bytes[index] == ':') {
            return index;
        }
        line_start = bytes[index] == '\n' || bytes[index] == '\r';
    }
    return size;
}

static bool is_text_image(const uint8_t* bytes, size_t size) {
    size_t limit = size < 4096u ? size : 4096u;
    size_t index;
    for (index = 0u; index < limit; index++) {
        uint8_t value = bytes[index];
        if (value != '\r' && value != '\n' && value != '\t' &&
            (value < 0x20u || value > 0x7eu)) {
            return false;
        }
    }
    return true;
}

bool hex_image_open(HexImage* image, const char* path, char* error, size_t error_size) {
    size_t offset;
    memset(image, 0, sizeof(*image));
    if (!read_file(path, &image->bytes, &image->size)) {
        set_error(error, error_size, "cannot read firmware image");
        return false;
    }
    if (!is_text_image(image->bytes, image->size)) {
        if (!decrypt_image(image->bytes, &image->size)) {
            hex_image_close(image);
            set_error(error, error_size, "cannot decrypt encrypted firmware image");
            return false;
        }
        image->encrypted = true;
    }
    offset = first_record(image->bytes, image->size);
    if (offset == image->size) {
        hex_image_close(image);
        set_error(error, error_size, "firmware image contains no Intel HEX records");
        return false;
    }
    if (offset != 0u) {
        memmove(image->bytes, image->bytes + offset, image->size - offset);
        image->size -= offset;
        image->bytes[image->size] = 0u;
    }
    return true;
}

void hex_image_close(HexImage* image) {
    free(image->bytes);
    image->bytes = NULL;
    image->size = 0u;
    image->encrypted = false;
}

static int hex_digit(uint8_t value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

static bool hex_byte(const uint8_t* text, uint8_t* value) {
    int high = hex_digit(text[0]);
    int low = hex_digit(text[1]);
    if (high < 0 || low < 0) {
        return false;
    }
    *value = (uint8_t)((high << 4u) | low);
    return true;
}

static bool parse_record(const uint8_t* line, size_t length, uint8_t* data,
                         uint8_t* count, uint16_t* address, uint8_t* type) {
    uint8_t bytes[260];
    uint16_t sum = 0u;
    uint16_t index;
    if (length < 11u || line[0] != ':' || !hex_byte(line + 1u, count) ||
        length < 11u + (size_t)*count * 2u) {
        return false;
    }
    for (index = 0u; index < (uint16_t)*count + 5u; index++) {
        if (!hex_byte(line + 1u + index * 2u, &bytes[index])) {
            return false;
        }
        sum += bytes[index];
    }
    if ((sum & 0xffu) != 0u) {
        return false;
    }
    *address = (uint16_t)(((uint16_t)bytes[1] << 8u) | bytes[2]);
    *type = bytes[3];
    memcpy(data, bytes + 4u, *count);
    return true;
}

bool hex_image_load_program(const HexImage* image, Dspic33* cpu, char* error,
                            size_t error_size) {
    size_t position = 0u;
    size_t line_number = 1u;
    uint32_t upper = 0u;
    bool eof = false;
    while (position < image->size) {
        size_t end = position;
        uint8_t data[255];
        uint8_t count;
        uint8_t type;
        uint16_t address;
        uint16_t index;
        while (end < image->size && image->bytes[end] != '\r' &&
               image->bytes[end] != '\n') {
            end++;
        }
        if (end != position && !parse_record(image->bytes + position, end - position,
                                             data, &count, &address, &type)) {
            if (error_size != 0u) {
                snprintf(error, error_size,
                         "Intel HEX record %zu is invalid near %.24s", line_number,
                         image->bytes + position);
            }
            return false;
        }
        if (end != position && type == 0u) {
            uint32_t absolute = upper + address;
            if ((absolute & 3u) != 0u || (count & 3u) != 0u) {
                set_error(error, error_size, "Intel HEX program data is misaligned");
                return false;
            }
            for (index = 0u; index < count; index += 4u) {
                uint32_t storage = absolute + index;
                uint32_t program_address = storage / 2u;
                uint32_t word = (uint32_t)data[index] |
                                ((uint32_t)data[index + 1u] << 8u) |
                                ((uint32_t)data[index + 2u] << 16u);
                if (program_address < DSPIC33_PROGRAM_LIMIT &&
                    !dspic33_load_program_word(cpu, program_address, word)) {
                    set_error(error, error_size, "Intel HEX program data is invalid");
                    return false;
                }
                if (program_address >= DSPIC33_CONFIGURATION_BASE &&
                    program_address <
                        DSPIC33_CONFIGURATION_BASE + DSPIC33_CONFIGURATION_SIZE &&
                    !dspic33_load_configuration_word(cpu, program_address, word)) {
                    set_error(error, error_size,
                              "Intel HEX configuration data is invalid");
                    return false;
                }
            }
        } else if (end != position && type == 1u) {
            eof = true;
            break;
        } else if (end != position && type == 4u && count == 2u) {
            upper = ((uint32_t)data[0] << 24u) | ((uint32_t)data[1] << 16u);
        } else if (end != position && type != 0u && type != 2u && type != 3u &&
                   type != 5u) {
            set_error(error, error_size, "Intel HEX record type is unsupported");
            return false;
        }
        position = end;
        while (position < image->size &&
               (image->bytes[position] == '\r' || image->bytes[position] == '\n')) {
            if (image->bytes[position] == '\n') {
                line_number++;
            }
            position++;
        }
    }
    if (!eof) {
        set_error(error, error_size, "Intel HEX image has no end record");
        return false;
    }
    return true;
}
