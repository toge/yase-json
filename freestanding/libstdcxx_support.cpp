// freestanding 用 libstdc++ ランタイムサポート (-nostdlib ビルド用)
//
// libstdc++.so / libstdc++.a が供給する out-of-line シンボルのうち、
// yase-json ゲストが実際に参照するものを供給する。
//
// - _Rb_tree_* : libstdc++-v3/src/c++98/tree.cc (SGI STL 由来) の該当関数のみ
// - std::__throw_* : functexcept 相当 (freestanding では abort に落とす)
// - basic_string::_M_replace_cold : 明示的実体化 (libstdc++ の定義をそのまま使用)
//
// 配布条件: libstdc++ 由来部分は GCC Runtime Library Exception 付き GPL/LGPL
// (SGI STL 部分は SGI 自由ライセンス) に従う。元実装: gcc-mirror/gcc。

#include <string>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <bits/stl_tree.h>
#include <bits/hashtable_policy.h>
#include <bits/basic_string.tcc>

#include <cstdlib>

namespace std {

// ---------------------------------------------------------------------------
// libstdc++-v3/src/c++98/tree.cc から必要関数のみ (SGI STL / FSF)
// ---------------------------------------------------------------------------

static _Rb_tree_node_base*
local_Rb_tree_increment(_Rb_tree_node_base* __x) throw ()
{
  if (__x->_M_right != 0)
    {
      __x = __x->_M_right;
      while (__x->_M_left != 0)
        __x = __x->_M_left;
    }
  else
    {
      _Rb_tree_node_base* __y = __x->_M_parent;
      while (__x == __y->_M_right)
        {
          __x = __y;
          __y = __y->_M_parent;
        }
      if (__x->_M_right != __y)
        __x = __y;
    }
  return __x;
}

_Rb_tree_node_base*
_Rb_tree_increment(_Rb_tree_node_base* __x) throw ()
{
  return local_Rb_tree_increment(__x);
}

const _Rb_tree_node_base*
_Rb_tree_increment(const _Rb_tree_node_base* __x) throw ()
{
  return local_Rb_tree_increment(const_cast<_Rb_tree_node_base*>(__x));
}

static _Rb_tree_node_base*
local_Rb_tree_decrement(_Rb_tree_node_base* __x) throw ()
{
  if (__x->_M_color == _S_red
      && __x->_M_parent->_M_parent == __x)
    __x = __x->_M_right;
  else if (__x->_M_left != 0)
    {
      _Rb_tree_node_base* __y = __x->_M_left;
      while (__y->_M_right != 0)
        __y = __y->_M_right;
      __x = __y;
    }
  else
    {
      _Rb_tree_node_base* __y = __x->_M_parent;
      while (__x == __y->_M_left)
        {
          __x = __y;
          __y = __y->_M_parent;
        }
      __x = __y;
    }
  return __x;
}

_Rb_tree_node_base*
_Rb_tree_decrement(_Rb_tree_node_base* __x) throw ()
{
  return local_Rb_tree_decrement(__x);
}

const _Rb_tree_node_base*
_Rb_tree_decrement(const _Rb_tree_node_base* __x) throw ()
{
  return local_Rb_tree_decrement(const_cast<_Rb_tree_node_base*>(__x));
}

static void
local_Rb_tree_rotate_left(_Rb_tree_node_base* const __x,
                          _Rb_tree_node_base*& __root)
{
  _Rb_tree_node_base* const __y = __x->_M_right;

  __x->_M_right = __y->_M_left;
  if (__y->_M_left != 0)
    __y->_M_left->_M_parent = __x;
  __y->_M_parent = __x->_M_parent;

  if (__x == __root)
    __root = __y;
  else if (__x == __x->_M_parent->_M_left)
    __x->_M_parent->_M_left = __y;
  else
    __x->_M_parent->_M_right = __y;
  __y->_M_left = __x;
  __x->_M_parent = __y;
}

static void
local_Rb_tree_rotate_right(_Rb_tree_node_base* const __x,
                           _Rb_tree_node_base*& __root)
{
  _Rb_tree_node_base* const __y = __x->_M_left;

  __x->_M_left = __y->_M_right;
  if (__y->_M_right != 0)
    __y->_M_right->_M_parent = __x;
  __y->_M_parent = __x->_M_parent;

  if (__x == __root)
    __root = __y;
  else if (__x == __x->_M_parent->_M_right)
    __x->_M_parent->_M_right = __y;
  else
    __x->_M_parent->_M_left = __y;
  __y->_M_right = __x;
  __x->_M_parent = __y;
}

void
_Rb_tree_insert_and_rebalance(const bool __insert_left,
                              _Rb_tree_node_base* __x,
                              _Rb_tree_node_base* __p,
                              _Rb_tree_node_base& __header) throw ()
{
  _Rb_tree_node_base*& __root = __header._M_parent;

  __x->_M_parent = __p;
  __x->_M_left = 0;
  __x->_M_right = 0;
  __x->_M_color = _S_red;

  if (__insert_left)
    {
      __p->_M_left = __x; // also makes leftmost = __x when __p == &__header

      if (__p == &__header)
        {
          __header._M_parent = __x;
          __header._M_right = __x;
        }
      else if (__p == __header._M_left)
        __header._M_left = __x; // maintain leftmost pointing to min node
    }
  else
    {
      __p->_M_right = __x;

      if (__p == __header._M_right)
        __header._M_right = __x; // maintain rightmost pointing to max node
    }

  while (__x != __root
         && __x->_M_parent->_M_color == _S_red)
    {
      _Rb_tree_node_base* const __xpp = __x->_M_parent->_M_parent;

      if (__x->_M_parent == __xpp->_M_left)
        {
          _Rb_tree_node_base* const __y = __xpp->_M_right;
          if (__y && __y->_M_color == _S_red)
            {
              __x->_M_parent->_M_color = _S_black;
              __y->_M_color = _S_black;
              __xpp->_M_color = _S_red;
              __x = __xpp;
            }
          else
            {
              if (__x == __x->_M_parent->_M_right)
                {
                  __x = __x->_M_parent;
                  local_Rb_tree_rotate_left(__x, __root);
                }
              __x->_M_parent->_M_color = _S_black;
              __xpp->_M_color = _S_red;
              local_Rb_tree_rotate_right(__xpp, __root);
            }
        }
      else
        {
          _Rb_tree_node_base* const __y = __xpp->_M_left;
          if (__y && __y->_M_color == _S_red)
            {
              __x->_M_parent->_M_color = _S_black;
              __y->_M_color = _S_black;
              __xpp->_M_color = _S_red;
              __x = __xpp;
            }
          else
            {
              if (__x == __x->_M_parent->_M_left)
                {
                  __x = __x->_M_parent;
                  local_Rb_tree_rotate_right(__x, __root);
                }
              __x->_M_parent->_M_color = _S_black;
              __xpp->_M_color = _S_red;
              local_Rb_tree_rotate_left(__xpp, __root);
            }
        }
    }
  __root->_M_color = _S_black;
}

// ---------------------------------------------------------------------------
// functexcept 相当 — 例外なし環境では abort に落とす (到達不能なはず)
// ---------------------------------------------------------------------------

__attribute__((__noreturn__)) void __throw_logic_error(char const*) { std::abort(); }
__attribute__((__noreturn__)) void __throw_length_error(char const*) { std::abort(); }
__attribute__((__noreturn__)) void __throw_out_of_range(char const*) { std::abort(); }
__attribute__((__noreturn__)) void __throw_invalid_argument(char const*) { std::abort(); }
__attribute__((__noreturn__)) void __throw_runtime_error(char const*) { std::abort(); }
__attribute__((__noreturn__)) void __throw_out_of_range_fmt(char const*, ...) { std::abort(); }
__attribute__((__noreturn__)) void __throw_bad_alloc() { std::abort(); }
__attribute__((__noreturn__)) void __throw_bad_array_new_length() { std::abort(); }

// ---------------------------------------------------------------------------
// unordered_map の rehash ポリシー (素数列は自前生成 — 具体的な素数値は
// unordered_map の正しさに影響しない)
// ---------------------------------------------------------------------------

namespace __detail {

static constexpr auto k_prime_table = [] {
  auto table = std::array<unsigned long, 36>{};
  auto v = 11UL;
  for (auto& slot : table) {
    auto candidate = v | 1;
    auto is_prime = [](unsigned long const n) {
      for (auto d = 3UL; d * d <= n; d += 2) {
        if (n % d == 0) {
          return false;
        }
      }
      return true;
    };
    while (!is_prime(candidate)) {
      candidate += 2;
    }
    slot = candidate;
    v = candidate * 2;
  }
  return table;
}();

std::size_t
_Prime_rehash_policy::_M_next_bkt(std::size_t __n) const
{
  auto const it = std::lower_bound(k_prime_table.begin(), k_prime_table.end(), __n);
  auto const bkt = it == k_prime_table.end() ? k_prime_table.back() : *it;
  _M_next_resize = static_cast<std::size_t>(static_cast<double>(bkt) * _M_max_load_factor);
  return bkt;
}

std::pair<bool, std::size_t>
_Prime_rehash_policy::
_M_need_rehash(std::size_t __n_bkt, std::size_t __n_elt, std::size_t __n_ins) const
{
  if (__n_elt + __n_ins > _M_next_resize)
    {
      double __min_bkts
        = std::max<std::size_t>(__n_elt + __n_ins, _M_next_resize ? 0 : 11)
        / static_cast<double>(_M_max_load_factor);
      if (__min_bkts >= __n_bkt)
        return { true,
          _M_next_bkt(std::max<std::size_t>(static_cast<std::size_t>(__builtin_floor(__min_bkts)) + 1,
                                            __n_bkt * 2)) };

      _M_next_resize
        = static_cast<std::size_t>(static_cast<double>(__n_bkt) * _M_max_load_factor);
      return { false, 0 };
    }
  return { false, 0 };
}

} // namespace __detail

// ---------------------------------------------------------------------------
// std::_Hash_bytes — MurmurHash64A (値は libstdc++ と同一でなくてよい:
// ハッシュ品質のみが性能に影響し、正しさには影響しない)
// ---------------------------------------------------------------------------

std::size_t _Hash_bytes(void const* ptr, std::size_t const len, std::size_t const seed) {
  auto const* data = static_cast<unsigned char const*>(ptr);
  auto const m = UINT64_C(0xc6a4a7935bd1e995);
  auto const r = 47;
  auto h = seed ^ (len * m);
  auto read8 = [&](std::size_t const i) {
    uint64_t v = 0;
    for (auto b = 0; b < 8; ++b) {
      v |= static_cast<uint64_t>(data[i + b]) << (8 * b);
    }
    return v;
  };
  auto const blocks = len / 8;
  for (std::size_t i = 0; i < blocks; ++i) {
    auto k = read8(i * 8);
    k *= m;
    k ^= k >> r;
    k *= m;
    h ^= k;
    h *= m;
  }
  if (len % 8 != 0) {
    uint64_t tail = 0;
    for (std::size_t i = blocks * 8; i < len; ++i) {
      tail |= static_cast<uint64_t>(data[i]) << (8 * (i - blocks * 8));
    }
    h ^= tail;
    h *= m;
  }
  h ^= h >> r;
  h *= m;
  h ^= h >> r;
  return h;
}

} // namespace std

// ---------------------------------------------------------------------------
// basic_string<char>::_M_replace_cold — libstdc++ の明示的実体化
// ---------------------------------------------------------------------------

template void
std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>::
  _M_replace_cold(char*, std::size_t, char const*, std::size_t, std::size_t);

// ---------------------------------------------------------------------------
// libc / C++ABI stub
// ---------------------------------------------------------------------------

extern "C" {

int __cxa_thread_atexit(void (*)(void*), void*, void*) {
  return 0;
}

[[noreturn]] void __assert_fail(char const*, char const*, unsigned, char const*) {
  std::abort();
}

void* memchr(void const* s, int const c, size_t const n) {
  auto const* p = static_cast<unsigned char const*>(s);
  for (size_t i = 0; i < n; ++i) {
    if (p[i] == static_cast<unsigned char>(c)) {
      return const_cast<void*>(static_cast<void const*>(p + i));
    }
  }
  return nullptr;
}

int abs(int const v) {
  return v < 0 ? -v : v;
}

// compiler-rt __multi3 相当 (128bit 乗算の下位128bit)
// 注意: 128bit 演算そのもので実装すると __multi3 再帰呼び出しに戻るため、
// 32/64bit 演算のみで構成する (glaze fast_float が wasm32 で要求する)
__int128 __multi3(__int128 const a, __int128 const b) {
  auto const mul64 = [](uint64_t const x, uint64_t const y, uint64_t* const hi) {
    auto const xl = static_cast<uint32_t>(x);
    auto const xh = static_cast<uint32_t>(x >> 32);
    auto const yl = static_cast<uint32_t>(y);
    auto const yh = static_cast<uint32_t>(y >> 32);
    auto const ll = static_cast<uint64_t>(xl) * yl;
    auto const lh = static_cast<uint64_t>(xl) * yh;
    auto const hl = static_cast<uint64_t>(xh) * yl;
    auto const hh = static_cast<uint64_t>(xh) * yh;
    auto const mid = (ll >> 32) + static_cast<uint32_t>(lh) + static_cast<uint32_t>(hl);
    *hi = hh + (lh >> 32) + (hl >> 32) + (mid >> 32);
    return (mid << 32) | static_cast<uint32_t>(ll);
  };
  uint64_t pa[2] = {};
  uint64_t pb[2] = {};
  memcpy(pa, &a, sizeof(pa));
  memcpy(pb, &b, sizeof(pb));
  auto const al = pa[0];
  auto const ah = pa[1];
  auto const bl = pb[0];
  auto const bh = pb[1];
  uint64_t carry = 0;
  auto const lo = mul64(al, bl, &carry);
  uint64_t m1 = 0;
  uint64_t m2 = 0;
  (void)mul64(al, bh, &m1);
  (void)mul64(ah, bl, &m2);
  auto const hi = carry + m1 + m2;
  __int128 result = {};
  uint64_t pr[2] = {lo, hi};
  memcpy(&result, pr, sizeof(pr));
  return result;
}

} // extern "C"
