#pragma once
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

struct TestRegistry
{
    struct Entry { std::string name; std::function<void()> fn; };
    static std::vector<Entry>& all() { static std::vector<Entry> v; return v; }
    static int& checks() { static int c = 0; return c; }
    static int& failures() { static int c = 0; return c; }
};
struct TestReg { TestReg (const char* n, std::function<void()> f) { TestRegistry::all().push_back ({ n, f }); } };

#define TEST(name) static void name(); static TestReg reg_##name (#name, name); static void name()
#define CHECK(cond) do { ++TestRegistry::checks(); if (!(cond)) { ++TestRegistry::failures(); std::printf ("    FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol) do { ++TestRegistry::checks(); const double va_ = (a), vb_ = (b); if (!(std::fabs (va_ - vb_) <= (tol))) { ++TestRegistry::failures(); std::printf ("    FAIL %s:%d  %s ~ %s  (got %.6f vs %.6f, tol %.6f)\n", __FILE__, __LINE__, #a, #b, va_, vb_, (double) (tol)); } } while (0)
