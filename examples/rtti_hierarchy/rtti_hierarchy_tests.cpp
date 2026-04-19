#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "rtti_hierarchy.h"

#include <memory>

// ------------------------------------------------------------
// Test Types
// ------------------------------------------------------------

// Non-polymorphic
struct PlainStruct
{
    int x{0};
};

// Polymorphic but no inheritance
struct PolyBase
{
    virtual ~PolyBase() = default;
};

// Single inheritance
struct SingleDerived : public PolyBase
{
};

// Multiple inheritance
struct Left
{
    virtual ~Left() = default;
};

struct Right
{
    virtual ~Right() = default;
};

struct MultiDerived : public Left, public Right
{
};

// Virtual inheritance (diamond)
struct DiamondBase
{
    virtual ~DiamondBase() = default;
};

struct DiamondLeft : virtual public DiamondBase
{
};

struct DiamondRight : virtual public DiamondBase
{
};

struct DiamondDerived : public DiamondLeft, public DiamondRight
{
};

// ------------------------------------------------------------
// supports_rtti tests
// ------------------------------------------------------------

TEST_CASE("supports_rtti returns false for nullptr", "[supports_rtti]")
{
    REQUIRE_FALSE(rtti_hierarchy::supports_rtti(nullptr));
}

TEST_CASE("supports_rtti returns false for non-polymorphic type", "[supports_rtti]")
{
    PlainStruct obj;
    REQUIRE_FALSE(rtti_hierarchy::supports_rtti(&obj));
}

TEST_CASE("supports_rtti returns true for polymorphic type", "[supports_rtti]")
{
    PolyBase obj;
    REQUIRE(rtti_hierarchy::supports_rtti(&obj));
}

// ------------------------------------------------------------
// has_inheritance tests
// ------------------------------------------------------------

TEST_CASE("has_inheritance returns false for nullptr", "[has_inheritance]")
{
    REQUIRE_FALSE(rtti_hierarchy::has_inheritance(nullptr));
}

TEST_CASE("has_inheritance returns false for polymorphic type without bases", "[has_inheritance]")
{
    PolyBase obj;
    REQUIRE_FALSE(rtti_hierarchy::has_inheritance(&obj));
}

TEST_CASE("has_inheritance returns true for single inheritance", "[has_inheritance]")
{
    SingleDerived obj;
    REQUIRE(rtti_hierarchy::has_inheritance(&obj));
}

TEST_CASE("has_inheritance returns true for multiple inheritance", "[has_inheritance]")
{
    MultiDerived obj;
    REQUIRE(rtti_hierarchy::has_inheritance(&obj));
}

TEST_CASE("has_inheritance returns true for virtual inheritance", "[has_inheritance]")
{
    DiamondDerived obj;
    REQUIRE(rtti_hierarchy::has_inheritance(&obj));
}

// ------------------------------------------------------------
// get_hierarchy tests
// ------------------------------------------------------------

TEST_CASE("get_hierarchy returns empty for nullptr", "[get_hierarchy]")
{
    auto result = rtti_hierarchy::get_hierarchy(nullptr);
    REQUIRE(result.empty());
}

TEST_CASE("get_hierarchy returns empty for non-polymorphic type", "[get_hierarchy]")
{
    PlainStruct obj;
    auto result = rtti_hierarchy::get_hierarchy(&obj);
    REQUIRE(result.empty());
}

TEST_CASE("get_hierarchy returns at least one type for polymorphic object", "[get_hierarchy]")
{
    PolyBase obj;
    auto result = rtti_hierarchy::get_hierarchy(&obj);
    REQUIRE_FALSE(result.empty());
}

TEST_CASE("get_hierarchy handles single inheritance", "[get_hierarchy]")
{
    SingleDerived obj;
    auto result = rtti_hierarchy::get_hierarchy(&obj);
    REQUIRE_FALSE(result.empty());
}

TEST_CASE("get_hierarchy handles multiple inheritance", "[get_hierarchy]")
{
    MultiDerived obj;
    auto result = rtti_hierarchy::get_hierarchy(&obj);
    REQUIRE_FALSE(result.empty());
}

TEST_CASE("get_hierarchy handles diamond inheritance", "[get_hierarchy]")
{
    DiamondDerived obj;
    auto result = rtti_hierarchy::get_hierarchy(&obj);
    REQUIRE_FALSE(result.empty());
}

// ------------------------------------------------------------
// Stability / repeated calls
// ------------------------------------------------------------

TEST_CASE("repeated calls are stable", "[stability]")
{
    SingleDerived obj;

    auto r1 = rtti_hierarchy::get_hierarchy(&obj);
    auto r2 = rtti_hierarchy::get_hierarchy(&obj);

    REQUIRE(r1.size() == r2.size());
}

// ------------------------------------------------------------
// Heap allocation path
// ------------------------------------------------------------

TEST_CASE("heap allocated polymorphic object", "[heap]")
{
    auto ptr = std::make_unique<SingleDerived>();

    REQUIRE(rtti_hierarchy::supports_rtti(ptr.get()));
    REQUIRE(rtti_hierarchy::has_inheritance(ptr.get()));

    auto result = rtti_hierarchy::get_hierarchy(ptr.get());
    REQUIRE_FALSE(result.empty());
}

// ------------------------------------------------------------
// Base pointer to derived object
// ------------------------------------------------------------

TEST_CASE("base pointer referencing derived object", "[polymorphism]")
{
    std::unique_ptr<PolyBase> ptr = std::make_unique<SingleDerived>();

    REQUIRE(rtti_hierarchy::supports_rtti(ptr.get()));
    REQUIRE(rtti_hierarchy::has_inheritance(ptr.get()));

    auto result = rtti_hierarchy::get_hierarchy(ptr.get());
    REQUIRE_FALSE(result.empty());
}

// ============================================================
// OPTIONAL: unsafe / fuzz-style tests (DISABLED BY DEFAULT)
// ============================================================
//
// These tests are intentionally NOT part of the unit suite
// because they operate on undefined or non-object memory.
//
// Enable ONLY in a dedicated fuzz or sanitizer build.
//

#if 0

TEST_CASE("random pointer is handled safely")
{
    int x = 42;
    void* p = &x;

    REQUIRE_FALSE(rtti_hierarchy::supports_rtti(p));
}

TEST_CASE("stack garbage pointer is handled safely")
{
    void* p = reinterpret_cast<void*>(0x1234);

    REQUIRE_FALSE(rtti_hierarchy::supports_rtti(p));
}

#endif
