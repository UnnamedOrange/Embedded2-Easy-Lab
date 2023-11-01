#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

#include <sysctl.h>
#include <uart.h>

extern "C"
{
#include "aes.h"
#include "aes_cbc.h"
#include "dmac.h"
#include "encoding.h"
#include "gcm.h"
#include "sysctl.h"
#include "w25qxx.h"
}

using NvData = std::array<uint8_t, 128>;

static constexpr auto FLASH_ADDRESS = uintptr_t(0x00B00000);
static constexpr auto SPI_INDEX = uint32_t(3);
static constexpr auto AES_KEY = std::array<uint8_t, AES_256>{
    0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c, 0x00, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08,
    0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c, 0x00, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08,
};
static constexpr auto AES_IV = std::array<uint8_t, 16>{
    0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad, 0xde, 0xca, 0xf8, 0x88, 0x67, 0x30, 0x83, 0x08,
};
static constexpr auto CBC_CONTEXT = cbc_context_t{
    .input_key = const_cast<uint8_t*>(AES_KEY.data()),
    .iv = const_cast<uint8_t*>(AES_IV.data()),
};

NvData encrypt(const NvData& src) {
    NvData ret;
    aes_cbc256_hard_encrypt(const_cast<cbc_context_t*>(&CBC_CONTEXT), //
                            const_cast<uint8_t*>(src.data()),         //
                            src.size(),                               //
                            ret.data());
    return ret;
}
NvData decrypt(const NvData& src) {
    NvData ret;
    aes_cbc256_hard_decrypt(const_cast<cbc_context_t*>(&CBC_CONTEXT), //
                            const_cast<uint8_t*>(src.data()),         //
                            src.size(),                               //
                            ret.data());
    return ret;
}

int main() {
    // 初始化 Flash DMA。
    w25qxx_init_dma(SPI_INDEX, 0);
    w25qxx_enable_quad_mode_dma();

    // 读取内容。
    auto nv_data = NvData{};
    w25qxx_read_data_dma(FLASH_ADDRESS, nv_data.data(), nv_data.size(), W25QXX_QUAD_FAST);

    // 解密内容。
    nv_data = decrypt(nv_data);

    // 输出内容。
    for (auto x : nv_data) {
        printf("%d ", x);
    }
    printf("\n");

    // 变换内容。
    std::transform(nv_data.begin(), nv_data.end(), nv_data.begin(), [](uint8_t it) { return it + 1; });

    // 加密内容。
    nv_data = encrypt(nv_data);

    // 写入内容。
    w25qxx_write_data_dma(FLASH_ADDRESS, nv_data.data(), nv_data.size());

    while (true)
        ;
}
