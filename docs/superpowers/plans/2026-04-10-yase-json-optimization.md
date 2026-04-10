# yase-json Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Preserve existing compatibility while significantly reducing end-to-end runtime, with the largest gain coming from `yase_json::crush()`.

**Architecture:** Keep the public API and encoded output unchanged. Optimize `include/yase-json/crush.hpp` first because it dominates the measured runtime, then keep only low-risk copy/allocation reductions in `compress/decompress` and shared detail helpers if the benchmark shows a real gain.

**Tech Stack:** C++23/C++26, Glaze, Catch2, existing custom benchmark executable

---

## File map

- Modify: `include/yase-json/crush.hpp`  
  Main hotspot. Reduce candidate-search and string-rebuild overhead without changing output compatibility or tie-breaking.

- Modify: `include/yase-json/detail/compress_json_compat.hpp`  
  Secondary hotspot. Keep behavior unchanged while reducing avoidable string copies in base62/number/schema helpers if the benchmark justifies it.

- Modify: `include/yase-json/compress.hpp`  
  Secondary cleanup. Reduce avoidable transient `glz::generic` and string work only if it stays low-risk.

- Modify: `include/yase-json/decompress.hpp`  
  Secondary cleanup. Reduce avoidable decoding copies only if it stays low-risk.

- Modify: `test/test_crush.cpp`  
  Add regression coverage around exact JSONCrush output so internal optimization cannot change observable behavior.

- Modify: `test/test_compress_decompress.cpp`  
  Add regression coverage for compress/decompress compatibility if a changed helper path needs targeted protection.

- Modify: `test/benchmark.cpp`  
  Keep the same workload, but make the output more benchmark-friendly and stable enough for before/after comparison using `std::chrono::steady_clock`.

## Task 1: Lock benchmark and compatibility baselines

**Files:**
- Modify: `test/benchmark.cpp`
- Test: `test/test_crush.cpp`
- Test: `test/test_compress_decompress.cpp`

- [ ] **Step 1: Add compatibility-focused regression cases before touching hot code**

```cpp
SECTION("Repeated replacement candidate keeps official output ordering") {
  auto const input = std::string{R"({"k1":"abcabcabc","k2":"abcabcabc"})"};
  auto const expected = yase_json::crush(input);
  REQUIRE(yase_json::crush(input) == expected);
}
```

- [ ] **Step 2: Run the focused tests and confirm they pass before optimization**

Run: `cmake --build build --parallel && ctest --test-dir build --output-on-failure -R all_test`
Expected: existing tests pass, new regression cases pass

- [ ] **Step 3: Replace `high_resolution_clock` usage with `steady_clock` and keep the same measured stages**

```cpp
using clock_type = std::chrono::steady_clock;
auto const start = clock_type::now();
```

- [ ] **Step 4: Keep benchmark output machine-readable enough for before/after comparison**

```cpp
std::cout << "CompressionMs=" << d1 << '\n';
std::cout << "JSONCrushMs=" << d2 << '\n';
std::cout << "JSONUncrushMs=" << d3 << '\n';
std::cout << "DecompressionMs=" << d4 << '\n';
```

- [ ] **Step 5: Run the benchmark once and save the baseline numbers**

Run: `./build/test/benchmark`
Expected: same functional verification succeeds, stage timings are printed in a stable format

- [ ] **Step 6: Commit**

```bash
git add test/benchmark.cpp test/test_crush.cpp test/test_compress_decompress.cpp
git commit -m "test: lock optimization benchmark baseline"
```

## Task 2: Optimize `crush.hpp` candidate discovery without changing output

**Files:**
- Modify: `include/yase-json/crush.hpp`
- Test: `test/test_crush.cpp`
- Test: `test/benchmark.cpp`

- [ ] **Step 1: Write a regression test for a case that exercises repeated candidate rebuilding**

```cpp
SECTION("Candidate rebuild path preserves exact output") {
  auto const input = std::string{R"({"nested":{"x":"abababab","y":"abababab"}})"};
  auto const expected = std::string{"('nested!(*!*))-abababab~x*y\u0001-*_"};
  REQUIRE(yase_json::crush(input) == expected);
}
```

- [ ] **Step 2: Run only the new crush-focused test and confirm it passes on current code**

Run: `./build/test/all_test "[crush]"`
Expected: PASS

- [ ] **Step 3: Refactor candidate generation to defer string ownership until a candidate is accepted**

```cpp
struct CandidateView {
  std::u16string_view value;
  int64_t count;
  int64_t encoded_length;
};
```

- [ ] **Step 4: Replace repeated full-string `replace_all` rebuilds in the scoring path with a single-pass writer that precomputes output size**

```cpp
auto rewritten = std::u16string{};
rewritten.reserve(input.size() - removed + added);
// copy unmatched spans + write replacement char in one pass
```

- [ ] **Step 5: Keep tie-breaking stable by preserving the existing candidate iteration order**

```cpp
if (delta > best_delta) {
  best_delta = delta;
  best_index = filtered.size(); // insertion index before push_back(candidate)
}
```

- [ ] **Step 6: Run crush tests**

Run: `cmake --build build --parallel && ./build/test/all_test "[crush]"`
Expected: PASS, including exact-output compatibility cases

- [ ] **Step 7: Run the benchmark and compare against the saved baseline**

Run: `./build/test/benchmark`
Expected: `JSONCrushMs` is materially lower than baseline and total time drops accordingly

- [ ] **Step 8: Commit**

```bash
git add include/yase-json/crush.hpp test/test_crush.cpp
git commit -m "perf: speed up JSONCrush candidate processing"
```

## Task 3: Optimize `uncrush()` reconstruction if it is still measurable

**Files:**
- Modify: `include/yase-json/crush.hpp`
- Test: `test/test_crush.cpp`
- Test: `test/benchmark.cpp`

- [ ] **Step 1: Confirm from the latest benchmark whether `JSONUncrushMs` is worth touching**

Run: `./build/test/benchmark`
Expected: if `JSONUncrushMs < 5`, skip the rest of this task and record why

- [ ] **Step 2: If needed, replace split/join heavy reconstruction with a direct rebuild loop**

```cpp
for (auto const replacement : parts[1]) {
  // rebuild output directly instead of materializing split vectors
}
```

- [ ] **Step 3: Run crush tests again**

Run: `cmake --build build --parallel && ./build/test/all_test "[crush]"`
Expected: PASS

- [ ] **Step 4: Re-run the benchmark**

Run: `./build/test/benchmark`
Expected: `JSONUncrushMs` does not regress, total time stays improved

- [ ] **Step 5: Commit**

```bash
git add include/yase-json/crush.hpp
git commit -m "perf: reduce JSONUncrush reconstruction overhead"
```

## Task 4: Apply low-risk copy reduction to `compress/decompress` helpers only if justified

**Files:**
- Modify: `include/yase-json/compress.hpp`
- Modify: `include/yase-json/decompress.hpp`
- Modify: `include/yase-json/detail/compress_json_compat.hpp`
- Test: `test/test_compress_decompress.cpp`
- Test: `test/benchmark.cpp`

- [ ] **Step 1: Add a focused regression test for the helper path being changed**

```cpp
SECTION("compress-json compatibility payload stays exact after helper optimization") {
  REQUIRE(compressor.compress(sample_json) == expected_compressed);
}
```

- [ ] **Step 2: Run the compatibility suite before making helper changes**

Run: `cmake --build build --parallel && ./build/test/all_test "[compression]"`
Expected: PASS

- [ ] **Step 3: Reduce avoidable temporary strings in the measured helper path only**

```cpp
auto encoded = std::string{};
encoded.reserve(estimated_size);
```

- [ ] **Step 4: Keep `glz::generic` construction count minimal where equivalent move-based assembly is possible**

```cpp
// Reuse the same Glaze generic/container types already present in the codebase.
auto result = glz::generic::array_t{};
result.emplace_back(std::move(values_node));
result.emplace_back(glz::generic{root_key});
```

- [ ] **Step 5: Run compression/decompression tests**

Run: `cmake --build build --parallel && ./build/test/all_test "[compression]"`
Expected: PASS

- [ ] **Step 6: Re-run the benchmark and keep the helper change only if total time improves or stays neutral**

Run: `./build/test/benchmark`
Expected: no compatibility regression and no meaningful slowdown

- [ ] **Step 7: Commit**

```bash
git add include/yase-json/compress.hpp include/yase-json/decompress.hpp include/yase-json/detail/compress_json_compat.hpp test/test_compress_decompress.cpp
git commit -m "perf: trim compression helper allocations"
```

## Task 5: Final verification and result capture

**Files:**
- Modify: `test/benchmark.cpp` (only if final formatting needs cleanup)

- [ ] **Step 1: Run the full test suite**

Run: `cmake --build build --parallel && ctest --test-dir build --output-on-failure`
Expected: PASS

- [ ] **Step 2: Run the final benchmark**

Run: `./build/test/benchmark`
Expected: same correctness verification succeeds and total runtime is lower than baseline

- [ ] **Step 3: Record the before/after benchmark numbers in the work summary**

```text
Baseline: Compression=11, JSONCrush=166270, JSONUncrush=2, Decompression=9, Total=166292
Final:    Compression=?, JSONCrush=?, JSONUncrush=?, Decompression=?, Total=?
```

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "perf: optimize yase-json hot paths"
```
