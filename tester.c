#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "queue.h"
#include "malloc_node_allocator.h"

#define TEST_THREADS 32

/* We're using malloc to allocate memory for queue nodes. */
lockfree_qnode_t *(*qnode_allocator)(void) = &malloc_node_allocator;
void(*qnode_deallocator)(lockfree_qnode_t *) = &malloc_node_deallocator;

/* Struct for information that is dispatched to a thread when it starts. */
typedef struct thread_test_data {
    /* A thread id, 0 to n-1. */
    int thread_id;
    /* The queue_test_t this thread belongs to. */
    struct queue_test *test;
} thread_test_data_t;

/* Struct for queue tests. */
typedef struct queue_test {
    /* The name of the queue test. */
    char *name;
    /* The number of threads to use during the test. */
    int num_threads;
    /* A queue the test can optionally use. */
    lockfree_queue_t queue;
    /* The main func for each launched thread. */
    void *(*thread_main_fp)(void *arg);
    /* The function to setup any data that should exist before the
     * test begins. */
    void (*test_setup_fp)(struct queue_test *self);
    /* Function for cleaning up tests and reporting if it was successful.
     * A return value of 1 indicates success. 0 means failure. */
    int (*test_cleanup_fp)(struct queue_test *self);
} queue_test_t;

/**
 * Runs an individual test.
 */
int run_test(queue_test_t *test)
{
    printf("Running test %s... ", test->name);

    lockfree_queue_init(&test->queue);

    /* Run the setup first. */
    test->test_setup_fp(test);

    /* Launch as many threads as we need to. */
    pthread_t *threads = malloc(sizeof(pthread_t) * test->num_threads);
    thread_test_data_t *thread_inputs = malloc(sizeof(thread_test_data_t) * test->num_threads);
    for (int t = 0; t < test->num_threads; t++) {
        thread_inputs[t].thread_id = t;
        thread_inputs[t].test = test;

        pthread_create(&threads[t], 0, test->thread_main_fp, &thread_inputs[t]);
    }

    /* Wait for the threads to finish. */
    void *thread_exit;
    for (int t = 0; t < test->num_threads; t++) {
        pthread_join(threads[t], &thread_exit);
    }

    /* Clean up the test. */
    int success = test->test_cleanup_fp(test);
    printf("%s\n", success ? "SUCCESS" : "FAILURE");
    return success;
}

/********************************************************************************************/
/************************ Everybody enqueue like there's no tomorrow ************************/
/********************************************************************************************/

void *enqueue_crazy_main(void *arg)
{
    thread_test_data_t *input = (thread_test_data_t *) arg;

    /* Add numbers 1 to 1000 to the queue. */
    for (int i = 1; i <= 1000; i++)
        lockfree_queue_enqueue(&input->test->queue, (void *) i);
    
    return 0;
}

void enqueue_crazy_setup(queue_test_t *test)
{
    /* Nothing to do. */
}

int enqueue_crazy_cleanup(queue_test_t *test)
{
    /* Make sure all the numbers appear num_threads times. */
    int counts[1000];
    memset(&counts, 0, sizeof(counts));
    
    int ret = (int) lockfree_queue_dequeue(&test->queue);
    while (ret != 0) {
        if (ret > 1000 || ret < 0) {
            /* We got a bad value! */
            return 1;
        }
        counts[ret - 1]++;
        ret = (int) lockfree_queue_dequeue(&test->queue);
    }

    int success = 1;
    /* Verify that we got the right number of counts. */
    for (int i = 0; i < 1000; i++) {
        success = success && (counts[i] == test->num_threads);
    }
    return success;
}

queue_test_t enqueue_crazy = {
    .name = "Everybody enqueues",
    .num_threads = 10,
    .thread_main_fp = &enqueue_crazy_main,
    .test_setup_fp = &enqueue_crazy_setup,
    .test_cleanup_fp = &enqueue_crazy_cleanup
};

int main() {

    queue_test_t *test_to_run[] = {
        &enqueue_crazy,
        0
    };

    int success = 1;
    queue_test_t **test = &test_to_run[0];
    for (queue_test_t **test = &test_to_run[0]; *test != 0; test++) {
        success = success && run_test(*test);
    }

    return success ? 0 : 1;
}
