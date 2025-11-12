#include "unity.h"
#include "unity_fixture.h"

#include "esp_log.h"

#include "generic_task.h"

#define TASK_STACK_SIZE 2048
#define TASK_PRIORITY   5

#define DELAY_TIME_MS 100

static const char *TEST_TASK = "test_task";

static GenericTask *test_task;

/** @struct TestMsg_t
 *  @brief Test message structure for the generic task
 */
typedef struct
{
  int value;
} TestMsg_t;

/** @brief Test task on_init callback
 *  @param self Pointer to the GenericTask instance
 *  @return ESP_OK on success
 */ 
static esp_err_t test_task_on_init(GenericTask *self)
{
  ESP_LOGI(self->name, "test_task_on_init called");
  return ESP_OK;
}

/** @brief Test task on_stop callback
 *  @param self Pointer to the GenericTask instance
 *  @return ESP_OK on success
 */
static esp_err_t test_task_on_stop(GenericTask *self)
{
  ESP_LOGI(self->name, "test_task_on_stop called");
  return ESP_OK;
}

/** @brief Test task on_message callback
 *  @param self Pointer to the GenericTask instance
 *  @param msg Pointer to the message buffer
 *  @param msg_size Size of the message
 */
static void test_task_on_message(GenericTask *self, void *msg, size_t msg_size)
{
  if (msg_size != sizeof(TestMsg_t))
  {
    ESP_LOGE(self->name, "Invalid message size: %u", (unsigned)msg_size);
    return;
  }

  TestMsg_t *test_msg = (TestMsg_t *)msg;
  ESP_LOGI(self->name, "test_task_on_message received value: %d", test_msg->value);
}

/** @brief Test task on_init callback that fails
 *  @param self Pointer to the GenericTask instance
 *  @return ESP_FAIL to simulate failure
 */
static esp_err_t test_task_on_init_fail(GenericTask *self)
{
  ESP_LOGI(self->name, "test_task_on_init (failing) called");
  return ESP_FAIL;
}

/** @brief Test task on_stop callback that fails
 *  @param self Pointer to the GenericTask instance
 *  @return ESP_FAIL to simulate failure
 */
static esp_err_t test_task_on_stop_fail(GenericTask *self)
{
  ESP_LOGI(self->name, "test_task_on_stop (failing) called");
  return ESP_FAIL;
}

// Test group setup
TEST_GROUP(generic_task);
TEST_SETUP(generic_task)
{
  // Setup code here
}

TEST_TEAR_DOWN(generic_task)
{
  // Teardown code here
}

/** @brief Test: Start Generic Task with NULL pointer
 *  @test Expected: generic_task_start returns ESP_ERR_INVALID_ARG
 */
TEST(generic_task, start_null_pointer)
{
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, generic_task_start(NULL, TASK_STACK_SIZE, TASK_PRIORITY));
}

/** @brief Test: Stop Generic Task with NULL pointer
 *  @test Expected: generic_task_stop returns ESP_ERR_INVALID_ARG
 */
TEST(generic_task, stop_null_pointer)
{
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, generic_task_stop(NULL));
}

/** @brief Test: Delete Generic Task with NULL pointer
 *  @test Expected: generic_task_delete returns ESP_ERR_INVALID_ARG
 */
TEST(generic_task, delete_null_pointer)
{
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, generic_task_delete(NULL));
}

/** @brief Test: Is Running Generic Task with NULL pointer
 *  @test Expected: generic_task_is_running returns false
 */
TEST(generic_task, is_running_null_pointer)
{
  TEST_ASSERT_FALSE(generic_task_is_running(NULL));
}

/** @brief Test: Get State Generic Task with NULL pointer
 *  @test Expected: generic_task_get_state returns TASK_STATE_ERROR
 */
TEST(generic_task, get_state_null_pointer)
{
  TEST_ASSERT_EQUAL(TASK_STATE_ERROR, generic_task_get_state(NULL));
}

/** @brief Test: Create Generic Task
 *  @test Expected: generic_task_create returns non-NULL pointer with correct initial values
 */
TEST(generic_task, create)
{
  TEST_ASSERT_NULL(test_task);

  test_task = generic_task_create(TEST_TASK, sizeof(TestMsg_t), test_task_on_init, test_task_on_message, test_task_on_stop);
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS));

  TEST_ASSERT_NOT_NULL(test_task);
  TEST_ASSERT_EQUAL(sizeof(TestMsg_t), test_task->item_size);
  TEST_ASSERT_NOT_NULL(test_task->queue);
  TEST_ASSERT_NOT_NULL(test_task->state_mutex);
  TEST_ASSERT_NULL(test_task->handle);
  TEST_ASSERT_EQUAL(test_task->on_init, test_task_on_init);
  TEST_ASSERT_EQUAL(test_task->on_message, test_task_on_message);
  TEST_ASSERT_EQUAL(test_task->on_stop, test_task_on_stop);
  TEST_ASSERT_EQUAL(TASK_STATE_STOPPED, test_task->state);
}

/** @brief Test: Start Generic Task
 *  @test Expected: generic_task_start returns ESP_OK
 */
TEST(generic_task, start)
{
  TEST_ASSERT_NOT_NULL(test_task);

  test_task->on_init = test_task_on_init;

  TEST_ASSERT_EQUAL(ESP_OK, generic_task_start(test_task, TASK_STACK_SIZE, TASK_PRIORITY));
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS));

  eGenericTaskState state = generic_task_get_state(test_task);
  TEST_ASSERT_EQUAL(TASK_STATE_RUNNING, state);
}

/** @brief Test: Stop Generic Task
 *  @test Expected: generic_task_stop returns ESP_OK
 */
TEST(generic_task, stop)
{
  TEST_ASSERT_NOT_NULL(test_task);

  test_task->on_stop = test_task_on_stop;

  TEST_ASSERT_EQUAL(ESP_OK, generic_task_stop(test_task));
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS));

  eGenericTaskState state = generic_task_get_state(test_task);
  TEST_ASSERT_EQUAL(TASK_STATE_STOPPED, state);
}

/** @brief Test: Delete Generic Task
 *  @test Expected: generic_task_delete returns ESP_OK
 */
TEST(generic_task, delete)
{
  TEST_ASSERT_NOT_NULL(test_task);

  TEST_ASSERT_EQUAL(ESP_OK, generic_task_delete(test_task));
  test_task = NULL;

  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS));
}

/** @brief Test: Start Generic Task with failing on_init
 *  @test Expected: generic_task_start returns ESP_FAIL and task state is STOPPED
 */
TEST(generic_task, start_fail_on_init)
{
  TEST_ASSERT_NOT_NULL(test_task);

  // Assign a failing on_init callback
  test_task->on_init = test_task_on_init_fail;

  // Initial state should be STOPPED
  eGenericTaskState state = generic_task_get_state(test_task);
  TEST_ASSERT_EQUAL(TASK_STATE_STOPPED, state);

  TEST_ASSERT_EQUAL(ESP_FAIL, generic_task_start(test_task, TASK_STACK_SIZE, TASK_PRIORITY));
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS));

  // Task should remain in STOPPED state due to init failure
  state = generic_task_get_state(test_task);
  TEST_ASSERT_EQUAL(TASK_STATE_STOPPED, state);
}

/** @brief Test: Stop Generic Task that is not running
 *  @test Expected: generic_task_stop returns ESP_OK and task state remains STOPPED
 */
TEST(generic_task, stop_not_running_task)
{
  TEST_ASSERT_NOT_NULL(test_task);

  // Initial state should be STOPPED
  eGenericTaskState state = generic_task_get_state(test_task);
  TEST_ASSERT_EQUAL(TASK_STATE_STOPPED, state);

  TEST_ASSERT_EQUAL(ESP_OK, generic_task_stop(test_task));
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS));

  // State should remain STOPPED
  state = generic_task_get_state(test_task);
  TEST_ASSERT_EQUAL(TASK_STATE_STOPPED, state);
}

/** @brief Test: Stop Generic Task with failing on_stop
 *  @test Expected: Task stops successfully even if on_stop fails (failure is logged but ignored)
 */
TEST(generic_task, stop_fail_on_stop)
{
  TEST_ASSERT_NOT_NULL(test_task);

  // Assign a failing on_stop callback
  test_task->on_stop = test_task_on_stop_fail;

  // Initial state should be RUNNING
  eGenericTaskState state = generic_task_get_state(test_task);
  TEST_ASSERT_EQUAL(TASK_STATE_RUNNING, state);

  // Task should stop successfully even if on_stop callback fails
  TEST_ASSERT_EQUAL(ESP_OK, generic_task_stop(test_task));
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS));

  // State should be STOPPED (on_stop failure doesn't prevent shutdown)
  state = generic_task_get_state(test_task);
  TEST_ASSERT_EQUAL(TASK_STATE_STOPPED, state);
}

/** @brief Test: Delete Generic Task that is still running
 *  @test Expected: generic_task_delete returns ESP_ERR_INVALID_STATE
 */
TEST(generic_task, delete_running_task)
{
  TEST_ASSERT_NOT_NULL(test_task);

  // Task should still be running from previous test
  eGenericTaskState state = generic_task_get_state(test_task);
  TEST_ASSERT_EQUAL(TASK_STATE_RUNNING, state);

  // Attempting to delete running task should fail
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, generic_task_delete(test_task));
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS));

  // Task should still be running
  state = generic_task_get_state(test_task);
  TEST_ASSERT_EQUAL(TASK_STATE_RUNNING, state);
}

/** @brief Test: Post message to Generic Task
 *  @test Expected: generic_task_post_msg returns pdTRUE and message is received
 */
TEST(generic_task, post_message)
{
  TEST_ASSERT_NOT_NULL(test_task);

  // Ensure task is running
  eGenericTaskState state = generic_task_get_state(test_task);
  TEST_ASSERT_EQUAL(TASK_STATE_RUNNING, state);

  // Post a message
  TestMsg_t msg = {.value = 42};
  BaseType_t result = generic_task_post_msg(test_task, &msg, sizeof(TestMsg_t));
  TEST_ASSERT_EQUAL(pdTRUE, result);

  // Give task time to process
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS));
}

/** @brief Test: Post message with wrong size
 *  @test Expected: generic_task_post_msg returns errQUEUE_FULL
 */
TEST(generic_task, post_message_wrong_size)
{
  TEST_ASSERT_NOT_NULL(test_task);

  // Post a message with incorrect size
  int wrong_msg = 123;
  BaseType_t result = generic_task_post_msg(test_task, &wrong_msg, sizeof(int) + 1);
  TEST_ASSERT_EQUAL(errQUEUE_FULL, result);
}

/** @brief Test: Start already running task
 *  @test Expected: generic_task_start returns ESP_ERR_INVALID_STATE
 */
TEST(generic_task, start_already_running)
{
  TEST_ASSERT_NOT_NULL(test_task);

  // Verify task is running
  eGenericTaskState state = generic_task_get_state(test_task);
  TEST_ASSERT_EQUAL(TASK_STATE_RUNNING, state);

  // Attempt to start again
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, generic_task_start(test_task, TASK_STACK_SIZE, TASK_PRIORITY));
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS));

  // Should still be running
  state = generic_task_get_state(test_task);
  TEST_ASSERT_EQUAL(TASK_STATE_RUNNING, state);
}

TEST_GROUP_RUNNER(generic_task)
{
  // Null pointer tests
  RUN_TEST_CASE(generic_task, start_null_pointer);
  RUN_TEST_CASE(generic_task, stop_null_pointer);
  RUN_TEST_CASE(generic_task, delete_null_pointer);
  RUN_TEST_CASE(generic_task, is_running_null_pointer);
  RUN_TEST_CASE(generic_task, get_state_null_pointer);

  // Task lifecycle tests - normal flow
  RUN_TEST_CASE(generic_task, create);
  RUN_TEST_CASE(generic_task, start);
  RUN_TEST_CASE(generic_task, post_message);
  RUN_TEST_CASE(generic_task, post_message_wrong_size);
  RUN_TEST_CASE(generic_task, start_already_running);
  RUN_TEST_CASE(generic_task, delete_running_task);
  RUN_TEST_CASE(generic_task, stop);
  RUN_TEST_CASE(generic_task, delete);

  // Task lifecycle tests - error flow on start
  RUN_TEST_CASE(generic_task, create);
  RUN_TEST_CASE(generic_task, start_fail_on_init);
  RUN_TEST_CASE(generic_task, stop_not_running_task);
  RUN_TEST_CASE(generic_task, delete);

  // Task lifecycle tests - normal flow
  RUN_TEST_CASE(generic_task, create);
  RUN_TEST_CASE(generic_task, start);
  RUN_TEST_CASE(generic_task, stop_fail_on_stop);
  RUN_TEST_CASE(generic_task, delete);
}
