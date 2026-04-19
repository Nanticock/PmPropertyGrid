#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "rtti_hierarchy.h"

#include <algorithm>
#include <memory>
#include <typeindex>
#include <typeinfo>

// ============================================================
// Test type hierarchy
//
// All types below are polymorphic (have at least one virtual
// function).  The API contract requires a valid polymorphic
// object — passing a non-polymorphic pointer is undefined
// behaviour and is not tested here.
// ============================================================

// Root: no bases
struct PolyBase
{
    virtual ~PolyBase() = default;
};

// Single inheritance
struct SingleDerived : PolyBase
{
};

// Multiple inheritance: two independent roots
struct LeftBase
{
    virtual ~LeftBase() = default;
};

struct RightBase
{
    virtual ~RightBase() = default;
};

struct MultiDerived : LeftBase, RightBase
{
};

// Diamond (virtual inheritance)
struct DiamondBase
{
    virtual ~DiamondBase() = default;
};

struct DiamondLeft : virtual DiamondBase
{
};

struct DiamondRight : virtual DiamondBase
{
};

struct DiamondDerived : DiamondLeft, DiamondRight
{
};

// Deep chain: A <- B <- C
struct ChainA
{
    virtual ~ChainA() = default;
};

struct ChainB : ChainA
{
};

struct ChainC : ChainB
{
};

// ============================================================
// Helper
// ============================================================

static bool contains(const std::vector<std::type_index>& v, std::type_index ti)
{
    return std::find(v.begin(), v.end(), ti) != v.end();
}

// ============================================================
// supports_rtti
// ============================================================

TEST_CASE("supports_rtti: nullptr returns false", "[supports_rtti]")
{
    REQUIRE_FALSE(rtti_hierarchy::supports_rtti(nullptr));
}

TEST_CASE("supports_rtti: root polymorphic object", "[supports_rtti]")
{
    PolyBase obj;
    REQUIRE(rtti_hierarchy::supports_rtti(&obj));
}

TEST_CASE("supports_rtti: derived object", "[supports_rtti]")
{
    SingleDerived obj;
    REQUIRE(rtti_hierarchy::supports_rtti(&obj));
}

TEST_CASE("supports_rtti: object accessed via base pointer", "[supports_rtti]")
{
    // Dynamic type (SingleDerived) must be detected, not the static type (PolyBase).
    std::unique_ptr<PolyBase> ptr = std::make_unique<SingleDerived>();
    REQUIRE(rtti_hierarchy::supports_rtti(ptr.get()));
}

TEST_CASE("supports_rtti: heap-allocated derived object", "[supports_rtti]")
{
    auto ptr = std::make_unique<DiamondDerived>();
    REQUIRE(rtti_hierarchy::supports_rtti(ptr.get()));
}

// ============================================================
// has_inheritance
// ============================================================

TEST_CASE("has_inheritance: nullptr returns false", "[has_inheritance]")
{
    REQUIRE_FALSE(rtti_hierarchy::has_inheritance(nullptr));
}

TEST_CASE("has_inheritance: root class (no bases) returns false", "[has_inheritance]")
{
    PolyBase obj;
    REQUIRE_FALSE(rtti_hierarchy::has_inheritance(&obj));
}

TEST_CASE("has_inheritance: single inheritance returns true", "[has_inheritance]")
{
    SingleDerived obj;
    REQUIRE(rtti_hierarchy::has_inheritance(&obj));
}

TEST_CASE("has_inheritance: multiple inheritance returns true", "[has_inheritance]")
{
    MultiDerived obj;
    REQUIRE(rtti_hierarchy::has_inheritance(&obj));
}

TEST_CASE("has_inheritance: virtual (diamond) inheritance returns true", "[has_inheritance]")
{
    DiamondDerived obj;
    REQUIRE(rtti_hierarchy::has_inheritance(&obj));
}

TEST_CASE("has_inheritance: deep chain leaf returns true", "[has_inheritance]")
{
    ChainC obj;
    REQUIRE(rtti_hierarchy::has_inheritance(&obj));
}

TEST_CASE("has_inheritance: base pointer to derived reflects dynamic type", "[has_inheritance]")
{
    std::unique_ptr<PolyBase> ptr = std::make_unique<SingleDerived>();
    // ptr.get() is PolyBase* but the dynamic object is SingleDerived.
    REQUIRE(rtti_hierarchy::has_inheritance(ptr.get()));
}

// ============================================================
// get_hierarchy — nullptr
// ============================================================

TEST_CASE("get_hierarchy: nullptr returns empty vector", "[get_hierarchy]")
{
    REQUIRE(rtti_hierarchy::get_hierarchy(nullptr).empty());
}

// ============================================================
// get_hierarchy — content: root class
// ============================================================

TEST_CASE("get_hierarchy: root class contains exactly itself", "[get_hierarchy]")
{
    PolyBase obj;
    auto result = rtti_hierarchy::get_hierarchy(&obj);

    REQUIRE(result.size() == 1u);
    REQUIRE(contains(result, std::type_index(typeid(PolyBase))));
}

// ============================================================
// get_hierarchy — content: single inheritance
// ============================================================

TEST_CASE("get_hierarchy: SingleDerived contains itself and PolyBase", "[get_hierarchy]")
{
    SingleDerived obj;
    auto result = rtti_hierarchy::get_hierarchy(&obj);

    REQUIRE(result.size() == 2u);
    REQUIRE(contains(result, std::type_index(typeid(SingleDerived))));
    REQUIRE(contains(result, std::type_index(typeid(PolyBase))));
}

TEST_CASE("get_hierarchy: derived is listed before its base", "[get_hierarchy]")
{
    // The DFS walk must emit each class before recursing into its bases,
    // so the most-derived type is always first.
    SingleDerived obj;
    auto result = rtti_hierarchy::get_hierarchy(&obj);

    REQUIRE_FALSE(result.empty());
    REQUIRE(result.front() == std::type_index(typeid(SingleDerived)));
}

// ============================================================
// get_hierarchy — content: multiple inheritance
// ============================================================

TEST_CASE("get_hierarchy: MultiDerived contains itself and both bases", "[get_hierarchy]")
{
    MultiDerived obj;
    auto result = rtti_hierarchy::get_hierarchy(&obj);

    REQUIRE(result.size() == 3u);
    REQUIRE(contains(result, std::type_index(typeid(MultiDerived))));
    REQUIRE(contains(result, std::type_index(typeid(LeftBase))));
    REQUIRE(contains(result, std::type_index(typeid(RightBase))));
}

// ============================================================
// get_hierarchy — content: diamond (virtual inheritance)
// ============================================================

TEST_CASE("get_hierarchy: diamond base appears exactly once", "[get_hierarchy]")
{
    DiamondDerived obj;
    auto result = rtti_hierarchy::get_hierarchy(&obj);

    // Expected: DiamondDerived, DiamondLeft, DiamondBase, DiamondRight
    // DiamondBase is reachable via both DiamondLeft and DiamondRight but
    // must appear only once (deduplication via visited set).
    REQUIRE(result.size() == 4u);
    REQUIRE(contains(result, std::type_index(typeid(DiamondDerived))));
    REQUIRE(contains(result, std::type_index(typeid(DiamondLeft))));
    REQUIRE(contains(result, std::type_index(typeid(DiamondRight))));
    REQUIRE(contains(result, std::type_index(typeid(DiamondBase))));
}

// ============================================================
// get_hierarchy — content: deep chain
// ============================================================

TEST_CASE("get_hierarchy: deep chain contains all levels", "[get_hierarchy]")
{
    ChainC obj;
    auto result = rtti_hierarchy::get_hierarchy(&obj);

    REQUIRE(result.size() == 3u);
    REQUIRE(contains(result, std::type_index(typeid(ChainC))));
    REQUIRE(contains(result, std::type_index(typeid(ChainB))));
    REQUIRE(contains(result, std::type_index(typeid(ChainA))));
}

// ============================================================
// get_hierarchy — base pointer to derived (dynamic type wins)
// ============================================================

TEST_CASE("get_hierarchy: base pointer returns hierarchy of dynamic type", "[get_hierarchy]")
{
    std::unique_ptr<PolyBase> ptr = std::make_unique<SingleDerived>();
    auto result = rtti_hierarchy::get_hierarchy(ptr.get());

    // Even though the static type is PolyBase*, the vptr points to
    // SingleDerived's vtable, so we must see SingleDerived's hierarchy.
    REQUIRE(result.size() == 2u);
    REQUIRE(contains(result, std::type_index(typeid(SingleDerived))));
    REQUIRE(contains(result, std::type_index(typeid(PolyBase))));
}

// ============================================================
// Stability
// ============================================================

TEST_CASE("get_hierarchy: repeated calls return identical results", "[stability]")
{
    DiamondDerived obj;
    auto r1 = rtti_hierarchy::get_hierarchy(&obj);
    auto r2 = rtti_hierarchy::get_hierarchy(&obj);

    REQUIRE(r1 == r2);
}
