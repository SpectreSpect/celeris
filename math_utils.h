#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <iostream>
#include <vector>
#include <array>

#include <cstdint>
#include <type_traits>
#include <sstream>
#include <iomanip>

#if __has_include(<bit>) && (__cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L))
  #include <bit>
  #define MATHUTILS_HAS_STD_BIT 1
#else
  #define MATHUTILS_HAS_STD_BIT 0
#endif

#if defined(_MSC_VER)
  #include <intrin.h>
#endif

namespace math_utils {
    static constexpr uint32_t BITS = 21;
    static constexpr uint64_t MASK = (uint64_t(1) << BITS) - 1;
    static constexpr int64_t OFFSET = int64_t(1) << (BITS - 1);

    static glm::vec3 bary_coords(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& p) {
        glm::vec3 v0 = b - a;
        glm::vec3 v1 = c - a;
        glm::vec3 v2 = p - a;

        float d00 = glm::dot(v0, v0);
        float d01 = glm::dot(v0, v1);
        float d11 = glm::dot(v1, v1);
        float d20 = glm::dot(v2, v0);
        float d21 = glm::dot(v2, v1);

        float denom = d00 * d11 - d01 * d01;
        if (std::abs(denom) < 1e-8f) return glm::vec3(-1.0f);

        float v = (d11 * d20 - d01 * d21) / denom;
        float w = (d00 * d21 - d01 * d20) / denom;
        float u = 1.0f - v - w;
        return glm::vec3(u, v, w);
    }

    static int floor_div(int a, int b) {
        int q = a / b;
        int r = a % b;
        if (r < 0) --q;
        return q;
    }

    static int floor_mod(int a, int b) {
        int r = a % b;
        if (r < 0) r += b;
        return r;
    }

    static uint32_t div_up_u32(uint32_t a, uint32_t b) { 
        return (a + b - 1u) / b; 
    }

    static double ms_now() {
        using clock = std::chrono::high_resolution_clock;
        static auto t0 = clock::now();
        auto t = clock::now();
        return std::chrono::duration<double, std::milli>(t - t0).count();
    }

    static uint64_t pack_key(int32_t cx, int32_t cy, int32_t cz) {
        static_assert(BITS > 0 && 3 * BITS <= 64);

        if (cx < -OFFSET || cx > OFFSET - 1 ||
            cy < -OFFSET || cy > OFFSET - 1 ||
            cz < -OFFSET || cz > OFFSET - 1) {
            throw std::out_of_range("chunk coord out of packable range");
        }

        auto enc = [](int32_t c) -> uint64_t {
            uint64_t u = static_cast<uint64_t>(static_cast<int64_t>(c) + OFFSET);
            return u & MASK;
        };

        uint64_t ux = enc(cx);
        uint64_t uy = enc(cy);
        uint64_t uz = enc(cz);

        return (ux << (BITS * 2)) | (uy << BITS) | uz;
    }

    static glm::ivec3 unpack_key(uint64_t key) {
        static_assert(BITS > 0 && 3 * BITS <= 64);

        auto dec = [](uint64_t u) -> int32_t {
            // u в диапазоне [0, 2^BITS - 1]
            return static_cast<int32_t>(static_cast<int64_t>(u) - OFFSET);
        };

        uint64_t ux = (key >> (BITS * 2)) & MASK;
        uint64_t uy = (key >> BITS)       & MASK;
        uint64_t uz =  key                & MASK;

        return { dec(ux), dec(uy), dec(uz) };
    }

    static uint32_t next_pow2_u32(uint32_t x) {
    if (x <= 1u) return 1u;
#if MATHUTILS_HAS_STD_BIT
    return static_cast<uint32_t>(std::bit_ceil(x));
#else
    // classic bit-hack
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
#endif
    }

    static inline uint32_t log2_floor_u32(uint32_t x) {
    #if MATHUTILS_HAS_STD_BIT
        return std::bit_width(x) - 1u;
    #elif defined(_MSC_VER)
        unsigned long idx = 0;
        _BitScanReverse(&idx, x);
        return static_cast<uint32_t>(idx);
    #else
        return 31u - static_cast<uint32_t>(__builtin_clz(x));
    #endif
    }

    static inline uint32_t log2_ceil_u32(uint32_t x) {
        if (x <= 1u) return 0u;

        uint32_t v = x - 1u; // важно: ceil(log2(x)) = floor(log2(x-1)) + 1

    #if defined(__cpp_lib_bitops) && (__cpp_lib_bitops >= 201907L)
        // C++20: bit_width(n) = floor(log2(n)) + 1 для n>0
        return std::bit_width(v);

    #elif defined(_MSC_VER)
        unsigned long idx;
        _BitScanReverse(&idx, v);      // idx = floor(log2(v))
        return uint32_t(idx + 1u);

    #elif defined(__GNUC__) || defined(__clang__)
        // __builtin_clz(0) UB, но v != 0 потому что x>1
        return 32u - uint32_t(__builtin_clz(v));

    #else
        // Самый простой fallback (до 31 итерации)
        uint32_t r = 0u;
        while (v) { v >>= 1u; ++r; }
        return r;
    #endif
    }

    static inline uint32_t log2_pow2_u32(uint32_t x) {
        return log2_floor_u32(x);
    }
    
    static glm::ivec2 split_u64(uint64_t k) {
        return glm::uvec2(uint32_t(k & 0xFFFFFFFFull), uint32_t(k >> 32));
    }

    static void normalize_plane(glm::vec4& p) {
        float len = glm::length(glm::vec3(p));
        if (len > 0.0f) p /= len;
    }

    static std::array<glm::vec4, 6> extract_frustum_planes(const glm::mat4& viewProj) {
        glm::mat4 t = glm::transpose(viewProj);

        glm::vec4 row0 = t[0];
        glm::vec4 row1 = t[1];
        glm::vec4 row2 = t[2];
        glm::vec4 row3 = t[3];

        std::array<glm::vec4, 6> p;
        p[0] = row3 + row0; // left
        p[1] = row3 - row0; // right
        p[2] = row3 + row1; // bottom
        p[3] = row3 - row1; // top
        p[4] = row3 + row2; // near
        p[5] = row3 - row2; // far

        for (auto& pl : p) normalize_plane(pl);
        return p;
    }

    static inline bool intersects(uint64_t s1, uint64_t len1, uint64_t s2, uint64_t len2) {
        uint64_t e1 = s1 + len1;
        uint64_t e2 = s2 + len2;
        return (s1 < e2) && (s2 < e1);
    }

    template<typename Map>
    struct MapDiff {
        std::vector<typename Map::key_type> only_in_a;
        std::vector<typename Map::key_type> only_in_b;
        // ключ + (value_from_a, value_from_b)
        std::vector<std::pair<typename Map::key_type,
                            std::pair<typename Map::mapped_type, typename Map::mapped_type>>> different;
    };

    template<typename Map>
    static inline MapDiff<Map> diff_maps(const Map& A, const Map& B) {
        MapDiff<Map> out;
        out.only_in_a.reserve( A.size() > B.size() ? (A.size() - B.size()) : 0 );
        out.only_in_b.reserve( B.size() > A.size() ? (B.size() - A.size()) : 0 );

        // проход по A: помечаем только-in-A и различающиеся значения
        for (const auto& kv : A) {
            auto it = B.find(kv.first);
            if (it == B.end()) {
                out.only_in_a.push_back(kv.first);
            } else if (!(kv.second == it->second)) { // использует operator== для значений
                out.different.emplace_back(kv.first, std::make_pair(kv.second, it->second));
            }
        }

        // проход по B: те ключи, которых нет в A
        for (const auto& kv : B) {
            if (A.find(kv.first) == A.end()) out.only_in_b.push_back(kv.first);
        }

        return out;
    }

    static inline std::string get_current_date_time()
    {
        using namespace std::chrono;

        auto now = system_clock::now();
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

        std::time_t t = system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&t);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y_%m_%d_%H_%M_%S");
        oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

        return oss.str();
    }
}