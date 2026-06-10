# Selective Field Compression in FastCompressor Implementation Plan

> **For Gemini:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement selective field compression in `FastCompressor` to support both compile-time (template-based) and runtime (dynamic) field specification, enabling faster compression and data filtering.

**Architecture:** 
1. Refactor `FastCompressor` to support an optional list of target fields.
2. If fields are specified, `FastCompressor` will extract only those fields from the input JSON.
3. For runtime selection, fields are passed to the constructor or a `set_fields` method.
4. For compile-time selection, a new `StaticFastCompressor<"field1", "field2">` (or similar using `fixed_string` for C++20+) will be introduced.
5. Optimization: When fields are fixed, avoid `glz::generic` and use a more efficient extraction method.

**Tech Stack:** C++26, glaze, Catch2

---

### Task 1: Add dynamic field selection to FastCompressor

**Files:**
- Modify: `include/yase-json/fast_compress.hpp`
- Test: `test/test_fast_compress_selective.cpp`

**Step 1: Write the failing test for dynamic field selection**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "yase-json/fast_compress.hpp"
#include "yase-json/decompress.hpp"

TEST_CASE("FastCompressor dynamic field selection", "[fast_compress]") {
    yase_json::FastCompressor compressor;
    compressor.set_fields({"name", "age"});
    
    std::string json = R"({"name": "John", "age": 30, "city": "New York"})";
    auto compressed = compressor.compress(json);
    
    yase_json::Decompressor decompressor;
    auto decompressed = decompressor.decompress(compressed);
    
    // City should be missing
    CHECK(decompressed.contains("\"name\":\"John\""));
    CHECK(decompressed.contains("\"age\":30"));
    CHECK_FALSE(decompressed.contains("\"city\""));
}
```

**Step 2: Run test to verify it fails**

Run: `cmake -B build -DENABLE_TEST=ON && cmake --build build --target all_test`
Expected: Compilation error (no `set_fields` member).

**Step 3: Implement `set_fields` and update `compress` logic**

```cpp
// In FastCompressor class
public:
  auto set_fields(std::vector<std::string> fields) -> void {
    reset();
    target_fields_ = std::move(fields);
    is_selective_ = !target_fields_.empty();
  }

private:
  std::vector<std::string> target_fields_{};
  bool is_selective_{false};

// In compress() implementation, before memory_.add_value(data)
// If is_selective_, filter the object
if (is_selective_ && data.is_object()) {
    auto const& full_obj = data.get<glz::generic::object_t>();
    glz::generic::object_t filtered_obj;
    for (auto const& field : target_fields_) {
        if (auto it = full_obj.find(field); it != full_obj.end()) {
            filtered_obj[field] = it->second;
        }
    }
    data = std::move(filtered_obj);
}
```

**Step 4: Run test to verify it passes**

Run: `cmake --build build --target all_test`
Expected: PASS

**Step 5: Commit**

```bash
git add include/yase-json/fast_compress.hpp test/test_fast_compress_selective.cpp
git commit -m "feat: add dynamic field selection to FastCompressor"
```

---

### Task 2: Implement StaticFastCompressor for compile-time field selection

**Files:**
- Modify: `include/yase-json/fast_compress.hpp`
- Test: `test/test_fast_compress_static.cpp`

**Step 1: Write the failing test for static field selection**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "yase-json/fast_compress.hpp"

TEST_CASE("StaticFastCompressor compile-time field selection", "[fast_compress]") {
    yase_json::StaticFastCompressor<"name", "age"> compressor;
    
    std::string json = R"({"name": "John", "age": 30, "city": "New York"})";
    auto compressed = compressor.compress(json);
    
    // Verification...
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build --target all_test`
Expected: Compilation error (`StaticFastCompressor` not defined).

**Step 3: Implement `StaticFastCompressor` using C++20/26 string templates**

```cpp
template <size_t N>
struct fixed_string {
    char buf[N];
    constexpr fixed_string(char const* s) { std::copy_n(s, N, buf); }
    operator char const*() const { return buf; }
};

template <fixed_string... Fields>
class StaticFastCompressor : public FastCompressor {
public:
    StaticFastCompressor() {
        set_fields({Fields.buf...});
    }
};
```

**Step 4: Run test to verify it passes**

Run: `cmake --build build --target all_test`
Expected: PASS

**Step 5: Commit**

```bash
git commit -m "feat: add StaticFastCompressor for compile-time field selection"
```

---

### Task 3: Optimization - Direct field extraction (Bonus/Refinement)

**Files:**
- Modify: `include/yase-json/fast_compress.hpp`

**Step 1: Implement optimized extraction for fixed fields**
Instead of parsing the whole JSON into `glz::generic`, use `glz::read_json` with a struct or specific glaze features to extract only the required fields.
