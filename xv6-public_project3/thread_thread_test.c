#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_THREAD 5

int status;
thread_t thread[NUM_THREAD+NUM_THREAD];
int expected[NUM_THREAD];

void failed()
{
  printf(1, "Test failed!\n");
  exit();
}

void *thread_thread_basic(void *arg){
    int val = (int)arg;
    printf(1, "Thread Thread %d start\n", val);
    if (val == 1) {
        sleep(200);
        status = 1;
    }
    printf(1, "Thread Thread %d end\n", val);
    thread_exit(arg);
    return 0;
}

void *thread_basic(void *arg)
{
  int val = (int)arg;
  int retval;
  thread_create(&thread[val+NUM_THREAD], thread_thread_basic, (void *)val);
  thread_join(thread[val+NUM_THREAD], (void**)&retval);
  thread_exit(arg);
  return 0;
}

void create_all(int n, void *(*entry)(void *))
{
  int i;
  for (i = 0; i < n; i++) {
    if (thread_create(&thread[i], entry, (void *)i) != 0) {
      printf(1, "Error creating thread %d\n", i);
      failed();
    }
  }
}

void join_all(int n)
{
  int i, retval;
  for (i = 0; i < n; i++) {
    if (thread_join(thread[i], (void **)&retval) != 0) {
      printf(1, "Error joining thread %d\n", i);
      failed();
    }
    if (retval != expected[i]) {
      printf(1, "Thread %d returned %d, but expected %d\n", i, retval, expected[i]);
      failed();
    }
  }
}

int main(int argc, char *argv[])
{
  int i;
  for (i = 0; i < NUM_THREAD; i++)
    expected[i] = i;

  printf(1, "Test 1: Basic test\n");
  create_all(3, thread_basic);
  sleep(100);
  printf(1, "Parent waiting for children...\n");
  join_all(3);
  if (status != 1) {
    printf(1, "Join returned before thread exit, or the address space is not properly shared\n");
    failed();
  }
  printf(1, "Test 1 passed\n\n");
  exit();
}
