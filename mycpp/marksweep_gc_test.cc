
#include <unordered_set>

#include "mycpp/runtime.h"
#include "vendor/greatest.h"

TEST hash_value_test() {
  int dummy1 = 41;
  int dummy2 = 42;
  void *p1 = static_cast<void *>(&dummy1);
  void *p2 = static_cast<void *>(&dummy2);

  std::unordered_set<void *> set;
  set.insert(p1);

  log("&dummy1 = %p", p1);
  log("&dummy2 = %p", p2);
  std::hash<void *> hasher;
  log("std::hash<void*>(&dummy1) = %x", hasher(p1));
  log("std::hash<void*>(&dummy2) = %x", hasher(p2));

  PASS();
}

TEST string_collection_test() {
  Str *test_str = StrFromC("foo");
  {
    // NOTE(Jesse): This causes a crash when this gets compiled against the
    // cheney collector w/ GC_EVERY_ALLOC.  I did verify it doesn't crash with
    // the marksweep allocator but didn't want to figure out how to tell the
    // build system to not compile these tests against the cheney collector
    //
    /* ASSERT(are_equal(test_str, StrFromC("foo"))); */

    StackRoots _roots({&test_str});

    ASSERT(are_equal(test_str, StrFromC("foo")));

    gHeap.Collect();

    ASSERT(are_equal(test_str, StrFromC("foo")));
  }

  // NOTE(Jesse): Technically UB.  If the collector hits between when the roots
  // go out of scope in the above block we'll get a UAF here.  ASAN should
  // detect this but we currently have no way of programatically verifying that
  // ASAN detects bugs.  AFAIK asan is not 100% reliable, so maybe that's a
  // path fraught with peril anyhow.
  //
  /* ASSERT(are_equal(test_str, StrFromC("foo"))); */

  gHeap.Collect();

  // NOTE(Jesse): ASAN detects UAF here when I tested by toggling this on
  //
  // ASSERT(are_equal(test_str, StrFromC("foo")));

  PASS();
}

TEST list_collection_test() {
  {
    Str *test_str0 = nullptr;
    Str *test_str1 = nullptr;
    List<Str *> *test_list = nullptr;

    StackRoots _roots({&test_str0, &test_str1, &test_list});

    test_str0 = StrFromC("foo_0");
    test_str1 = StrFromC("foo_1");
    test_list = NewList<Str *>();

    test_list->append(test_str0);
    test_list->append(test_str1);

    // Verify the list looks as we expected
    {
      ASSERT(are_equal(test_list->index_(0), test_str0));
      ASSERT(are_equal(test_list->index_(1), test_str1));

      ASSERT_EQ(test_list->index_(0), test_str0);
      ASSERT_EQ(test_list->index_(1), test_str1);

      ASSERT_EQ(2, len(test_list));
    }

    gHeap.Collect();

    {
      ASSERT(are_equal(test_list->index_(0), test_str0));
      ASSERT(are_equal(test_list->index_(1), test_str1));

      ASSERT_EQ(test_list->index_(0), test_str0);
      ASSERT_EQ(test_list->index_(1), test_str1);
    }

    test_list->pop();
    ASSERT_EQ(1, len(test_list));
  }

  gHeap.Collect();

  PASS();
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
  gHeap.Init(MiB(64));

  PRINT_GC_MODE_STRING();

  GREATEST_MAIN_BEGIN();

  RUN_TEST(hash_value_test);
  RUN_TEST(string_collection_test);
  RUN_TEST(list_collection_test);

  GREATEST_MAIN_END(); /* display results */
  return 0;
}