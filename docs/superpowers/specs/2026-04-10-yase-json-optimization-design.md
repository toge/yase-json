# yase-json optimization design

## Problem

The current end-to-end benchmark spends almost all of its time in `yase_json::crush()`.
Using the existing benchmark input in `test/benchmark.cpp`, the baseline is:

- Compression: 11 ms
- JSONCrush: 166270 ms
- JSONUncrush: 2 ms
- Decompression: 9 ms
- Total: 166292 ms

The goal is to preserve public API compatibility and encoded output compatibility while reducing total runtime as much as possible.

## Scope

Primary scope:

- `include/yase-json/crush.hpp`

Secondary scope:

- `include/yase-json/compress.hpp`
- `include/yase-json/decompress.hpp`
- `include/yase-json/detail/compress_json_compat.hpp`

Out of scope:

- Public API changes
- Encoded output format changes
- Behavior changes that would break compatibility with official JSONCrush or `compress-json`

## Chosen approach

Use a balanced optimization strategy:

1. Make `crush.hpp` the primary target because it dominates the benchmark.
2. Keep output compatibility and tie-breaking behavior unchanged.
3. Reduce copying and transient allocations in `compress/decompress` and shared detail helpers only where the change is low-risk and measurable.

## Design

### 1. `crush.hpp`

Keep the external functions unchanged:

- `auto crush(std::string_view) -> std::string`
- `auto uncrush(std::string_view) -> std::string`

Internal optimization plan:

1. Reduce eager `std::u16string` materialization during candidate discovery.
2. Avoid repeated whole-string reconstruction where a single-pass write can preserve the same result.
3. Narrow candidate rebuilding work after each replacement so unchanged or unprofitable candidates are not needlessly rebuilt.
4. Preserve the current candidate ordering and best-candidate selection semantics so output remains byte-for-byte compatible with the current implementation and official JSONCrush expectations in tests.

### 2. `compress/decompress`

Apply only low-risk optimizations:

1. Reduce temporary `glz::generic` construction where the same final structure can be built with fewer moves/copies.
2. Reduce unnecessary `std::string` copying in base62 and number/string encoding helpers when it does not change behavior.
3. Leave behavior and output untouched.

### 3. Validation

Compatibility requirements:

1. Existing Catch2 tests must continue to pass.
2. JSONCrush output compatibility tests must remain exact.
3. `compress-json` compatibility tests must remain exact.

Benchmark requirements:

1. Use the existing `test/benchmark.cpp` workflow for before/after comparison.
2. Keep the per-stage timing breakdown so improvements can be attributed.
3. Consider the work successful only if the benchmark shows a real improvement, with the main gain expected in `JSONCrush`.

## Risks and mitigations

### Risk: changing JSONCrush output

Mitigation: keep candidate ordering and tie-breaking behavior unchanged, and validate against existing exact-output tests.

### Risk: optimizing the wrong area

Mitigation: use the existing benchmark as the baseline and compare the same benchmark after each optimization batch.

### Risk: low-value churn in non-dominant code

Mitigation: treat `compress/decompress` changes as secondary and only keep them if they are simple and measurable.
