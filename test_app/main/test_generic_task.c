#include "unity.h"
#include "unity_fixture.h"

#include "esp_log.h"

#include "generic_task.h"

#define TASK_STACK_SIZE 2048
#define TASK_PRIORITY   5

static const char *TAG = "test_generic_task";
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

/** @brief Test: Create Generic Task
 *  @test Expected: generic_task_create returns non-NULL pointer with correct initial values
 */
TEST(generic_task, create)
{
  test_task = generic_task_create(TEST_TASK, sizeof(TestMsg_t), test_task_on_init, test_task_on_message, test_task_on_stop);

  TEST_ASSERT_NOT_NULL(test_task);
  TEST_ASSERT_EQUAL(sizeof(TestMsg_t), test_task->item_size);
  TEST_ASSERT_NULL(test_task->queue);
  TEST_ASSERT_NULL(test_task->handle);
  TEST_ASSERT_NULL(test_task->state_mutex);
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
  TEST_ASSERT_EQUAL(ESP_OK, generic_task_start(test_task, TASK_STACK_SIZE, TASK_PRIORITY));
  eGenericTaskState state = generic_task_get_state(test_task);
  TEST_ASSERT_EQUAL(TASK_STATE_RUNNING, state);
}

/** @brief Test: Stop Generic Task
 *  @test Expected: generic_task_stop returns ESP_OK
 */
TEST(generic_task, stop)
{
  TEST_ASSERT_EQUAL(ESP_OK, generic_task_stop(test_task));
  eGenericTaskState state = generic_task_get_state(test_task);
  TEST_ASSERT_EQUAL(TASK_STATE_STOPPED, state);
}

/** @brief Test: Delete Generic Task
 *  @test Expected: generic_task_delete returns ESP_OK
 */
TEST(generic_task, delete)
{
  TEST_ASSERT_EQUAL(ESP_OK, generic_task_delete(test_task));
  test_task = NULL;
}

TEST_GROUP_RUNNER(generic_task)
{
  RUN_TEST_CASE(generic_task, create);
  RUN_TEST_CASE(generic_task, start);
  RUN_TEST_CASE(generic_task, stop);
  RUN_TEST_CASE(generic_task, delete);
}
