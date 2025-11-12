#include "unity.h"
#include "unity_fixture.h"

#include "script_helpers.h"

#include "file_system.h"

static const char *TEST_KEY = "test_key";
static const char *TEST_VALUE = "test_value";
static const char *TEST_NEW_KEY = "TEST_NEW_KEY";
static const char *TEST_UPDATED_VALUE = "test_updated_value";
static const char *TEST_INVALID_KEY = "INVALID";

// Test group setup
TEST_GROUP(file_system);
TEST_SETUP(file_system)
{
  file_system_write_string(TEST_KEY, TEST_VALUE);
}

TEST_TEAR_DOWN(file_system)
{
  file_system_clear(TEST_KEY);
  file_system_clear(TEST_NEW_KEY);
}

TEST(file_system, setup)
{
  stop_all_tasks();
}

/** @brief Test reading a string with a null key
 *  @test Expected: Returns NULL
 */
TEST(file_system, read_string_null_key)
{
  char *result = file_system_read_string(NULL);
  TEST_ASSERT_NULL(result);
}

/** @brief Test reading a string with a key that does not exist
 *  @test Expected: Returns NULL
 */
TEST(file_system, read_string_key_does_not_exist)
{
  char *result = file_system_read_string(TEST_INVALID_KEY);
  TEST_ASSERT_NULL(result);
}

/** @brief Test reading a string with a valid key
 *  @test Expected: Returns the correct value
 */
TEST(file_system, read_string_returns_valid_value)
{
  char *result = file_system_read_string(TEST_KEY);
  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING(TEST_VALUE, result);
}

/** @brief Test clearing a string with a null key
 *  @test Expected: No operation performed
 */
TEST(file_system, read_clear_null_key)
{
  file_system_clear(NULL);
}

/** @brief Test clearing a string with a key that does not exist
 *  @test Expected: No operation performed
 */
TEST(file_system, read_clear_key_does_not_exist)
{
  file_system_clear(TEST_INVALID_KEY);

  char *result = file_system_read_string(TEST_INVALID_KEY);
  TEST_ASSERT_NULL(result);
}

/** @brief Test clearing a string with a key that exists
 *  @test Expected: Key is cleared and subsequent read returns NULL
 */
TEST(file_system, read_clear_key_exists)
{
  file_system_clear(TEST_KEY);

  char *result = file_system_read_string(TEST_KEY);
  TEST_ASSERT_NULL(result);
}

/** @brief Test writing a string with a null key
 *  @test Expected: No operation performed
 */
TEST(file_system, write_string_null_key)
{
  file_system_write_string(NULL, TEST_VALUE);
  TEST_ASSERT_NULL(file_system_read_string(NULL));
}

/** @brief Test writing a string with a null value
 *  @test Expected: No operation performed. Existing value remains unchanged.
 */
TEST(file_system, write_string_null_value)
{
  file_system_write_string(TEST_KEY, NULL);
  
  char *result = file_system_read_string(TEST_KEY);
  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING(TEST_VALUE, result);
}

/** @brief Test writing a string with a key that does not exist
 *  @test Expected: Key is created with the correct value
 */
TEST(file_system, write_string_key_does_not_exist)
{
  file_system_write_string(TEST_NEW_KEY, TEST_VALUE);

  char *result = file_system_read_string(TEST_NEW_KEY);
  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING(TEST_VALUE, result);
}

/** @brief Test writing a string with a key that exists
 *  @test Expected: Existing key is updated with the new value
 */
TEST(file_system, write_string_key_exists)
{
  file_system_write_string(TEST_KEY, TEST_UPDATED_VALUE);

  char *result = file_system_read_string(TEST_KEY);
  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING(TEST_UPDATED_VALUE, result);
}

TEST_GROUP_RUNNER(file_system)
{
  RUN_TEST_CASE(file_system, setup);
  RUN_TEST_CASE(file_system, read_string_null_key);
  RUN_TEST_CASE(file_system, read_string_key_does_not_exist);
  RUN_TEST_CASE(file_system, read_string_returns_valid_value);
  RUN_TEST_CASE(file_system, read_clear_null_key);
  RUN_TEST_CASE(file_system, read_clear_key_does_not_exist);
  RUN_TEST_CASE(file_system, read_clear_key_exists);
  RUN_TEST_CASE(file_system, write_string_null_key);
  RUN_TEST_CASE(file_system, write_string_null_value);
  RUN_TEST_CASE(file_system, write_string_key_does_not_exist);
  RUN_TEST_CASE(file_system, write_string_key_exists);
}
