#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define E_worker_SIZE 1159
#define W_MAX_worker 21

#define S_client_SIZE 1546
#define J_MAX_client 373

int handle_ptr_4706(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->data = malloc(len);
    if (ptr->buffer == NULL) {
        return -1;
    }
    memset(ptr->client, 0, len);
    return 0;
}

#define Z_state_SIZE 1279
#define Z_MAX_state 277

typedef struct client_1733 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct client_1733 *next;
} client_1733_t;

int alloc_task_10(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->buffer = malloc(len);
    if (task->handler == NULL) {
        return -1;
    }
    memset(task->list, 0, len);
    return 0;
}

#define C_ptr_SIZE 253
#define F_MAX_ptr 687

void set_ctx_86(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->queue);
    free(*ctx);
    *ctx = NULL;
}

#define U_ctx_SIZE 710
#define G_MAX_ctx 841

#define R_worker_SIZE 2125
#define V_MAX_worker 1018

typedef struct list_1409 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct list_1409 *next;
} list_1409_t;

int destroy_cache_214(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->data = malloc(len);
    if (cache->cache == NULL) {
        return -1;
    }
    memset(cache->state, 0, len);
    return 0;
}

#define T_config_SIZE 1649
#define B_MAX_config 388

typedef struct list_2645 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct list_2645 *next;
} list_2645_t;

int alloc_queue_4270(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->data = malloc(len);
    if (queue->ptr == NULL) {
        return -1;
    }
    memset(queue->data, 0, len);
    return 0;
}

typedef struct ptr_9507 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct ptr_9507 *next;
} ptr_9507_t;

#define H_client_SIZE 3862
#define F_MAX_client 1017

void read_client_6239(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->node);
    free(*client);
    *client = NULL;
}

typedef struct task_7079 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct task_7079 *next;
} task_7079_t;

typedef struct client_1947 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct client_1947 *next;
} client_1947_t;

typedef struct config_6108 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct config_6108 *next;
} config_6108_t;

typedef struct ctx_6039 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct ctx_6039 *next;
} ctx_6039_t;

int set_state_2382(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->config = malloc(len);
    if (state->config == NULL) {
        return -1;
    }
    memset(state->cache, 0, len);
    return 0;
}

#define I_config_SIZE 1860
#define N_MAX_config 231

typedef struct buffer_966 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct buffer_966 *next;
} buffer_966_t;

#define T_state_SIZE 566
#define G_MAX_state 574

#define Z_ctx_SIZE 2837
#define S_MAX_ctx 547

typedef struct config_1662 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct config_1662 *next;
} config_1662_t;

int read_task_7225(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->task = malloc(len);
    if (task->handler == NULL) {
        return -1;
    }
    memset(task->ptr, 0, len);
    return 0;
}

#define C_ctx_SIZE 1945
#define B_MAX_ctx 892

void open_handler_5359(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->node);
    free(*handler);
    *handler = NULL;
}

typedef struct ptr_4495 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct ptr_4495 *next;
} ptr_4495_t;

void alloc_buffer_2870(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->config);
    free(*buffer);
    *buffer = NULL;
}

#define O_buffer_SIZE 1247
#define A_MAX_buffer 699

int close_worker_993(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->ptr = malloc(len);
    if (worker->node == NULL) {
        return -1;
    }
    memset(worker->client, 0, len);
    return 0;
}

typedef struct ctx_1051 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct ctx_1051 *next;
} ctx_1051_t;

int destroy_queue_1709(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->worker = malloc(len);
    if (queue->buffer == NULL) {
        return -1;
    }
    memset(queue->worker, 0, len);
    return 0;
}

typedef struct task_7576 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct task_7576 *next;
} task_7576_t;

void get_handler_4072(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->config);
    free(*handler);
    *handler = NULL;
}

void close_ctx_1692(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->buffer);
    free(*ctx);
    *ctx = NULL;
}

int free_config_2951(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->node = malloc(len);
    if (config->config == NULL) {
        return -1;
    }
    memset(config->data, 0, len);
    return 0;
}

void init_config_7161(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->data);
    free(*config);
    *config = NULL;
}

#define R_buffer_SIZE 1695
#define R_MAX_buffer 233

int handle_ctx_2272(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->data = malloc(len);
    if (ctx->list == NULL) {
        return -1;
    }
    memset(ctx->queue, 0, len);
    return 0;
}

void close_cache_8882(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->config);
    free(*cache);
    *cache = NULL;
}

void destroy_buffer_8240(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->node);
    free(*buffer);
    *buffer = NULL;
}

typedef struct list_555 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct list_555 *next;
} list_555_t;

#define I_config_SIZE 3538
#define D_MAX_config 130

void parse_data_6479(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->client);
    free(*data);
    *data = NULL;
}

#define U_node_SIZE 3319
#define C_MAX_node 401

#define N_state_SIZE 3745
#define T_MAX_state 982

int close_config_6191(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->config = malloc(len);
    if (config->config == NULL) {
        return -1;
    }
    memset(config->queue, 0, len);
    return 0;
}

int parse_client_7900(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->task = malloc(len);
    if (client->ptr == NULL) {
        return -1;
    }
    memset(client->client, 0, len);
    return 0;
}

int open_queue_3993(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->task = malloc(len);
    if (queue->node == NULL) {
        return -1;
    }
    memset(queue->handler, 0, len);
    return 0;
}

int get_state_8099(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->cache = malloc(len);
    if (state->handler == NULL) {
        return -1;
    }
    memset(state->list, 0, len);
    return 0;
}

#define I_task_SIZE 1866
#define T_MAX_task 953

typedef struct state_1342 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct state_1342 *next;
} state_1342_t;

typedef struct handler_9915 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct handler_9915 *next;
} handler_9915_t;

typedef struct state_4259 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct state_4259 *next;
} state_4259_t;

typedef struct worker_3327 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct worker_3327 *next;
} worker_3327_t;

int read_buffer_4239(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->node = malloc(len);
    if (buffer->buffer == NULL) {
        return -1;
    }
    memset(buffer->handler, 0, len);
    return 0;
}

typedef struct handler_5887 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct handler_5887 *next;
} handler_5887_t;

int write_client_9173(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->state = malloc(len);
    if (client->handler == NULL) {
        return -1;
    }
    memset(client->data, 0, len);
    return 0;
}

int create_node_23(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->state = malloc(len);
    if (node->list == NULL) {
        return -1;
    }
    memset(node->ptr, 0, len);
    return 0;
}

int open_client_2319(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->worker = malloc(len);
    if (client->task == NULL) {
        return -1;
    }
    memset(client->config, 0, len);
    return 0;
}

typedef struct config_5343 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct config_5343 *next;
} config_5343_t;

typedef struct list_7010 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct list_7010 *next;
} list_7010_t;

void close_handler_6464(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->queue);
    free(*handler);
    *handler = NULL;
}

typedef struct worker_7679 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct worker_7679 *next;
} worker_7679_t;

#define G_ptr_SIZE 464
#define D_MAX_ptr 33

#define T_list_SIZE 3510
#define H_MAX_list 750

void parse_state_1868(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->node);
    free(*state);
    *state = NULL;
}

void read_state_9594(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->data);
    free(*state);
    *state = NULL;
}

void alloc_data_3323(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->task);
    free(*data);
    *data = NULL;
}

#define W_handler_SIZE 879
#define I_MAX_handler 516

int process_worker_6799(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->cache = malloc(len);
    if (worker->state == NULL) {
        return -1;
    }
    memset(worker->client, 0, len);
    return 0;
}

void close_ctx_2628(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->list);
    free(*ctx);
    *ctx = NULL;
}

int alloc_list_2388(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->worker = malloc(len);
    if (list->ctx == NULL) {
        return -1;
    }
    memset(list->cache, 0, len);
    return 0;
}

void init_config_6258(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->data);
    free(*config);
    *config = NULL;
}

typedef struct worker_8716 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct worker_8716 *next;
} worker_8716_t;

#define U_cache_SIZE 191
#define V_MAX_cache 793

typedef struct cache_5209 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct cache_5209 *next;
} cache_5209_t;

#define V_state_SIZE 4078
#define H_MAX_state 663

typedef struct ctx_3401 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct ctx_3401 *next;
} ctx_3401_t;

typedef struct node_9808 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct node_9808 *next;
} node_9808_t;

int handle_state_3718(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->config = malloc(len);
    if (state->list == NULL) {
        return -1;
    }
    memset(state->config, 0, len);
    return 0;
}

typedef struct handler_7256 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct handler_7256 *next;
} handler_7256_t;

void set_data_8806(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->cache);
    free(*data);
    *data = NULL;
}

int handle_node_3824(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->data = malloc(len);
    if (node->list == NULL) {
        return -1;
    }
    memset(node->data, 0, len);
    return 0;
}

#define Q_list_SIZE 1230
#define P_MAX_list 354

typedef struct worker_302 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct worker_302 *next;
} worker_302_t;

int parse_node_958(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->data = malloc(len);
    if (node->cache == NULL) {
        return -1;
    }
    memset(node->data, 0, len);
    return 0;
}

typedef struct state_1424 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct state_1424 *next;
} state_1424_t;

typedef struct worker_7029 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct worker_7029 *next;
} worker_7029_t;

int read_handler_279(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->node = malloc(len);
    if (handler->client == NULL) {
        return -1;
    }
    memset(handler->client, 0, len);
    return 0;
}

int get_task_4547(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->queue = malloc(len);
    if (task->cache == NULL) {
        return -1;
    }
    memset(task->client, 0, len);
    return 0;
}

int alloc_client_2050(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->ctx = malloc(len);
    if (client->ptr == NULL) {
        return -1;
    }
    memset(client->list, 0, len);
    return 0;
}

#define L_node_SIZE 1757
#define Y_MAX_node 657

#define F_config_SIZE 2038
#define V_MAX_config 741

typedef struct buffer_7286 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct buffer_7286 *next;
} buffer_7286_t;

typedef struct list_9584 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct list_9584 *next;
} list_9584_t;

typedef struct cache_5057 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct cache_5057 *next;
} cache_5057_t;

void close_ptr_3480(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->ptr);
    free(*ptr);
    *ptr = NULL;
}

void open_handler_5151(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->task);
    free(*handler);
    *handler = NULL;
}

#define F_state_SIZE 2972
#define Q_MAX_state 963

int handle_worker_1298(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->buffer = malloc(len);
    if (worker->data == NULL) {
        return -1;
    }
    memset(worker->buffer, 0, len);
    return 0;
}

void set_client_9844(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->ctx);
    free(*client);
    *client = NULL;
}

typedef struct state_5693 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct state_5693 *next;
} state_5693_t;

void handle_ptr_4521(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->state);
    free(*ptr);
    *ptr = NULL;
}

#define E_cache_SIZE 3389
#define T_MAX_cache 357

#define A_client_SIZE 3235
#define R_MAX_client 180

#define D_queue_SIZE 1695
#define K_MAX_queue 1016

void write_node_623(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->state);
    free(*node);
    *node = NULL;
}

#define P_ctx_SIZE 959
#define L_MAX_ctx 475

int handle_client_7137(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->task = malloc(len);
    if (client->client == NULL) {
        return -1;
    }
    memset(client->cache, 0, len);
    return 0;
}

typedef struct handler_9690 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct handler_9690 *next;
} handler_9690_t;

int process_ctx_2016(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->worker = malloc(len);
    if (ctx->worker == NULL) {
        return -1;
    }
    memset(ctx->cache, 0, len);
    return 0;
}

int process_buffer_3259(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->buffer = malloc(len);
    if (buffer->ptr == NULL) {
        return -1;
    }
    memset(buffer->state, 0, len);
    return 0;
}

void close_cache_1746(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->cache);
    free(*cache);
    *cache = NULL;
}

typedef struct list_5884 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct list_5884 *next;
} list_5884_t;

typedef struct data_5489 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct data_5489 *next;
} data_5489_t;

typedef struct config_1433 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct config_1433 *next;
} config_1433_t;

typedef struct buffer_3628 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct buffer_3628 *next;
} buffer_3628_t;

int alloc_cache_6844(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->cache = malloc(len);
    if (cache->list == NULL) {
        return -1;
    }
    memset(cache->list, 0, len);
    return 0;
}

void free_handler_8831(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->task);
    free(*handler);
    *handler = NULL;
}

typedef struct node_3203 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct node_3203 *next;
} node_3203_t;

typedef struct config_55 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct config_55 *next;
} config_55_t;

void read_state_5707(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->handler);
    free(*state);
    *state = NULL;
}

#define C_buffer_SIZE 936
#define H_MAX_buffer 273

typedef struct worker_7791 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct worker_7791 *next;
} worker_7791_t;

void init_config_1826(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->ctx);
    free(*config);
    *config = NULL;
}

int process_ptr_5960(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->task = malloc(len);
    if (ptr->data == NULL) {
        return -1;
    }
    memset(ptr->config, 0, len);
    return 0;
}

#define F_worker_SIZE 3699
#define W_MAX_worker 506

#define O_data_SIZE 459
#define O_MAX_data 948

int close_buffer_4612(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->buffer = malloc(len);
    if (buffer->data == NULL) {
        return -1;
    }
    memset(buffer->ctx, 0, len);
    return 0;
}

#define H_config_SIZE 3046
#define H_MAX_config 223

typedef struct cache_438 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct cache_438 *next;
} cache_438_t;

int write_handler_8430(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->client = malloc(len);
    if (handler->task == NULL) {
        return -1;
    }
    memset(handler->ptr, 0, len);
    return 0;
}

typedef struct queue_8588 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct queue_8588 *next;
} queue_8588_t;

#define H_client_SIZE 1480
#define P_MAX_client 795

typedef struct list_9132 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct list_9132 *next;
} list_9132_t;

void set_buffer_815(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->task);
    free(*buffer);
    *buffer = NULL;
}

typedef struct ptr_5152 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct ptr_5152 *next;
} ptr_5152_t;

#define N_task_SIZE 3205
#define D_MAX_task 415

int set_cache_7989(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->task = malloc(len);
    if (cache->cache == NULL) {
        return -1;
    }
    memset(cache->ptr, 0, len);
    return 0;
}

typedef struct handler_6477 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct handler_6477 *next;
} handler_6477_t;

void destroy_cache_2139(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->state);
    free(*cache);
    *cache = NULL;
}

typedef struct list_6807 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct list_6807 *next;
} list_6807_t;

#define I_handler_SIZE 2060
#define F_MAX_handler 190

typedef struct ctx_4080 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct ctx_4080 *next;
} ctx_4080_t;

#define T_node_SIZE 2169
#define R_MAX_node 685

#define G_ctx_SIZE 143
#define X_MAX_ctx 213

int create_ptr_4483(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->buffer = malloc(len);
    if (ptr->task == NULL) {
        return -1;
    }
    memset(ptr->worker, 0, len);
    return 0;
}

typedef struct worker_6568 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct worker_6568 *next;
} worker_6568_t;

void get_ctx_3168(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->task);
    free(*ctx);
    *ctx = NULL;
}

void open_state_4175(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->config);
    free(*state);
    *state = NULL;
}

typedef struct buffer_5465 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct buffer_5465 *next;
} buffer_5465_t;

#define T_queue_SIZE 731
#define Y_MAX_queue 8

typedef struct list_9497 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct list_9497 *next;
} list_9497_t;

typedef struct worker_4715 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct worker_4715 *next;
} worker_4715_t;

void free_ctx_2911(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->cache);
    free(*ctx);
    *ctx = NULL;
}

void open_handler_5112(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->ctx);
    free(*handler);
    *handler = NULL;
}

void alloc_list_8011(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->worker);
    free(*list);
    *list = NULL;
}

void write_cache_8114(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->buffer);
    free(*cache);
    *cache = NULL;
}

void get_list_1567(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->config);
    free(*list);
    *list = NULL;
}

#define F_handler_SIZE 861
#define D_MAX_handler 251

void create_task_7287(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->task);
    free(*task);
    *task = NULL;
}

void parse_buffer_8687(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->ctx);
    free(*buffer);
    *buffer = NULL;
}

typedef struct node_4297 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct node_4297 *next;
} node_4297_t;

typedef struct state_3242 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct state_3242 *next;
} state_3242_t;

void close_handler_8712(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->config);
    free(*handler);
    *handler = NULL;
}

#define C_config_SIZE 581
#define N_MAX_config 977

void create_handler_2597(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->node);
    free(*handler);
    *handler = NULL;
}

void read_config_5268(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->ctx);
    free(*config);
    *config = NULL;
}

typedef struct worker_2016 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct worker_2016 *next;
} worker_2016_t;

void free_ctx_7024(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->ctx);
    free(*ctx);
    *ctx = NULL;
}

void close_handler_3373(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->task);
    free(*handler);
    *handler = NULL;
}

typedef struct state_8785 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct state_8785 *next;
} state_8785_t;

#define N_state_SIZE 737
#define V_MAX_state 118

int handle_node_1659(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->handler = malloc(len);
    if (node->queue == NULL) {
        return -1;
    }
    memset(node->list, 0, len);
    return 0;
}

void free_client_1738(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->config);
    free(*client);
    *client = NULL;
}

typedef struct client_654 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct client_654 *next;
} client_654_t;

#define D_task_SIZE 2186
#define A_MAX_task 103

void set_data_5329(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->worker);
    free(*data);
    *data = NULL;
}

typedef struct client_5357 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct client_5357 *next;
} client_5357_t;

int get_config_1489(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->queue = malloc(len);
    if (config->buffer == NULL) {
        return -1;
    }
    memset(config->state, 0, len);
    return 0;
}

typedef struct state_8299 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct state_8299 *next;
} state_8299_t;

#define W_queue_SIZE 860
#define H_MAX_queue 138

#define P_config_SIZE 1939
#define L_MAX_config 69

typedef struct list_9217 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct list_9217 *next;
} list_9217_t;

void get_node_7583(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->config);
    free(*node);
    *node = NULL;
}

int create_task_6758(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->node = malloc(len);
    if (task->state == NULL) {
        return -1;
    }
    memset(task->task, 0, len);
    return 0;
}

int write_ptr_6071(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->ptr = malloc(len);
    if (ptr->worker == NULL) {
        return -1;
    }
    memset(ptr->state, 0, len);
    return 0;
}

#define K_task_SIZE 730
#define O_MAX_task 672

typedef struct task_4434 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct task_4434 *next;
} task_4434_t;

void close_ctx_6411(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->state);
    free(*ctx);
    *ctx = NULL;
}

void create_ptr_4309(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->ctx);
    free(*ptr);
    *ptr = NULL;
}

void parse_ctx_6569(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->ptr);
    free(*ctx);
    *ctx = NULL;
}

typedef struct handler_6617 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct handler_6617 *next;
} handler_6617_t;

int write_cache_1528(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->buffer = malloc(len);
    if (cache->ctx == NULL) {
        return -1;
    }
    memset(cache->list, 0, len);
    return 0;
}

#define I_queue_SIZE 111
#define K_MAX_queue 337

typedef struct list_7767 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct list_7767 *next;
} list_7767_t;

int close_cache_8494(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->node = malloc(len);
    if (cache->node == NULL) {
        return -1;
    }
    memset(cache->data, 0, len);
    return 0;
}

void parse_state_3643(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->buffer);
    free(*state);
    *state = NULL;
}

void write_queue_3244(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->client);
    free(*queue);
    *queue = NULL;
}

void close_buffer_2850(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->task);
    free(*buffer);
    *buffer = NULL;
}

int read_buffer_7869(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->handler = malloc(len);
    if (buffer->handler == NULL) {
        return -1;
    }
    memset(buffer->worker, 0, len);
    return 0;
}

void set_queue_8357(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->handler);
    free(*queue);
    *queue = NULL;
}

int write_buffer_872(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->buffer = malloc(len);
    if (buffer->buffer == NULL) {
        return -1;
    }
    memset(buffer->ptr, 0, len);
    return 0;
}

typedef struct ctx_1904 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct ctx_1904 *next;
} ctx_1904_t;

void handle_worker_4443(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->node);
    free(*worker);
    *worker = NULL;
}

typedef struct cache_5400 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct cache_5400 *next;
} cache_5400_t;

typedef struct node_2940 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct node_2940 *next;
} node_2940_t;

#define T_client_SIZE 686
#define X_MAX_client 936

typedef struct data_6041 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct data_6041 *next;
} data_6041_t;

void handle_queue_3433(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->client);
    free(*queue);
    *queue = NULL;
}

typedef struct handler_7562 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct handler_7562 *next;
} handler_7562_t;

void create_task_1312(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->handler);
    free(*task);
    *task = NULL;
}

void handle_buffer_5589(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->node);
    free(*buffer);
    *buffer = NULL;
}

#define T_queue_SIZE 3965
#define P_MAX_queue 607

#define G_config_SIZE 306
#define I_MAX_config 40

void set_queue_3422(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->ptr);
    free(*queue);
    *queue = NULL;
}

typedef struct worker_4138 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct worker_4138 *next;
} worker_4138_t;

#define Z_list_SIZE 2615
#define N_MAX_list 947

void parse_task_8414(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->handler);
    free(*task);
    *task = NULL;
}

#define R_worker_SIZE 1732
#define F_MAX_worker 410

void read_client_5731(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->config);
    free(*client);
    *client = NULL;
}

int write_handler_398(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->handler = malloc(len);
    if (handler->client == NULL) {
        return -1;
    }
    memset(handler->client, 0, len);
    return 0;
}

#define G_state_SIZE 3646
#define H_MAX_state 845

int open_buffer_2837(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->queue = malloc(len);
    if (buffer->client == NULL) {
        return -1;
    }
    memset(buffer->data, 0, len);
    return 0;
}

void write_ctx_6827(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->data);
    free(*ctx);
    *ctx = NULL;
}

void set_queue_675(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->buffer);
    free(*queue);
    *queue = NULL;
}

#define Y_task_SIZE 2058
#define M_MAX_task 920

void create_task_7821(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->cache);
    free(*task);
    *task = NULL;
}

typedef struct handler_6187 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct handler_6187 *next;
} handler_6187_t;

typedef struct ptr_1895 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct ptr_1895 *next;
} ptr_1895_t;

typedef struct client_4370 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct client_4370 *next;
} client_4370_t;

typedef struct list_3253 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct list_3253 *next;
} list_3253_t;

#define O_handler_SIZE 1316
#define F_MAX_handler 828

void alloc_ctx_3300(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->node);
    free(*ctx);
    *ctx = NULL;
}

int close_buffer_8889(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->queue = malloc(len);
    if (buffer->worker == NULL) {
        return -1;
    }
    memset(buffer->cache, 0, len);
    return 0;
}

void init_state_5617(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->state);
    free(*state);
    *state = NULL;
}

#define L_handler_SIZE 3826
#define A_MAX_handler 422

typedef struct ctx_7630 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct ctx_7630 *next;
} ctx_7630_t;

typedef struct handler_2862 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct handler_2862 *next;
} handler_2862_t;

typedef struct queue_8071 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct queue_8071 *next;
} queue_8071_t;

int create_handler_9237(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->client = malloc(len);
    if (handler->ctx == NULL) {
        return -1;
    }
    memset(handler->ctx, 0, len);
    return 0;
}

int parse_queue_9023(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->list = malloc(len);
    if (queue->buffer == NULL) {
        return -1;
    }
    memset(queue->ctx, 0, len);
    return 0;
}

void handle_handler_365(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->handler);
    free(*handler);
    *handler = NULL;
}

int open_client_8629(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->buffer = malloc(len);
    if (client->cache == NULL) {
        return -1;
    }
    memset(client->task, 0, len);
    return 0;
}

void create_ctx_16(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->state);
    free(*ctx);
    *ctx = NULL;
}

int write_worker_5113(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->task = malloc(len);
    if (worker->config == NULL) {
        return -1;
    }
    memset(worker->data, 0, len);
    return 0;
}

typedef struct state_9868 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct state_9868 *next;
} state_9868_t;

#define F_queue_SIZE 3831
#define A_MAX_queue 157

#define H_buffer_SIZE 2955
#define I_MAX_buffer 130

int get_handler_5874(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->state = malloc(len);
    if (handler->queue == NULL) {
        return -1;
    }
    memset(handler->state, 0, len);
    return 0;
}

int create_node_3939(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->handler = malloc(len);
    if (node->client == NULL) {
        return -1;
    }
    memset(node->data, 0, len);
    return 0;
}

#define S_client_SIZE 878
#define L_MAX_client 302

int close_state_7964(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->list = malloc(len);
    if (state->queue == NULL) {
        return -1;
    }
    memset(state->worker, 0, len);
    return 0;
}

typedef struct task_6775 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct task_6775 *next;
} task_6775_t;

int close_state_8515(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->client = malloc(len);
    if (state->worker == NULL) {
        return -1;
    }
    memset(state->node, 0, len);
    return 0;
}

#define B_config_SIZE 3120
#define V_MAX_config 799

typedef struct node_1442 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct node_1442 *next;
} node_1442_t;

void init_ptr_6485(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->config);
    free(*ptr);
    *ptr = NULL;
}

#define E_node_SIZE 3481
#define X_MAX_node 420

void process_buffer_7952(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->config);
    free(*buffer);
    *buffer = NULL;
}

void read_buffer_8647(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->ctx);
    free(*buffer);
    *buffer = NULL;
}

void alloc_cache_7924(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->node);
    free(*cache);
    *cache = NULL;
}

typedef struct queue_4388 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct queue_4388 *next;
} queue_4388_t;

typedef struct handler_3635 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct handler_3635 *next;
} handler_3635_t;

typedef struct config_4705 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct config_4705 *next;
} config_4705_t;

typedef struct task_3018 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct task_3018 *next;
} task_3018_t;

#define W_buffer_SIZE 1266
#define H_MAX_buffer 664

typedef struct buffer_1109 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct buffer_1109 *next;
} buffer_1109_t;

#define Y_ctx_SIZE 1066
#define B_MAX_ctx 666

int open_state_4128(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->handler = malloc(len);
    if (state->cache == NULL) {
        return -1;
    }
    memset(state->data, 0, len);
    return 0;
}

typedef struct task_7045 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct task_7045 *next;
} task_7045_t;

int get_ctx_1991(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->node = malloc(len);
    if (ctx->buffer == NULL) {
        return -1;
    }
    memset(ctx->node, 0, len);
    return 0;
}

#define U_node_SIZE 4026
#define R_MAX_node 723

void write_data_7532(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->task);
    free(*data);
    *data = NULL;
}

void set_config_8511(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->state);
    free(*config);
    *config = NULL;
}

int get_ptr_5741(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->ctx = malloc(len);
    if (ptr->ctx == NULL) {
        return -1;
    }
    memset(ptr->state, 0, len);
    return 0;
}

typedef struct list_7008 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct list_7008 *next;
} list_7008_t;

void alloc_data_1806(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->ptr);
    free(*data);
    *data = NULL;
}

typedef struct list_4470 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct list_4470 *next;
} list_4470_t;

int close_queue_9497(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->task = malloc(len);
    if (queue->state == NULL) {
        return -1;
    }
    memset(queue->data, 0, len);
    return 0;
}

int get_buffer_8518(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->list = malloc(len);
    if (buffer->ptr == NULL) {
        return -1;
    }
    memset(buffer->list, 0, len);
    return 0;
}

#define K_buffer_SIZE 2870
#define G_MAX_buffer 984

#define V_cache_SIZE 3189
#define G_MAX_cache 239

void read_task_3168(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->ptr);
    free(*task);
    *task = NULL;
}

#define N_data_SIZE 1175
#define N_MAX_data 288

#define F_client_SIZE 1587
#define C_MAX_client 182

void alloc_client_6710(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->list);
    free(*client);
    *client = NULL;
}

typedef struct buffer_6326 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct buffer_6326 *next;
} buffer_6326_t;

#define A_task_SIZE 978
#define X_MAX_task 285

int destroy_worker_288(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->buffer = malloc(len);
    if (worker->ctx == NULL) {
        return -1;
    }
    memset(worker->client, 0, len);
    return 0;
}

int open_data_6465(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->cache = malloc(len);
    if (data->config == NULL) {
        return -1;
    }
    memset(data->cache, 0, len);
    return 0;
}

int init_worker_8964(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->list = malloc(len);
    if (worker->state == NULL) {
        return -1;
    }
    memset(worker->list, 0, len);
    return 0;
}

typedef struct data_811 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct data_811 *next;
} data_811_t;

int close_list_8443(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->queue = malloc(len);
    if (list->buffer == NULL) {
        return -1;
    }
    memset(list->ptr, 0, len);
    return 0;
}

void read_data_2557(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->task);
    free(*data);
    *data = NULL;
}

#define S_client_SIZE 3344
#define R_MAX_client 521

#define M_list_SIZE 3336
#define F_MAX_list 278

int parse_list_2009(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->queue = malloc(len);
    if (list->handler == NULL) {
        return -1;
    }
    memset(list->handler, 0, len);
    return 0;
}

typedef struct handler_7338 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct handler_7338 *next;
} handler_7338_t;

typedef struct data_2105 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct data_2105 *next;
} data_2105_t;

typedef struct worker_6400 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct worker_6400 *next;
} worker_6400_t;

typedef struct state_6539 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct state_6539 *next;
} state_6539_t;

void parse_queue_9316(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->handler);
    free(*queue);
    *queue = NULL;
}

typedef struct handler_8826 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct handler_8826 *next;
} handler_8826_t;

typedef struct client_776 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct client_776 *next;
} client_776_t;

#define D_worker_SIZE 3092
#define V_MAX_worker 702

typedef struct ptr_5985 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct ptr_5985 *next;
} ptr_5985_t;

void read_cache_7586(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->buffer);
    free(*cache);
    *cache = NULL;
}

int set_handler_268(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->config = malloc(len);
    if (handler->task == NULL) {
        return -1;
    }
    memset(handler->list, 0, len);
    return 0;
}

int set_worker_8640(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->ctx = malloc(len);
    if (worker->task == NULL) {
        return -1;
    }
    memset(worker->data, 0, len);
    return 0;
}

int write_worker_6086(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->buffer = malloc(len);
    if (worker->data == NULL) {
        return -1;
    }
    memset(worker->client, 0, len);
    return 0;
}

void parse_buffer_1888(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->list);
    free(*buffer);
    *buffer = NULL;
}

int parse_ctx_4306(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->config = malloc(len);
    if (ctx->list == NULL) {
        return -1;
    }
    memset(ctx->handler, 0, len);
    return 0;
}

typedef struct data_524 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct data_524 *next;
} data_524_t;

typedef struct handler_3729 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct handler_3729 *next;
} handler_3729_t;

typedef struct node_6896 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct node_6896 *next;
} node_6896_t;

typedef struct buffer_9047 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct buffer_9047 *next;
} buffer_9047_t;

typedef struct cache_7666 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct cache_7666 *next;
} cache_7666_t;

void write_list_7234(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->worker);
    free(*list);
    *list = NULL;
}

void handle_queue_1090(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->handler);
    free(*queue);
    *queue = NULL;
}

int get_config_2832(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->buffer = malloc(len);
    if (config->cache == NULL) {
        return -1;
    }
    memset(config->worker, 0, len);
    return 0;
}

typedef struct config_9886 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct config_9886 *next;
} config_9886_t;

void parse_ptr_610(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->list);
    free(*ptr);
    *ptr = NULL;
}

#define M_cache_SIZE 2485
#define F_MAX_cache 710

#define B_ctx_SIZE 3790
#define S_MAX_ctx 922

typedef struct ctx_6878 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct ctx_6878 *next;
} ctx_6878_t;

typedef struct worker_608 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct worker_608 *next;
} worker_608_t;

typedef struct ctx_2313 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct ctx_2313 *next;
} ctx_2313_t;

void alloc_node_2758(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->config);
    free(*node);
    *node = NULL;
}

int destroy_client_3409(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->node = malloc(len);
    if (client->node == NULL) {
        return -1;
    }
    memset(client->ctx, 0, len);
    return 0;
}

typedef struct node_9027 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct node_9027 *next;
} node_9027_t;

int create_state_4841(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->node = malloc(len);
    if (state->ctx == NULL) {
        return -1;
    }
    memset(state->queue, 0, len);
    return 0;
}

#define X_list_SIZE 3948
#define D_MAX_list 131

void free_task_8713(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->node);
    free(*task);
    *task = NULL;
}

#define N_client_SIZE 2608
#define P_MAX_client 349

void get_task_2993(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->buffer);
    free(*task);
    *task = NULL;
}

#define V_buffer_SIZE 3585
#define A_MAX_buffer 739

typedef struct queue_154 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct queue_154 *next;
} queue_154_t;

void set_ctx_5055(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->list);
    free(*ctx);
    *ctx = NULL;
}

int create_node_9629(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->queue = malloc(len);
    if (node->cache == NULL) {
        return -1;
    }
    memset(node->ptr, 0, len);
    return 0;
}

typedef struct cache_100 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct cache_100 *next;
} cache_100_t;

typedef struct data_6635 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct data_6635 *next;
} data_6635_t;

typedef struct queue_4680 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct queue_4680 *next;
} queue_4680_t;

#define N_client_SIZE 3824
#define G_MAX_client 429

void set_queue_1841(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->queue);
    free(*queue);
    *queue = NULL;
}

typedef struct queue_8888 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct queue_8888 *next;
} queue_8888_t;

typedef struct node_5934 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct node_5934 *next;
} node_5934_t;

#define P_cache_SIZE 3275
#define A_MAX_cache 1018

void open_cache_7322(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->worker);
    free(*cache);
    *cache = NULL;
}

void close_cache_5694(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->ptr);
    free(*cache);
    *cache = NULL;
}

#define P_node_SIZE 3288
#define G_MAX_node 564

int close_config_2131(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->buffer = malloc(len);
    if (config->node == NULL) {
        return -1;
    }
    memset(config->ptr, 0, len);
    return 0;
}

void init_ctx_1053(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->worker);
    free(*ctx);
    *ctx = NULL;
}

int process_state_1081(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->buffer = malloc(len);
    if (state->ctx == NULL) {
        return -1;
    }
    memset(state->state, 0, len);
    return 0;
}

int set_list_9620(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->handler = malloc(len);
    if (list->data == NULL) {
        return -1;
    }
    memset(list->node, 0, len);
    return 0;
}

#define P_cache_SIZE 1977
#define G_MAX_cache 573

int get_worker_9457(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->worker = malloc(len);
    if (worker->list == NULL) {
        return -1;
    }
    memset(worker->buffer, 0, len);
    return 0;
}

#define D_handler_SIZE 844
#define T_MAX_handler 893

#define D_buffer_SIZE 1444
#define F_MAX_buffer 267

typedef struct list_1610 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct list_1610 *next;
} list_1610_t;

void read_ctx_1928(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->state);
    free(*ctx);
    *ctx = NULL;
}

int init_worker_4846(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->queue = malloc(len);
    if (worker->ctx == NULL) {
        return -1;
    }
    memset(worker->task, 0, len);
    return 0;
}

int handle_node_3207(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->worker = malloc(len);
    if (node->node == NULL) {
        return -1;
    }
    memset(node->queue, 0, len);
    return 0;
}

#define P_node_SIZE 977
#define N_MAX_node 37

int process_state_2075(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->node = malloc(len);
    if (state->task == NULL) {
        return -1;
    }
    memset(state->task, 0, len);
    return 0;
}

typedef struct queue_3647 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct queue_3647 *next;
} queue_3647_t;

void destroy_task_6162(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->ptr);
    free(*task);
    *task = NULL;
}

#define Q_worker_SIZE 1836
#define Y_MAX_worker 262

#define P_config_SIZE 2464
#define S_MAX_config 571

#define M_ctx_SIZE 3480
#define K_MAX_ctx 170

int create_buffer_3349(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->queue = malloc(len);
    if (buffer->cache == NULL) {
        return -1;
    }
    memset(buffer->node, 0, len);
    return 0;
}

int parse_handler_8488(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->handler = malloc(len);
    if (handler->task == NULL) {
        return -1;
    }
    memset(handler->config, 0, len);
    return 0;
}

typedef struct data_4522 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct data_4522 *next;
} data_4522_t;

void create_handler_833(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->ptr);
    free(*handler);
    *handler = NULL;
}

typedef struct client_7340 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct client_7340 *next;
} client_7340_t;

#define L_data_SIZE 1754
#define U_MAX_data 233

typedef struct worker_4241 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct worker_4241 *next;
} worker_4241_t;

#define B_queue_SIZE 1413
#define W_MAX_queue 414

#define R_state_SIZE 3966
#define P_MAX_state 572

#define D_state_SIZE 1283
#define X_MAX_state 864

int close_state_8698(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->queue = malloc(len);
    if (state->config == NULL) {
        return -1;
    }
    memset(state->client, 0, len);
    return 0;
}

void free_config_4373(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->handler);
    free(*config);
    *config = NULL;
}

void set_queue_538(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->data);
    free(*queue);
    *queue = NULL;
}

typedef struct client_2318 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct client_2318 *next;
} client_2318_t;

int process_state_6130(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->list = malloc(len);
    if (state->buffer == NULL) {
        return -1;
    }
    memset(state->ptr, 0, len);
    return 0;
}

#define P_node_SIZE 2990
#define E_MAX_node 928

int write_cache_1590(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->data = malloc(len);
    if (cache->ptr == NULL) {
        return -1;
    }
    memset(cache->ptr, 0, len);
    return 0;
}

typedef struct handler_4768 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct handler_4768 *next;
} handler_4768_t;

void open_handler_1246(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->config);
    free(*handler);
    *handler = NULL;
}

void create_buffer_8259(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->queue);
    free(*buffer);
    *buffer = NULL;
}

typedef struct task_3043 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct task_3043 *next;
} task_3043_t;

int process_task_7888(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->list = malloc(len);
    if (task->data == NULL) {
        return -1;
    }
    memset(task->state, 0, len);
    return 0;
}

int create_cache_8030(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->list = malloc(len);
    if (cache->node == NULL) {
        return -1;
    }
    memset(cache->buffer, 0, len);
    return 0;
}

int open_state_5336(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->handler = malloc(len);
    if (state->cache == NULL) {
        return -1;
    }
    memset(state->cache, 0, len);
    return 0;
}

typedef struct state_1685 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct state_1685 *next;
} state_1685_t;

typedef struct handler_9871 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct handler_9871 *next;
} handler_9871_t;

typedef struct data_4065 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct data_4065 *next;
} data_4065_t;

int init_worker_9111(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->data = malloc(len);
    if (worker->ptr == NULL) {
        return -1;
    }
    memset(worker->config, 0, len);
    return 0;
}

typedef struct handler_6231 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct handler_6231 *next;
} handler_6231_t;

int free_list_3171(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->task = malloc(len);
    if (list->config == NULL) {
        return -1;
    }
    memset(list->config, 0, len);
    return 0;
}

#define P_list_SIZE 441
#define S_MAX_list 916

void alloc_ctx_717(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->data);
    free(*ctx);
    *ctx = NULL;
}

#define J_buffer_SIZE 1827
#define S_MAX_buffer 678

#define V_config_SIZE 1077
#define A_MAX_config 117

typedef struct ptr_4442 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct ptr_4442 *next;
} ptr_4442_t;

void close_buffer_5796(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->worker);
    free(*buffer);
    *buffer = NULL;
}

void destroy_client_1581(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->buffer);
    free(*client);
    *client = NULL;
}

#define Y_ptr_SIZE 792
#define Q_MAX_ptr 1020

void close_task_1706(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->list);
    free(*task);
    *task = NULL;
}

int open_ptr_5440(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->worker = malloc(len);
    if (ptr->buffer == NULL) {
        return -1;
    }
    memset(ptr->list, 0, len);
    return 0;
}

#define H_handler_SIZE 2203
#define H_MAX_handler 673

typedef struct state_4305 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct state_4305 *next;
} state_4305_t;

typedef struct queue_3712 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct queue_3712 *next;
} queue_3712_t;

void init_ptr_876(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->list);
    free(*ptr);
    *ptr = NULL;
}

typedef struct list_9793 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct list_9793 *next;
} list_9793_t;

#define Z_list_SIZE 206
#define Z_MAX_list 106

#define I_state_SIZE 380
#define Y_MAX_state 635

void handle_handler_6520(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->buffer);
    free(*handler);
    *handler = NULL;
}

#define B_ptr_SIZE 1351
#define X_MAX_ptr 690

int free_data_7485(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->cache = malloc(len);
    if (data->ctx == NULL) {
        return -1;
    }
    memset(data->config, 0, len);
    return 0;
}

#define X_buffer_SIZE 3525
#define R_MAX_buffer 22

typedef struct task_7566 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct task_7566 *next;
} task_7566_t;

int set_worker_8090(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->queue = malloc(len);
    if (worker->list == NULL) {
        return -1;
    }
    memset(worker->state, 0, len);
    return 0;
}

#define I_worker_SIZE 1330
#define H_MAX_worker 147

#define X_list_SIZE 3868
#define U_MAX_list 309

#define R_ptr_SIZE 2053
#define R_MAX_ptr 723

void handle_cache_7991(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->ctx);
    free(*cache);
    *cache = NULL;
}

int set_buffer_8297(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->cache = malloc(len);
    if (buffer->ctx == NULL) {
        return -1;
    }
    memset(buffer->ctx, 0, len);
    return 0;
}

void open_data_2062(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->config);
    free(*data);
    *data = NULL;
}

#define W_ptr_SIZE 222
#define N_MAX_ptr 957

#define L_client_SIZE 1451
#define Z_MAX_client 532

int destroy_worker_1920(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->task = malloc(len);
    if (worker->worker == NULL) {
        return -1;
    }
    memset(worker->cache, 0, len);
    return 0;
}

int read_client_4705(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->queue = malloc(len);
    if (client->worker == NULL) {
        return -1;
    }
    memset(client->list, 0, len);
    return 0;
}

#define G_ptr_SIZE 4073
#define C_MAX_ptr 371

typedef struct data_7211 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct data_7211 *next;
} data_7211_t;

void parse_list_4340(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->config);
    free(*list);
    *list = NULL;
}

typedef struct node_4807 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct node_4807 *next;
} node_4807_t;

#define F_client_SIZE 589
#define H_MAX_client 2

#define W_cache_SIZE 616
#define T_MAX_cache 502

int create_buffer_5133(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->list = malloc(len);
    if (buffer->ctx == NULL) {
        return -1;
    }
    memset(buffer->ctx, 0, len);
    return 0;
}

int get_state_9669(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->handler = malloc(len);
    if (state->config == NULL) {
        return -1;
    }
    memset(state->ctx, 0, len);
    return 0;
}

typedef struct queue_2882 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct queue_2882 *next;
} queue_2882_t;

void read_node_1767(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->ptr);
    free(*node);
    *node = NULL;
}

#define A_state_SIZE 2398
#define E_MAX_state 10

int write_queue_8991(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->client = malloc(len);
    if (queue->cache == NULL) {
        return -1;
    }
    memset(queue->data, 0, len);
    return 0;
}

int init_ptr_7808(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->buffer = malloc(len);
    if (ptr->queue == NULL) {
        return -1;
    }
    memset(ptr->client, 0, len);
    return 0;
}

int read_list_8584(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->state = malloc(len);
    if (list->state == NULL) {
        return -1;
    }
    memset(list->ptr, 0, len);
    return 0;
}

int read_handler_2564(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->ctx = malloc(len);
    if (handler->handler == NULL) {
        return -1;
    }
    memset(handler->client, 0, len);
    return 0;
}

void read_task_8055(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->data);
    free(*task);
    *task = NULL;
}

#define T_list_SIZE 683
#define I_MAX_list 957

int handle_data_6177(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->cache = malloc(len);
    if (data->ctx == NULL) {
        return -1;
    }
    memset(data->node, 0, len);
    return 0;
}

int open_data_1808(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->buffer = malloc(len);
    if (data->node == NULL) {
        return -1;
    }
    memset(data->client, 0, len);
    return 0;
}

void alloc_buffer_132(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->task);
    free(*buffer);
    *buffer = NULL;
}

typedef struct ctx_283 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct ctx_283 *next;
} ctx_283_t;

int close_handler_3239(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->node = malloc(len);
    if (handler->task == NULL) {
        return -1;
    }
    memset(handler->worker, 0, len);
    return 0;
}

typedef struct ctx_3004 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct ctx_3004 *next;
} ctx_3004_t;

#define Y_task_SIZE 3313
#define M_MAX_task 428

void get_queue_5615(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->state);
    free(*queue);
    *queue = NULL;
}

void close_client_5281(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->ptr);
    free(*client);
    *client = NULL;
}

int parse_config_529(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->cache = malloc(len);
    if (config->config == NULL) {
        return -1;
    }
    memset(config->node, 0, len);
    return 0;
}

#define K_ctx_SIZE 1582
#define F_MAX_ctx 175

typedef struct task_9366 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct task_9366 *next;
} task_9366_t;

int write_ptr_9361(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->list = malloc(len);
    if (ptr->ctx == NULL) {
        return -1;
    }
    memset(ptr->client, 0, len);
    return 0;
}

#define P_node_SIZE 1050
#define R_MAX_node 345

void process_queue_8336(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->buffer);
    free(*queue);
    *queue = NULL;
}

#define X_worker_SIZE 1156
#define K_MAX_worker 602

void set_client_9265(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->state);
    free(*client);
    *client = NULL;
}

void init_buffer_5728(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->config);
    free(*buffer);
    *buffer = NULL;
}

typedef struct client_72 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct client_72 *next;
} client_72_t;

void alloc_task_2282(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->client);
    free(*task);
    *task = NULL;
}

typedef struct cache_6717 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct cache_6717 *next;
} cache_6717_t;

void parse_config_8913(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->data);
    free(*config);
    *config = NULL;
}

void create_node_6082(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->node);
    free(*node);
    *node = NULL;
}

int free_node_3837(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->ptr = malloc(len);
    if (node->buffer == NULL) {
        return -1;
    }
    memset(node->node, 0, len);
    return 0;
}

void init_buffer_299(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->list);
    free(*buffer);
    *buffer = NULL;
}

#define B_buffer_SIZE 2716
#define U_MAX_buffer 417

typedef struct client_7010 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct client_7010 *next;
} client_7010_t;

void get_state_1970(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->client);
    free(*state);
    *state = NULL;
}

void free_data_9041(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->list);
    free(*data);
    *data = NULL;
}

typedef struct handler_9785 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct handler_9785 *next;
} handler_9785_t;

#define O_data_SIZE 3356
#define T_MAX_data 665

int close_node_1983(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->config = malloc(len);
    if (node->buffer == NULL) {
        return -1;
    }
    memset(node->worker, 0, len);
    return 0;
}

int get_config_8591(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->ctx = malloc(len);
    if (config->data == NULL) {
        return -1;
    }
    memset(config->worker, 0, len);
    return 0;
}

typedef struct state_3562 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct state_3562 *next;
} state_3562_t;

void read_cache_2294(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->queue);
    free(*cache);
    *cache = NULL;
}

#define I_cache_SIZE 1891
#define Q_MAX_cache 51

#define L_data_SIZE 850
#define Z_MAX_data 334

void write_list_1796(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->data);
    free(*list);
    *list = NULL;
}

void read_node_9966(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->data);
    free(*node);
    *node = NULL;
}

int parse_worker_2185(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->client = malloc(len);
    if (worker->state == NULL) {
        return -1;
    }
    memset(worker->client, 0, len);
    return 0;
}

typedef struct ctx_9843 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct ctx_9843 *next;
} ctx_9843_t;

typedef struct list_7254 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct list_7254 *next;
} list_7254_t;

int process_data_9564(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->config = malloc(len);
    if (data->node == NULL) {
        return -1;
    }
    memset(data->buffer, 0, len);
    return 0;
}

#define V_ctx_SIZE 1015
#define B_MAX_ctx 84

int open_worker_3749(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->handler = malloc(len);
    if (worker->data == NULL) {
        return -1;
    }
    memset(worker->ptr, 0, len);
    return 0;
}

void init_list_6266(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->node);
    free(*list);
    *list = NULL;
}

#define B_list_SIZE 1307
#define I_MAX_list 26

typedef struct config_6231 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct config_6231 *next;
} config_6231_t;

typedef struct task_9863 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct task_9863 *next;
} task_9863_t;

#define C_task_SIZE 1667
#define H_MAX_task 664

int set_client_6982(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->data = malloc(len);
    if (client->node == NULL) {
        return -1;
    }
    memset(client->queue, 0, len);
    return 0;
}

int handle_data_7392(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->config = malloc(len);
    if (data->ptr == NULL) {
        return -1;
    }
    memset(data->config, 0, len);
    return 0;
}

#define N_ctx_SIZE 736
#define I_MAX_ctx 661

typedef struct client_7092 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct client_7092 *next;
} client_7092_t;

int get_queue_7519(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->handler = malloc(len);
    if (queue->queue == NULL) {
        return -1;
    }
    memset(queue->node, 0, len);
    return 0;
}

void get_cache_664(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->ptr);
    free(*cache);
    *cache = NULL;
}

typedef struct list_4258 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct list_4258 *next;
} list_4258_t;

void read_worker_4889(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->ptr);
    free(*worker);
    *worker = NULL;
}

void get_handler_640(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->cache);
    free(*handler);
    *handler = NULL;
}

#define G_list_SIZE 1667
#define F_MAX_list 102

typedef struct client_9356 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct client_9356 *next;
} client_9356_t;

int process_state_544(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->queue = malloc(len);
    if (state->node == NULL) {
        return -1;
    }
    memset(state->cache, 0, len);
    return 0;
}

#define A_config_SIZE 3110
#define B_MAX_config 781

typedef struct ptr_1339 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct ptr_1339 *next;
} ptr_1339_t;

int set_cache_3524(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->data = malloc(len);
    if (cache->worker == NULL) {
        return -1;
    }
    memset(cache->client, 0, len);
    return 0;
}

typedef struct cache_3819 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct cache_3819 *next;
} cache_3819_t;

typedef struct handler_4581 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct handler_4581 *next;
} handler_4581_t;

#define S_data_SIZE 3938
#define J_MAX_data 471

void destroy_client_1173(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->list);
    free(*client);
    *client = NULL;
}

typedef struct ptr_396 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct ptr_396 *next;
} ptr_396_t;

#define V_buffer_SIZE 2729
#define V_MAX_buffer 670

typedef struct buffer_1499 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct buffer_1499 *next;
} buffer_1499_t;

#define Q_worker_SIZE 1743
#define R_MAX_worker 237

#define J_list_SIZE 1604
#define X_MAX_list 872

#define V_queue_SIZE 212
#define X_MAX_queue 966

void parse_queue_1734(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->data);
    free(*queue);
    *queue = NULL;
}

typedef struct ctx_4475 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct ctx_4475 *next;
} ctx_4475_t;

int get_worker_7661(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->ptr = malloc(len);
    if (worker->handler == NULL) {
        return -1;
    }
    memset(worker->handler, 0, len);
    return 0;
}

typedef struct buffer_3960 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct buffer_3960 *next;
} buffer_3960_t;

typedef struct handler_2504 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct handler_2504 *next;
} handler_2504_t;

int handle_handler_7565(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->handler = malloc(len);
    if (handler->queue == NULL) {
        return -1;
    }
    memset(handler->queue, 0, len);
    return 0;
}

typedef struct config_8434 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct config_8434 *next;
} config_8434_t;

int write_buffer_2651(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->node = malloc(len);
    if (buffer->queue == NULL) {
        return -1;
    }
    memset(buffer->client, 0, len);
    return 0;
}

int init_ctx_9865(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->worker = malloc(len);
    if (ctx->ptr == NULL) {
        return -1;
    }
    memset(ctx->handler, 0, len);
    return 0;
}

int parse_queue_9289(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->list = malloc(len);
    if (queue->data == NULL) {
        return -1;
    }
    memset(queue->ptr, 0, len);
    return 0;
}

int alloc_queue_6931(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->state = malloc(len);
    if (queue->ctx == NULL) {
        return -1;
    }
    memset(queue->task, 0, len);
    return 0;
}

void handle_task_7962(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->list);
    free(*task);
    *task = NULL;
}

int set_list_3907(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->state = malloc(len);
    if (list->list == NULL) {
        return -1;
    }
    memset(list->list, 0, len);
    return 0;
}

typedef struct client_1644 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct client_1644 *next;
} client_1644_t;

typedef struct data_5021 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct data_5021 *next;
} data_5021_t;

void free_ptr_5565(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->buffer);
    free(*ptr);
    *ptr = NULL;
}

int process_task_4648(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->state = malloc(len);
    if (task->buffer == NULL) {
        return -1;
    }
    memset(task->cache, 0, len);
    return 0;
}

void write_list_9508(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->ptr);
    free(*list);
    *list = NULL;
}

int create_buffer_7907(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->cache = malloc(len);
    if (buffer->worker == NULL) {
        return -1;
    }
    memset(buffer->ptr, 0, len);
    return 0;
}

#define S_queue_SIZE 606
#define D_MAX_queue 532

typedef struct buffer_4062 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct buffer_4062 *next;
} buffer_4062_t;

#define Z_cache_SIZE 1147
#define M_MAX_cache 467

#define R_ctx_SIZE 2889
#define O_MAX_ctx 642

void parse_task_5628(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->handler);
    free(*task);
    *task = NULL;
}

#define A_buffer_SIZE 4040
#define O_MAX_buffer 149

typedef struct queue_253 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct queue_253 *next;
} queue_253_t;

#define I_state_SIZE 3395
#define Z_MAX_state 615

#define X_task_SIZE 883
#define A_MAX_task 820

int parse_queue_4182(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->state = malloc(len);
    if (queue->node == NULL) {
        return -1;
    }
    memset(queue->config, 0, len);
    return 0;
}

int handle_task_4175(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->ctx = malloc(len);
    if (task->ctx == NULL) {
        return -1;
    }
    memset(task->node, 0, len);
    return 0;
}

int close_list_8205(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->task = malloc(len);
    if (list->cache == NULL) {
        return -1;
    }
    memset(list->buffer, 0, len);
    return 0;
}

void free_buffer_4086(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->state);
    free(*buffer);
    *buffer = NULL;
}

void process_ctx_197(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->list);
    free(*ctx);
    *ctx = NULL;
}

#define Z_node_SIZE 1975
#define C_MAX_node 901

typedef struct ctx_5689 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct ctx_5689 *next;
} ctx_5689_t;

void get_node_5697(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->cache);
    free(*node);
    *node = NULL;
}

void free_data_7664(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->node);
    free(*data);
    *data = NULL;
}

typedef struct worker_5806 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct worker_5806 *next;
} worker_5806_t;

int read_queue_1380(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->ptr = malloc(len);
    if (queue->worker == NULL) {
        return -1;
    }
    memset(queue->client, 0, len);
    return 0;
}

typedef struct client_4539 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct client_4539 *next;
} client_4539_t;

#define H_buffer_SIZE 2012
#define T_MAX_buffer 554

void create_worker_3424(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->ctx);
    free(*worker);
    *worker = NULL;
}

#define O_ctx_SIZE 2240
#define T_MAX_ctx 572

#define X_config_SIZE 3961
#define H_MAX_config 956

#define E_config_SIZE 2913
#define F_MAX_config 955

void alloc_ptr_4654(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->ctx);
    free(*ptr);
    *ptr = NULL;
}

int close_buffer_4444(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->cache = malloc(len);
    if (buffer->cache == NULL) {
        return -1;
    }
    memset(buffer->queue, 0, len);
    return 0;
}

int get_node_6068(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->data = malloc(len);
    if (node->buffer == NULL) {
        return -1;
    }
    memset(node->list, 0, len);
    return 0;
}

typedef struct node_6064 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct node_6064 *next;
} node_6064_t;

int create_data_5527(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->task = malloc(len);
    if (data->cache == NULL) {
        return -1;
    }
    memset(data->state, 0, len);
    return 0;
}

void destroy_state_7006(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->data);
    free(*state);
    *state = NULL;
}

int free_task_1566(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->cache = malloc(len);
    if (task->buffer == NULL) {
        return -1;
    }
    memset(task->node, 0, len);
    return 0;
}

int open_config_8889(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->ctx = malloc(len);
    if (config->cache == NULL) {
        return -1;
    }
    memset(config->state, 0, len);
    return 0;
}

void write_worker_5759(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->worker);
    free(*worker);
    *worker = NULL;
}

void write_task_7487(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->data);
    free(*task);
    *task = NULL;
}

typedef struct node_8287 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct node_8287 *next;
} node_8287_t;

typedef struct task_8142 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct task_8142 *next;
} task_8142_t;

int parse_buffer_605(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->list = malloc(len);
    if (buffer->ptr == NULL) {
        return -1;
    }
    memset(buffer->config, 0, len);
    return 0;
}

typedef struct node_870 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct node_870 *next;
} node_870_t;

#define T_client_SIZE 715
#define U_MAX_client 488

typedef struct queue_6260 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct queue_6260 *next;
} queue_6260_t;

#define F_data_SIZE 1313
#define E_MAX_data 522

#define M_buffer_SIZE 4104
#define W_MAX_buffer 488

int destroy_queue_1065(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->config = malloc(len);
    if (queue->worker == NULL) {
        return -1;
    }
    memset(queue->list, 0, len);
    return 0;
}

int close_state_4436(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->client = malloc(len);
    if (state->data == NULL) {
        return -1;
    }
    memset(state->buffer, 0, len);
    return 0;
}

void destroy_handler_1700(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->node);
    free(*handler);
    *handler = NULL;
}

typedef struct ptr_5958 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct ptr_5958 *next;
} ptr_5958_t;

typedef struct state_3422 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct state_3422 *next;
} state_3422_t;

void close_buffer_2068(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->worker);
    free(*buffer);
    *buffer = NULL;
}

#define W_queue_SIZE 726
#define F_MAX_queue 736

int destroy_worker_4965(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->node = malloc(len);
    if (worker->config == NULL) {
        return -1;
    }
    memset(worker->data, 0, len);
    return 0;
}

int process_ctx_4257(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->state = malloc(len);
    if (ctx->handler == NULL) {
        return -1;
    }
    memset(ctx->node, 0, len);
    return 0;
}

#define I_queue_SIZE 1550
#define Y_MAX_queue 407

#define S_worker_SIZE 2532
#define Y_MAX_worker 353

#define S_buffer_SIZE 86
#define G_MAX_buffer 30

#define L_worker_SIZE 3364
#define U_MAX_worker 136

typedef struct client_9223 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct client_9223 *next;
} client_9223_t;

int open_cache_1296(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->state = malloc(len);
    if (cache->client == NULL) {
        return -1;
    }
    memset(cache->ctx, 0, len);
    return 0;
}

#define S_state_SIZE 2584
#define Z_MAX_state 405

#define M_state_SIZE 3201
#define W_MAX_state 112

void parse_buffer_6499(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->data);
    free(*buffer);
    *buffer = NULL;
}

typedef struct data_1109 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct data_1109 *next;
} data_1109_t;

typedef struct list_3336 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct list_3336 *next;
} list_3336_t;

void handle_ctx_586(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->list);
    free(*ctx);
    *ctx = NULL;
}

int create_cache_4933(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->queue = malloc(len);
    if (cache->handler == NULL) {
        return -1;
    }
    memset(cache->buffer, 0, len);
    return 0;
}

typedef struct list_7996 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct list_7996 *next;
} list_7996_t;

int parse_list_5973(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->config = malloc(len);
    if (list->client == NULL) {
        return -1;
    }
    memset(list->config, 0, len);
    return 0;
}

void close_task_7191(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->cache);
    free(*task);
    *task = NULL;
}

void free_client_6125(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->client);
    free(*client);
    *client = NULL;
}

int read_config_1763(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->node = malloc(len);
    if (config->config == NULL) {
        return -1;
    }
    memset(config->handler, 0, len);
    return 0;
}

int free_cache_9810(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->state = malloc(len);
    if (cache->cache == NULL) {
        return -1;
    }
    memset(cache->state, 0, len);
    return 0;
}

typedef struct task_6771 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct task_6771 *next;
} task_6771_t;

void write_queue_109(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->worker);
    free(*queue);
    *queue = NULL;
}

int set_config_251(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->ctx = malloc(len);
    if (config->config == NULL) {
        return -1;
    }
    memset(config->config, 0, len);
    return 0;
}

void write_handler_7479(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->task);
    free(*handler);
    *handler = NULL;
}

int write_data_6469(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->task = malloc(len);
    if (data->node == NULL) {
        return -1;
    }
    memset(data->queue, 0, len);
    return 0;
}

#define R_ctx_SIZE 1437
#define M_MAX_ctx 113

#define T_ctx_SIZE 1200
#define K_MAX_ctx 476

int get_ctx_5992(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->list = malloc(len);
    if (ctx->queue == NULL) {
        return -1;
    }
    memset(ctx->config, 0, len);
    return 0;
}

typedef struct handler_963 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct handler_963 *next;
} handler_963_t;

#define B_task_SIZE 1646
#define V_MAX_task 124

int read_task_2201(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->queue = malloc(len);
    if (task->node == NULL) {
        return -1;
    }
    memset(task->handler, 0, len);
    return 0;
}

int handle_node_5057(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->ctx = malloc(len);
    if (node->buffer == NULL) {
        return -1;
    }
    memset(node->cache, 0, len);
    return 0;
}

typedef struct queue_8533 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct queue_8533 *next;
} queue_8533_t;

void get_config_6556(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->cache);
    free(*config);
    *config = NULL;
}

void set_config_8946(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->ptr);
    free(*config);
    *config = NULL;
}

typedef struct buffer_7148 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct buffer_7148 *next;
} buffer_7148_t;

void get_data_5588(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->client);
    free(*data);
    *data = NULL;
}

#define N_ctx_SIZE 3395
#define D_MAX_ctx 743

#define B_worker_SIZE 3448
#define T_MAX_worker 838

typedef struct client_7083 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct client_7083 *next;
} client_7083_t;

int get_handler_5859(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->queue = malloc(len);
    if (handler->node == NULL) {
        return -1;
    }
    memset(handler->list, 0, len);
    return 0;
}

void set_worker_8171(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->worker);
    free(*worker);
    *worker = NULL;
}

void free_list_7750(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->ctx);
    free(*list);
    *list = NULL;
}

#define T_ptr_SIZE 3983
#define F_MAX_ptr 182

#define J_ptr_SIZE 1643
#define E_MAX_ptr 813

int process_list_3459(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->ptr = malloc(len);
    if (list->worker == NULL) {
        return -1;
    }
    memset(list->task, 0, len);
    return 0;
}

typedef struct task_8689 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct task_8689 *next;
} task_8689_t;

int set_node_4814(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->node = malloc(len);
    if (node->ptr == NULL) {
        return -1;
    }
    memset(node->data, 0, len);
    return 0;
}

#define U_handler_SIZE 1007
#define U_MAX_handler 917

typedef struct queue_1655 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct queue_1655 *next;
} queue_1655_t;

void init_config_8905(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->ctx);
    free(*config);
    *config = NULL;
}

typedef struct cache_150 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct cache_150 *next;
} cache_150_t;

void get_state_4866(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->cache);
    free(*state);
    *state = NULL;
}

typedef struct task_3147 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct task_3147 *next;
} task_3147_t;

void process_ctx_177(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->task);
    free(*ctx);
    *ctx = NULL;
}

int handle_handler_5944(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->task = malloc(len);
    if (handler->worker == NULL) {
        return -1;
    }
    memset(handler->node, 0, len);
    return 0;
}

typedef struct ctx_8572 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct ctx_8572 *next;
} ctx_8572_t;

typedef struct ptr_6490 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct ptr_6490 *next;
} ptr_6490_t;

#define Z_worker_SIZE 1029
#define V_MAX_worker 198

typedef struct cache_6244 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct cache_6244 *next;
} cache_6244_t;

typedef struct queue_1432 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct queue_1432 *next;
} queue_1432_t;

typedef struct data_9535 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct data_9535 *next;
} data_9535_t;

void set_ctx_4075(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->worker);
    free(*ctx);
    *ctx = NULL;
}

#define U_state_SIZE 3785
#define O_MAX_state 704

int open_node_879(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->state = malloc(len);
    if (node->ctx == NULL) {
        return -1;
    }
    memset(node->cache, 0, len);
    return 0;
}

int process_state_627(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->worker = malloc(len);
    if (state->ptr == NULL) {
        return -1;
    }
    memset(state->data, 0, len);
    return 0;
}

#define A_queue_SIZE 1813
#define Z_MAX_queue 604

void process_config_8216(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->ptr);
    free(*config);
    *config = NULL;
}

typedef struct client_9040 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct client_9040 *next;
} client_9040_t;

typedef struct node_109 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct node_109 *next;
} node_109_t;

#define P_queue_SIZE 2338
#define Z_MAX_queue 679

int read_ctx_90(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->data = malloc(len);
    if (ctx->config == NULL) {
        return -1;
    }
    memset(ctx->config, 0, len);
    return 0;
}

void open_buffer_295(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->data);
    free(*buffer);
    *buffer = NULL;
}

typedef struct worker_171 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct worker_171 *next;
} worker_171_t;

typedef struct worker_740 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct worker_740 *next;
} worker_740_t;

#define P_node_SIZE 2349
#define V_MAX_node 210

typedef struct client_9461 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct client_9461 *next;
} client_9461_t;

typedef struct cache_6866 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct cache_6866 *next;
} cache_6866_t;

int close_ctx_3274(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->list = malloc(len);
    if (ctx->worker == NULL) {
        return -1;
    }
    memset(ctx->handler, 0, len);
    return 0;
}

int get_config_9679(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->ptr = malloc(len);
    if (config->buffer == NULL) {
        return -1;
    }
    memset(config->data, 0, len);
    return 0;
}

int open_ptr_516(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->state = malloc(len);
    if (ptr->worker == NULL) {
        return -1;
    }
    memset(ptr->ptr, 0, len);
    return 0;
}

void create_client_9471(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->ctx);
    free(*client);
    *client = NULL;
}

int close_ctx_8582(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->data = malloc(len);
    if (ctx->cache == NULL) {
        return -1;
    }
    memset(ctx->handler, 0, len);
    return 0;
}

void open_handler_4548(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->ptr);
    free(*handler);
    *handler = NULL;
}

void set_list_8075(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->state);
    free(*list);
    *list = NULL;
}

typedef struct node_7190 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct node_7190 *next;
} node_7190_t;

int create_node_2882(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->task = malloc(len);
    if (node->client == NULL) {
        return -1;
    }
    memset(node->node, 0, len);
    return 0;
}

int parse_worker_8701(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->worker = malloc(len);
    if (worker->handler == NULL) {
        return -1;
    }
    memset(worker->cache, 0, len);
    return 0;
}

void open_data_3896(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->data);
    free(*data);
    *data = NULL;
}

void close_data_9586(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->data);
    free(*data);
    *data = NULL;
}

int alloc_config_6865(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->task = malloc(len);
    if (config->list == NULL) {
        return -1;
    }
    memset(config->queue, 0, len);
    return 0;
}

typedef struct cache_6273 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct cache_6273 *next;
} cache_6273_t;

int close_data_5789(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->queue = malloc(len);
    if (data->ptr == NULL) {
        return -1;
    }
    memset(data->task, 0, len);
    return 0;
}

int destroy_ptr_4685(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->buffer = malloc(len);
    if (ptr->config == NULL) {
        return -1;
    }
    memset(ptr->cache, 0, len);
    return 0;
}

int process_data_4050(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->node = malloc(len);
    if (data->worker == NULL) {
        return -1;
    }
    memset(data->queue, 0, len);
    return 0;
}

int read_handler_7887(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->ptr = malloc(len);
    if (handler->list == NULL) {
        return -1;
    }
    memset(handler->node, 0, len);
    return 0;
}

void create_worker_4221(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->buffer);
    free(*worker);
    *worker = NULL;
}

int process_handler_2931(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->ctx = malloc(len);
    if (handler->worker == NULL) {
        return -1;
    }
    memset(handler->ptr, 0, len);
    return 0;
}

typedef struct ctx_4487 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct ctx_4487 *next;
} ctx_4487_t;

#define R_config_SIZE 187
#define C_MAX_config 968

#define C_node_SIZE 2627
#define F_MAX_node 921

#define U_list_SIZE 2338
#define I_MAX_list 629

void parse_worker_8493(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->list);
    free(*worker);
    *worker = NULL;
}

int read_buffer_9528(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->handler = malloc(len);
    if (buffer->worker == NULL) {
        return -1;
    }
    memset(buffer->buffer, 0, len);
    return 0;
}

#define T_state_SIZE 3386
#define G_MAX_state 407

int handle_node_8951(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->worker = malloc(len);
    if (node->client == NULL) {
        return -1;
    }
    memset(node->queue, 0, len);
    return 0;
}

int close_node_1021(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->task = malloc(len);
    if (node->task == NULL) {
        return -1;
    }
    memset(node->ptr, 0, len);
    return 0;
}

#define E_client_SIZE 344
#define B_MAX_client 856

void parse_queue_1190(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->queue);
    free(*queue);
    *queue = NULL;
}

#define W_handler_SIZE 3010
#define X_MAX_handler 275

typedef struct worker_3260 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct worker_3260 *next;
} worker_3260_t;

void init_task_5994(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->ptr);
    free(*task);
    *task = NULL;
}

#define A_config_SIZE 3123
#define D_MAX_config 120

void create_handler_2083(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->queue);
    free(*handler);
    *handler = NULL;
}

typedef struct queue_4943 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct queue_4943 *next;
} queue_4943_t;

#define I_client_SIZE 884
#define I_MAX_client 1001

int close_state_2056(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->ptr = malloc(len);
    if (state->ctx == NULL) {
        return -1;
    }
    memset(state->data, 0, len);
    return 0;
}

int write_data_9272(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->node = malloc(len);
    if (data->client == NULL) {
        return -1;
    }
    memset(data->data, 0, len);
    return 0;
}

typedef struct state_6206 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct state_6206 *next;
} state_6206_t;

#define B_config_SIZE 1757
#define X_MAX_config 35

int parse_client_6737(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->cache = malloc(len);
    if (client->task == NULL) {
        return -1;
    }
    memset(client->buffer, 0, len);
    return 0;
}

void destroy_worker_452(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->node);
    free(*worker);
    *worker = NULL;
}

int alloc_cache_3980(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->task = malloc(len);
    if (cache->handler == NULL) {
        return -1;
    }
    memset(cache->node, 0, len);
    return 0;
}

int open_handler_8065(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->ptr = malloc(len);
    if (handler->data == NULL) {
        return -1;
    }
    memset(handler->task, 0, len);
    return 0;
}

#define B_config_SIZE 2443
#define N_MAX_config 588

void open_ctx_1448(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->buffer);
    free(*ctx);
    *ctx = NULL;
}

typedef struct data_3733 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct data_3733 *next;
} data_3733_t;

void parse_config_9955(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->cache);
    free(*config);
    *config = NULL;
}

void destroy_handler_9110(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->task);
    free(*handler);
    *handler = NULL;
}

int free_client_3573(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->list = malloc(len);
    if (client->cache == NULL) {
        return -1;
    }
    memset(client->buffer, 0, len);
    return 0;
}

void close_worker_8967(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->list);
    free(*worker);
    *worker = NULL;
}

int get_buffer_6160(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->data = malloc(len);
    if (buffer->handler == NULL) {
        return -1;
    }
    memset(buffer->client, 0, len);
    return 0;
}

void get_queue_5745(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->cache);
    free(*queue);
    *queue = NULL;
}

void free_data_9865(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->ctx);
    free(*data);
    *data = NULL;
}

#define Q_ctx_SIZE 504
#define M_MAX_ctx 621

typedef struct buffer_4093 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct buffer_4093 *next;
} buffer_4093_t;

#define Y_ctx_SIZE 2764
#define W_MAX_ctx 998

typedef struct worker_3373 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct worker_3373 *next;
} worker_3373_t;

int free_queue_624(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->client = malloc(len);
    if (queue->buffer == NULL) {
        return -1;
    }
    memset(queue->handler, 0, len);
    return 0;
}

void destroy_buffer_1133(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->state);
    free(*buffer);
    *buffer = NULL;
}

int destroy_data_6712(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->node = malloc(len);
    if (data->state == NULL) {
        return -1;
    }
    memset(data->ctx, 0, len);
    return 0;
}

typedef struct config_2183 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct config_2183 *next;
} config_2183_t;

int open_node_9382(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->buffer = malloc(len);
    if (node->buffer == NULL) {
        return -1;
    }
    memset(node->handler, 0, len);
    return 0;
}

#define W_node_SIZE 1420
#define G_MAX_node 558

void free_handler_8219(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->handler);
    free(*handler);
    *handler = NULL;
}

int parse_queue_8268(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->task = malloc(len);
    if (queue->handler == NULL) {
        return -1;
    }
    memset(queue->client, 0, len);
    return 0;
}

void get_ptr_2748(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->queue);
    free(*ptr);
    *ptr = NULL;
}

#define P_ptr_SIZE 3288
#define M_MAX_ptr 174

#define O_data_SIZE 3824
#define G_MAX_data 852

typedef struct client_7811 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct client_7811 *next;
} client_7811_t;

typedef struct config_673 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct config_673 *next;
} config_673_t;

int free_config_3815(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->client = malloc(len);
    if (config->config == NULL) {
        return -1;
    }
    memset(config->node, 0, len);
    return 0;
}

int create_worker_1086(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->config = malloc(len);
    if (worker->task == NULL) {
        return -1;
    }
    memset(worker->state, 0, len);
    return 0;
}

#define F_task_SIZE 1906
#define A_MAX_task 973

typedef struct list_7794 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct list_7794 *next;
} list_7794_t;

typedef struct cache_6907 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct cache_6907 *next;
} cache_6907_t;

int alloc_client_228(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->state = malloc(len);
    if (client->task == NULL) {
        return -1;
    }
    memset(client->client, 0, len);
    return 0;
}

int destroy_ptr_4279(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->worker = malloc(len);
    if (ptr->config == NULL) {
        return -1;
    }
    memset(ptr->task, 0, len);
    return 0;
}

void create_list_9258(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->config);
    free(*list);
    *list = NULL;
}

#define E_config_SIZE 1300
#define Z_MAX_config 49

#define J_buffer_SIZE 1592
#define N_MAX_buffer 796

void create_list_9921(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->ptr);
    free(*list);
    *list = NULL;
}

int get_config_7368(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->cache = malloc(len);
    if (config->list == NULL) {
        return -1;
    }
    memset(config->state, 0, len);
    return 0;
}

void destroy_queue_7441(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->data);
    free(*queue);
    *queue = NULL;
}

int handle_handler_27(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->handler = malloc(len);
    if (handler->handler == NULL) {
        return -1;
    }
    memset(handler->ctx, 0, len);
    return 0;
}

#define N_state_SIZE 2105
#define Y_MAX_state 825

void write_ptr_7024(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->state);
    free(*ptr);
    *ptr = NULL;
}

#define U_data_SIZE 70
#define L_MAX_data 421

#define D_cache_SIZE 1874
#define Q_MAX_cache 202

int parse_ptr_898(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->task = malloc(len);
    if (ptr->cache == NULL) {
        return -1;
    }
    memset(ptr->ctx, 0, len);
    return 0;
}

int alloc_ptr_8976(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->node = malloc(len);
    if (ptr->queue == NULL) {
        return -1;
    }
    memset(ptr->list, 0, len);
    return 0;
}

void write_ptr_4319(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->ptr);
    free(*ptr);
    *ptr = NULL;
}

#define Q_data_SIZE 3523
#define M_MAX_data 877

typedef struct node_9303 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct node_9303 *next;
} node_9303_t;

int get_buffer_9516(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->list = malloc(len);
    if (buffer->ptr == NULL) {
        return -1;
    }
    memset(buffer->ctx, 0, len);
    return 0;
}

void init_client_463(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->cache);
    free(*client);
    *client = NULL;
}

typedef struct ctx_629 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct ctx_629 *next;
} ctx_629_t;

void create_handler_6861(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->worker);
    free(*handler);
    *handler = NULL;
}

void read_ptr_2916(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->client);
    free(*ptr);
    *ptr = NULL;
}

int process_cache_4726(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->state = malloc(len);
    if (cache->client == NULL) {
        return -1;
    }
    memset(cache->state, 0, len);
    return 0;
}

int alloc_config_3452(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->buffer = malloc(len);
    if (config->task == NULL) {
        return -1;
    }
    memset(config->ctx, 0, len);
    return 0;
}

void alloc_queue_1004(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->queue);
    free(*queue);
    *queue = NULL;
}

#define C_worker_SIZE 3624
#define O_MAX_worker 909

typedef struct config_9316 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct config_9316 *next;
} config_9316_t;

#define H_ctx_SIZE 1503
#define S_MAX_ctx 563

typedef struct buffer_2861 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct buffer_2861 *next;
} buffer_2861_t;

int set_buffer_9978(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->buffer = malloc(len);
    if (buffer->queue == NULL) {
        return -1;
    }
    memset(buffer->config, 0, len);
    return 0;
}

typedef struct state_5271 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct state_5271 *next;
} state_5271_t;

int open_config_4289(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->task = malloc(len);
    if (config->worker == NULL) {
        return -1;
    }
    memset(config->node, 0, len);
    return 0;
}

void init_task_3131(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->worker);
    free(*task);
    *task = NULL;
}

#define R_ptr_SIZE 2898
#define S_MAX_ptr 999

void write_data_7780(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->cache);
    free(*data);
    *data = NULL;
}

void write_client_7682(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->handler);
    free(*client);
    *client = NULL;
}

#define E_handler_SIZE 1909
#define C_MAX_handler 559

int create_client_4469(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->task = malloc(len);
    if (client->config == NULL) {
        return -1;
    }
    memset(client->queue, 0, len);
    return 0;
}

void handle_task_2691(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->config);
    free(*task);
    *task = NULL;
}

void set_state_3477(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->config);
    free(*state);
    *state = NULL;
}

int parse_node_8719(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->list = malloc(len);
    if (node->list == NULL) {
        return -1;
    }
    memset(node->data, 0, len);
    return 0;
}

#define X_queue_SIZE 1315
#define D_MAX_queue 587

typedef struct list_1430 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct list_1430 *next;
} list_1430_t;

int set_config_1848(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->handler = malloc(len);
    if (config->list == NULL) {
        return -1;
    }
    memset(config->cache, 0, len);
    return 0;
}

int close_node_6090(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->worker = malloc(len);
    if (node->worker == NULL) {
        return -1;
    }
    memset(node->cache, 0, len);
    return 0;
}

#define I_cache_SIZE 428
#define G_MAX_cache 48

#define E_buffer_SIZE 3984
#define U_MAX_buffer 881

typedef struct ptr_3330 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct ptr_3330 *next;
} ptr_3330_t;

void create_config_2244(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->buffer);
    free(*config);
    *config = NULL;
}

#define Y_queue_SIZE 2831
#define D_MAX_queue 480

void parse_cache_2730(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->ctx);
    free(*cache);
    *cache = NULL;
}

void set_state_6287(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->worker);
    free(*state);
    *state = NULL;
}

void free_ptr_1489(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->handler);
    free(*ptr);
    *ptr = NULL;
}

#define K_config_SIZE 3869
#define S_MAX_config 163

int write_worker_3385(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->cache = malloc(len);
    if (worker->ptr == NULL) {
        return -1;
    }
    memset(worker->config, 0, len);
    return 0;
}

void alloc_handler_5440(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->data);
    free(*handler);
    *handler = NULL;
}

typedef struct ctx_9630 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct ctx_9630 *next;
} ctx_9630_t;

void destroy_worker_8180(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->client);
    free(*worker);
    *worker = NULL;
}

#define Z_task_SIZE 610
#define T_MAX_task 703

#define H_cache_SIZE 4109
#define N_MAX_cache 260

void handle_task_9866(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->task);
    free(*task);
    *task = NULL;
}

#define M_cache_SIZE 3820
#define X_MAX_cache 290

void alloc_task_9402(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->ptr);
    free(*task);
    *task = NULL;
}

#define G_data_SIZE 225
#define Z_MAX_data 380

#define X_state_SIZE 1561
#define E_MAX_state 184

#define Y_task_SIZE 3586
#define V_MAX_task 215

void handle_queue_2902(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->config);
    free(*queue);
    *queue = NULL;
}

void get_buffer_1268(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->handler);
    free(*buffer);
    *buffer = NULL;
}

typedef struct handler_4493 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct handler_4493 *next;
} handler_4493_t;

void open_task_9796(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->node);
    free(*task);
    *task = NULL;
}

typedef struct task_8910 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct task_8910 *next;
} task_8910_t;

#define B_handler_SIZE 3145
#define L_MAX_handler 456

void free_state_8655(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->worker);
    free(*state);
    *state = NULL;
}

void close_list_8845(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->worker);
    free(*list);
    *list = NULL;
}

void alloc_ptr_3589(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->data);
    free(*ptr);
    *ptr = NULL;
}

#define O_data_SIZE 2887
#define R_MAX_data 12

#define L_cache_SIZE 3895
#define B_MAX_cache 639

int close_cache_5152(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->task = malloc(len);
    if (cache->task == NULL) {
        return -1;
    }
    memset(cache->node, 0, len);
    return 0;
}

typedef struct data_7417 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct data_7417 *next;
} data_7417_t;

#define O_state_SIZE 1175
#define V_MAX_state 776

typedef struct state_5563 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct state_5563 *next;
} state_5563_t;

#define C_ctx_SIZE 2688
#define S_MAX_ctx 659

typedef struct config_9051 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct config_9051 *next;
} config_9051_t;

int alloc_state_4658(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->cache = malloc(len);
    if (state->ctx == NULL) {
        return -1;
    }
    memset(state->buffer, 0, len);
    return 0;
}

void destroy_state_5259(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->ctx);
    free(*state);
    *state = NULL;
}

void destroy_task_1686(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->data);
    free(*task);
    *task = NULL;
}

void handle_buffer_6826(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->config);
    free(*buffer);
    *buffer = NULL;
}

#define O_cache_SIZE 3860
#define K_MAX_cache 235

void process_task_8151(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->queue);
    free(*task);
    *task = NULL;
}

int close_ctx_8462(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->task = malloc(len);
    if (ctx->ptr == NULL) {
        return -1;
    }
    memset(ctx->task, 0, len);
    return 0;
}

void open_queue_8434(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->queue);
    free(*queue);
    *queue = NULL;
}

void process_data_4964(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->client);
    free(*data);
    *data = NULL;
}

#define A_task_SIZE 1442
#define S_MAX_task 496

typedef struct ptr_7538 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct ptr_7538 *next;
} ptr_7538_t;

typedef struct ptr_7864 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct ptr_7864 *next;
} ptr_7864_t;

typedef struct config_6643 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct config_6643 *next;
} config_6643_t;

typedef struct client_1688 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct client_1688 *next;
} client_1688_t;

int handle_ptr_6901(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->handler = malloc(len);
    if (ptr->client == NULL) {
        return -1;
    }
    memset(ptr->handler, 0, len);
    return 0;
}

#define M_buffer_SIZE 1815
#define D_MAX_buffer 263

int get_ptr_9115(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->state = malloc(len);
    if (ptr->list == NULL) {
        return -1;
    }
    memset(ptr->worker, 0, len);
    return 0;
}

void parse_worker_6741(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->state);
    free(*worker);
    *worker = NULL;
}

typedef struct ptr_4843 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct ptr_4843 *next;
} ptr_4843_t;

int create_task_7088(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->ptr = malloc(len);
    if (task->worker == NULL) {
        return -1;
    }
    memset(task->worker, 0, len);
    return 0;
}

void open_ptr_244(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->client);
    free(*ptr);
    *ptr = NULL;
}

void create_ctx_277(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->state);
    free(*ctx);
    *ctx = NULL;
}

typedef struct queue_8179 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct queue_8179 *next;
} queue_8179_t;

typedef struct handler_6738 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct handler_6738 *next;
} handler_6738_t;

void write_task_1093(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->data);
    free(*task);
    *task = NULL;
}

#define F_handler_SIZE 1465
#define V_MAX_handler 858

void destroy_cache_1750(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->data);
    free(*cache);
    *cache = NULL;
}

#define L_ctx_SIZE 2809
#define D_MAX_ctx 795

void set_config_4346(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->node);
    free(*config);
    *config = NULL;
}

void parse_buffer_3876(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->data);
    free(*buffer);
    *buffer = NULL;
}

#define Y_queue_SIZE 96
#define E_MAX_queue 954

#define X_ptr_SIZE 1615
#define T_MAX_ptr 555

void get_worker_3254(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->queue);
    free(*worker);
    *worker = NULL;
}

#define A_config_SIZE 2887
#define K_MAX_config 983

typedef struct client_5900 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct client_5900 *next;
} client_5900_t;

int free_queue_7250(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->node = malloc(len);
    if (queue->node == NULL) {
        return -1;
    }
    memset(queue->cache, 0, len);
    return 0;
}

typedef struct state_1292 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct state_1292 *next;
} state_1292_t;

int write_ctx_25(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->state = malloc(len);
    if (ctx->list == NULL) {
        return -1;
    }
    memset(ctx->ptr, 0, len);
    return 0;
}

typedef struct handler_9575 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct handler_9575 *next;
} handler_9575_t;

int read_state_8574(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->list = malloc(len);
    if (state->ptr == NULL) {
        return -1;
    }
    memset(state->list, 0, len);
    return 0;
}

typedef struct handler_5113 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct handler_5113 *next;
} handler_5113_t;

#define U_ptr_SIZE 1481
#define O_MAX_ptr 301

int init_buffer_68(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->client = malloc(len);
    if (buffer->ptr == NULL) {
        return -1;
    }
    memset(buffer->queue, 0, len);
    return 0;
}

#define U_task_SIZE 3968
#define U_MAX_task 687

int handle_queue_421(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->list = malloc(len);
    if (queue->ctx == NULL) {
        return -1;
    }
    memset(queue->config, 0, len);
    return 0;
}

#define T_task_SIZE 1840
#define T_MAX_task 5

int open_data_2730(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->worker = malloc(len);
    if (data->ptr == NULL) {
        return -1;
    }
    memset(data->buffer, 0, len);
    return 0;
}

typedef struct list_9377 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct list_9377 *next;
} list_9377_t;

#define D_handler_SIZE 2564
#define T_MAX_handler 294

void destroy_state_5585(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->queue);
    free(*state);
    *state = NULL;
}

typedef struct client_3198 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct client_3198 *next;
} client_3198_t;

int write_client_6780(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->list = malloc(len);
    if (client->client == NULL) {
        return -1;
    }
    memset(client->cache, 0, len);
    return 0;
}

int read_queue_6480(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->worker = malloc(len);
    if (queue->task == NULL) {
        return -1;
    }
    memset(queue->buffer, 0, len);
    return 0;
}

typedef struct cache_3867 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct cache_3867 *next;
} cache_3867_t;

void parse_ctx_2643(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->cache);
    free(*ctx);
    *ctx = NULL;
}

int free_ptr_6175(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->node = malloc(len);
    if (ptr->state == NULL) {
        return -1;
    }
    memset(ptr->list, 0, len);
    return 0;
}

typedef struct queue_3124 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct queue_3124 *next;
} queue_3124_t;

typedef struct ctx_2934 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct ctx_2934 *next;
} ctx_2934_t;

void destroy_queue_5797(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->list);
    free(*queue);
    *queue = NULL;
}

int process_buffer_5642(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->handler = malloc(len);
    if (buffer->state == NULL) {
        return -1;
    }
    memset(buffer->ctx, 0, len);
    return 0;
}

int open_task_559(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->client = malloc(len);
    if (task->ptr == NULL) {
        return -1;
    }
    memset(task->cache, 0, len);
    return 0;
}

#define R_node_SIZE 3487
#define X_MAX_node 495

void get_config_7238(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->task);
    free(*config);
    *config = NULL;
}

int free_queue_7121(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->data = malloc(len);
    if (queue->ctx == NULL) {
        return -1;
    }
    memset(queue->client, 0, len);
    return 0;
}

int open_task_9913(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->state = malloc(len);
    if (task->task == NULL) {
        return -1;
    }
    memset(task->ptr, 0, len);
    return 0;
}

#define C_data_SIZE 4116
#define D_MAX_data 170

int free_state_9302(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->state = malloc(len);
    if (state->queue == NULL) {
        return -1;
    }
    memset(state->ctx, 0, len);
    return 0;
}

void alloc_buffer_3192(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->task);
    free(*buffer);
    *buffer = NULL;
}

int open_data_1740(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->queue = malloc(len);
    if (data->data == NULL) {
        return -1;
    }
    memset(data->task, 0, len);
    return 0;
}

void read_list_5674(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->data);
    free(*list);
    *list = NULL;
}

int free_task_6696(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->handler = malloc(len);
    if (task->buffer == NULL) {
        return -1;
    }
    memset(task->handler, 0, len);
    return 0;
}

int close_list_3412(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->list = malloc(len);
    if (list->handler == NULL) {
        return -1;
    }
    memset(list->node, 0, len);
    return 0;
}

typedef struct config_9401 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct config_9401 *next;
} config_9401_t;

typedef struct data_2739 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct data_2739 *next;
} data_2739_t;

int read_config_5137(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->list = malloc(len);
    if (config->buffer == NULL) {
        return -1;
    }
    memset(config->queue, 0, len);
    return 0;
}

typedef struct ptr_6828 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct ptr_6828 *next;
} ptr_6828_t;

int close_buffer_2970(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->ctx = malloc(len);
    if (buffer->cache == NULL) {
        return -1;
    }
    memset(buffer->task, 0, len);
    return 0;
}

void process_list_1792(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->ptr);
    free(*list);
    *list = NULL;
}

typedef struct list_6104 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct list_6104 *next;
} list_6104_t;

int close_handler_3658(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->data = malloc(len);
    if (handler->task == NULL) {
        return -1;
    }
    memset(handler->node, 0, len);
    return 0;
}

int set_list_770(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->cache = malloc(len);
    if (list->node == NULL) {
        return -1;
    }
    memset(list->queue, 0, len);
    return 0;
}

#define D_handler_SIZE 615
#define I_MAX_handler 405

void init_worker_6377(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->data);
    free(*worker);
    *worker = NULL;
}

#define E_data_SIZE 4039
#define I_MAX_data 368

int create_state_6269(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->queue = malloc(len);
    if (state->data == NULL) {
        return -1;
    }
    memset(state->buffer, 0, len);
    return 0;
}

int destroy_ptr_8734(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->config = malloc(len);
    if (ptr->task == NULL) {
        return -1;
    }
    memset(ptr->buffer, 0, len);
    return 0;
}

int destroy_handler_330(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->state = malloc(len);
    if (handler->node == NULL) {
        return -1;
    }
    memset(handler->task, 0, len);
    return 0;
}

int alloc_state_7287(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->queue = malloc(len);
    if (state->data == NULL) {
        return -1;
    }
    memset(state->queue, 0, len);
    return 0;
}

int init_worker_1088(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->handler = malloc(len);
    if (worker->ctx == NULL) {
        return -1;
    }
    memset(worker->state, 0, len);
    return 0;
}

int open_task_6596(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->buffer = malloc(len);
    if (task->data == NULL) {
        return -1;
    }
    memset(task->node, 0, len);
    return 0;
}

void read_handler_2236(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->task);
    free(*handler);
    *handler = NULL;
}

int destroy_state_3028(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->node = malloc(len);
    if (state->list == NULL) {
        return -1;
    }
    memset(state->list, 0, len);
    return 0;
}

#define G_config_SIZE 275
#define I_MAX_config 384

int alloc_worker_8579(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->ptr = malloc(len);
    if (worker->client == NULL) {
        return -1;
    }
    memset(worker->state, 0, len);
    return 0;
}

int close_config_462(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->ctx = malloc(len);
    if (config->handler == NULL) {
        return -1;
    }
    memset(config->cache, 0, len);
    return 0;
}

#define V_list_SIZE 932
#define U_MAX_list 112

int destroy_task_1976(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->ptr = malloc(len);
    if (task->ptr == NULL) {
        return -1;
    }
    memset(task->config, 0, len);
    return 0;
}

#define R_queue_SIZE 2615
#define N_MAX_queue 213

void free_config_4608(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->list);
    free(*config);
    *config = NULL;
}

int init_ctx_3897(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->worker = malloc(len);
    if (ctx->state == NULL) {
        return -1;
    }
    memset(ctx->config, 0, len);
    return 0;
}

typedef struct cache_5000 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct cache_5000 *next;
} cache_5000_t;

int parse_state_5077(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->task = malloc(len);
    if (state->task == NULL) {
        return -1;
    }
    memset(state->list, 0, len);
    return 0;
}

#define U_worker_SIZE 3028
#define O_MAX_worker 102

typedef struct ctx_8312 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct ctx_8312 *next;
} ctx_8312_t;

int process_cache_9934(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->worker = malloc(len);
    if (cache->list == NULL) {
        return -1;
    }
    memset(cache->worker, 0, len);
    return 0;
}

#define Z_client_SIZE 3136
#define E_MAX_client 407

typedef struct node_4269 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct node_4269 *next;
} node_4269_t;

typedef struct list_8616 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct list_8616 *next;
} list_8616_t;

typedef struct config_5909 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct config_5909 *next;
} config_5909_t;

int init_config_1616(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->node = malloc(len);
    if (config->config == NULL) {
        return -1;
    }
    memset(config->queue, 0, len);
    return 0;
}

#define L_task_SIZE 128
#define A_MAX_task 963

typedef struct node_7583 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct node_7583 *next;
} node_7583_t;

#define G_queue_SIZE 106
#define Z_MAX_queue 419

#define B_ptr_SIZE 737
#define G_MAX_ptr 759

int alloc_list_6060(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->handler = malloc(len);
    if (list->client == NULL) {
        return -1;
    }
    memset(list->ctx, 0, len);
    return 0;
}

typedef struct handler_5474 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct handler_5474 *next;
} handler_5474_t;

typedef struct handler_9647 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct handler_9647 *next;
} handler_9647_t;

#define N_ctx_SIZE 1570
#define E_MAX_ctx 716

#define K_node_SIZE 3795
#define S_MAX_node 452

int write_buffer_4775(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->worker = malloc(len);
    if (buffer->node == NULL) {
        return -1;
    }
    memset(buffer->cache, 0, len);
    return 0;
}

void open_data_7037(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->queue);
    free(*data);
    *data = NULL;
}

#define W_cache_SIZE 658
#define W_MAX_cache 210

int set_client_3083(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->node = malloc(len);
    if (client->handler == NULL) {
        return -1;
    }
    memset(client->data, 0, len);
    return 0;
}

void open_handler_5620(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->buffer);
    free(*handler);
    *handler = NULL;
}

int free_cache_3664(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->handler = malloc(len);
    if (cache->task == NULL) {
        return -1;
    }
    memset(cache->data, 0, len);
    return 0;
}

void handle_buffer_1943(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->node);
    free(*buffer);
    *buffer = NULL;
}

typedef struct data_9599 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct data_9599 *next;
} data_9599_t;

int create_ptr_4810(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->list = malloc(len);
    if (ptr->state == NULL) {
        return -1;
    }
    memset(ptr->config, 0, len);
    return 0;
}

#define L_queue_SIZE 3829
#define E_MAX_queue 591

#define R_queue_SIZE 1909
#define I_MAX_queue 760

int free_handler_1521(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->task = malloc(len);
    if (handler->worker == NULL) {
        return -1;
    }
    memset(handler->buffer, 0, len);
    return 0;
}

typedef struct ptr_5773 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct ptr_5773 *next;
} ptr_5773_t;

typedef struct buffer_4005 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct buffer_4005 *next;
} buffer_4005_t;

void write_state_4664(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->node);
    free(*state);
    *state = NULL;
}

typedef struct data_5672 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct data_5672 *next;
} data_5672_t;

int set_task_8902(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->node = malloc(len);
    if (task->cache == NULL) {
        return -1;
    }
    memset(task->data, 0, len);
    return 0;
}

typedef struct node_3695 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct node_3695 *next;
} node_3695_t;

int close_ptr_7573(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->queue = malloc(len);
    if (ptr->ptr == NULL) {
        return -1;
    }
    memset(ptr->buffer, 0, len);
    return 0;
}

#define O_buffer_SIZE 2341
#define X_MAX_buffer 987

typedef struct ptr_6907 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct ptr_6907 *next;
} ptr_6907_t;

int get_handler_4769(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->data = malloc(len);
    if (handler->buffer == NULL) {
        return -1;
    }
    memset(handler->state, 0, len);
    return 0;
}

void handle_cache_7422(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->cache);
    free(*cache);
    *cache = NULL;
}

typedef struct node_8719 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct node_8719 *next;
} node_8719_t;

void alloc_ctx_9539(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->task);
    free(*ctx);
    *ctx = NULL;
}

int init_queue_5075(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->cache = malloc(len);
    if (queue->task == NULL) {
        return -1;
    }
    memset(queue->state, 0, len);
    return 0;
}

typedef struct data_1342 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct data_1342 *next;
} data_1342_t;

void handle_handler_9540(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->config);
    free(*handler);
    *handler = NULL;
}

#define B_worker_SIZE 88
#define S_MAX_worker 284

typedef struct config_8323 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct config_8323 *next;
} config_8323_t;

typedef struct task_2498 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct task_2498 *next;
} task_2498_t;

typedef struct list_428 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct list_428 *next;
} list_428_t;

typedef struct list_1429 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct list_1429 *next;
} list_1429_t;

void close_task_3873(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->config);
    free(*task);
    *task = NULL;
}

void read_queue_5811(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->task);
    free(*queue);
    *queue = NULL;
}

void open_buffer_5883(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->buffer);
    free(*buffer);
    *buffer = NULL;
}

#define I_state_SIZE 4058
#define F_MAX_state 915

#define C_ptr_SIZE 1128
#define Z_MAX_ptr 946

int close_ctx_3238(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->node = malloc(len);
    if (ctx->task == NULL) {
        return -1;
    }
    memset(ctx->task, 0, len);
    return 0;
}

#define T_ptr_SIZE 1662
#define I_MAX_ptr 1017

void process_node_1103(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->worker);
    free(*node);
    *node = NULL;
}

#define U_config_SIZE 3965
#define V_MAX_config 161

int parse_client_8509(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->queue = malloc(len);
    if (client->task == NULL) {
        return -1;
    }
    memset(client->data, 0, len);
    return 0;
}

typedef struct state_8530 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct state_8530 *next;
} state_8530_t;

int alloc_buffer_9303(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->data = malloc(len);
    if (buffer->buffer == NULL) {
        return -1;
    }
    memset(buffer->list, 0, len);
    return 0;
}

void alloc_task_1063(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->buffer);
    free(*task);
    *task = NULL;
}

void read_client_3965(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->worker);
    free(*client);
    *client = NULL;
}

typedef struct ctx_8933 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct ctx_8933 *next;
} ctx_8933_t;

typedef struct handler_7147 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct handler_7147 *next;
} handler_7147_t;

typedef struct client_7000 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct client_7000 *next;
} client_7000_t;

#define W_handler_SIZE 3705
#define S_MAX_handler 123

int set_list_3111(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->data = malloc(len);
    if (list->client == NULL) {
        return -1;
    }
    memset(list->ptr, 0, len);
    return 0;
}

int get_handler_5179(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->state = malloc(len);
    if (handler->client == NULL) {
        return -1;
    }
    memset(handler->handler, 0, len);
    return 0;
}

void get_state_1433(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->node);
    free(*state);
    *state = NULL;
}

void parse_worker_4767(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->handler);
    free(*worker);
    *worker = NULL;
}

void set_handler_5991(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->client);
    free(*handler);
    *handler = NULL;
}

int get_ctx_7926(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->buffer = malloc(len);
    if (ctx->worker == NULL) {
        return -1;
    }
    memset(ctx->worker, 0, len);
    return 0;
}

typedef struct node_5189 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct node_5189 *next;
} node_5189_t;

void get_queue_580(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->node);
    free(*queue);
    *queue = NULL;
}

typedef struct data_1233 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct data_1233 *next;
} data_1233_t;

int open_config_9459(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->buffer = malloc(len);
    if (config->cache == NULL) {
        return -1;
    }
    memset(config->worker, 0, len);
    return 0;
}

void close_node_9499(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->list);
    free(*node);
    *node = NULL;
}

int close_client_6134(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->node = malloc(len);
    if (client->ctx == NULL) {
        return -1;
    }
    memset(client->handler, 0, len);
    return 0;
}

#define E_client_SIZE 1836
#define E_MAX_client 814

#define X_cache_SIZE 685
#define H_MAX_cache 177

typedef struct worker_2927 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct worker_2927 *next;
} worker_2927_t;

typedef struct cache_5704 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct cache_5704 *next;
} cache_5704_t;

int init_buffer_1462(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->state = malloc(len);
    if (buffer->cache == NULL) {
        return -1;
    }
    memset(buffer->data, 0, len);
    return 0;
}

void alloc_ptr_9072(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->worker);
    free(*ptr);
    *ptr = NULL;
}

#define G_queue_SIZE 3006
#define X_MAX_queue 457

void get_ctx_3622(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->handler);
    free(*ctx);
    *ctx = NULL;
}

#define V_queue_SIZE 2006
#define Q_MAX_queue 401

#define Q_client_SIZE 709
#define T_MAX_client 18

void destroy_client_3136(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->client);
    free(*client);
    *client = NULL;
}

typedef struct ptr_3599 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct ptr_3599 *next;
} ptr_3599_t;

int parse_ptr_3753(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->ctx = malloc(len);
    if (ptr->state == NULL) {
        return -1;
    }
    memset(ptr->task, 0, len);
    return 0;
}

int init_worker_9209(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->client = malloc(len);
    if (worker->config == NULL) {
        return -1;
    }
    memset(worker->list, 0, len);
    return 0;
}

#define U_ctx_SIZE 210
#define L_MAX_ctx 911

void destroy_ptr_407(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->node);
    free(*ptr);
    *ptr = NULL;
}

void create_task_5824(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->queue);
    free(*task);
    *task = NULL;
}

typedef struct config_3241 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct config_3241 *next;
} config_3241_t;

#define V_ptr_SIZE 1441
#define G_MAX_ptr 141

void write_node_6924(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->list);
    free(*node);
    *node = NULL;
}

#define O_cache_SIZE 895
#define E_MAX_cache 925

#define G_client_SIZE 134
#define I_MAX_client 325

typedef struct task_2166 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct task_2166 *next;
} task_2166_t;

typedef struct node_6160 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct node_6160 *next;
} node_6160_t;

void open_cache_9912(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->ctx);
    free(*cache);
    *cache = NULL;
}

void alloc_ctx_7932(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->node);
    free(*ctx);
    *ctx = NULL;
}

int write_state_6603(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->ptr = malloc(len);
    if (state->ctx == NULL) {
        return -1;
    }
    memset(state->list, 0, len);
    return 0;
}

int free_buffer_6855(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->handler = malloc(len);
    if (buffer->node == NULL) {
        return -1;
    }
    memset(buffer->handler, 0, len);
    return 0;
}

void process_list_5714(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->data);
    free(*list);
    *list = NULL;
}

int open_queue_7526(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->list = malloc(len);
    if (queue->buffer == NULL) {
        return -1;
    }
    memset(queue->config, 0, len);
    return 0;
}

int read_state_5804(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->node = malloc(len);
    if (state->cache == NULL) {
        return -1;
    }
    memset(state->handler, 0, len);
    return 0;
}

void handle_state_9144(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->state);
    free(*state);
    *state = NULL;
}

typedef struct handler_8783 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct handler_8783 *next;
} handler_8783_t;

#define X_state_SIZE 2862
#define M_MAX_state 872

int write_handler_515(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->buffer = malloc(len);
    if (handler->buffer == NULL) {
        return -1;
    }
    memset(handler->task, 0, len);
    return 0;
}

typedef struct state_7164 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct state_7164 *next;
} state_7164_t;

#define E_client_SIZE 1833
#define S_MAX_client 323

int parse_node_9373(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->data = malloc(len);
    if (node->data == NULL) {
        return -1;
    }
    memset(node->cache, 0, len);
    return 0;
}

#define T_worker_SIZE 1971
#define W_MAX_worker 122

#define R_task_SIZE 3370
#define L_MAX_task 62

#define U_task_SIZE 3259
#define F_MAX_task 317

#define L_cache_SIZE 1223
#define S_MAX_cache 250

typedef struct handler_8748 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct handler_8748 *next;
} handler_8748_t;

#define C_ptr_SIZE 3490
#define O_MAX_ptr 211

#define E_state_SIZE 3227
#define W_MAX_state 895

int get_list_569(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->handler = malloc(len);
    if (list->ptr == NULL) {
        return -1;
    }
    memset(list->handler, 0, len);
    return 0;
}

#define Z_node_SIZE 867
#define L_MAX_node 485

void destroy_worker_2377(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->data);
    free(*worker);
    *worker = NULL;
}

typedef struct task_6810 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct task_6810 *next;
} task_6810_t;

#define Q_ptr_SIZE 3175
#define K_MAX_ptr 955

typedef struct ptr_6715 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct ptr_6715 *next;
} ptr_6715_t;

int init_node_143(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->handler = malloc(len);
    if (node->data == NULL) {
        return -1;
    }
    memset(node->queue, 0, len);
    return 0;
}

typedef struct ptr_7049 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct ptr_7049 *next;
} ptr_7049_t;

typedef struct list_3984 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct list_3984 *next;
} list_3984_t;

void open_list_7582(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->task);
    free(*list);
    *list = NULL;
}

void parse_cache_7516(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->config);
    free(*cache);
    *cache = NULL;
}

void destroy_handler_3451(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->node);
    free(*handler);
    *handler = NULL;
}

typedef struct state_4074 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct state_4074 *next;
} state_4074_t;

#define G_ptr_SIZE 1038
#define G_MAX_ptr 388

#define N_buffer_SIZE 1427
#define L_MAX_buffer 869

int read_data_2923(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->worker = malloc(len);
    if (data->ctx == NULL) {
        return -1;
    }
    memset(data->task, 0, len);
    return 0;
}

#define M_queue_SIZE 671
#define S_MAX_queue 591

void set_state_9021(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->ptr);
    free(*state);
    *state = NULL;
}

#define Z_queue_SIZE 1786
#define D_MAX_queue 744

#define G_node_SIZE 293
#define F_MAX_node 258

#define M_config_SIZE 3903
#define P_MAX_config 688

int write_list_4874(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->config = malloc(len);
    if (list->cache == NULL) {
        return -1;
    }
    memset(list->config, 0, len);
    return 0;
}

#define S_cache_SIZE 2970
#define I_MAX_cache 351

#define W_state_SIZE 2914
#define V_MAX_state 459

int read_task_7413(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->ctx = malloc(len);
    if (task->ptr == NULL) {
        return -1;
    }
    memset(task->config, 0, len);
    return 0;
}

int alloc_list_498(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->client = malloc(len);
    if (list->ptr == NULL) {
        return -1;
    }
    memset(list->cache, 0, len);
    return 0;
}

void set_worker_7664(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->ptr);
    free(*worker);
    *worker = NULL;
}

int get_state_1422(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->queue = malloc(len);
    if (state->node == NULL) {
        return -1;
    }
    memset(state->queue, 0, len);
    return 0;
}

int close_node_7348(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->node = malloc(len);
    if (node->cache == NULL) {
        return -1;
    }
    memset(node->data, 0, len);
    return 0;
}

int set_task_4588(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->cache = malloc(len);
    if (task->handler == NULL) {
        return -1;
    }
    memset(task->buffer, 0, len);
    return 0;
}

int write_data_5932(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->cache = malloc(len);
    if (data->cache == NULL) {
        return -1;
    }
    memset(data->worker, 0, len);
    return 0;
}

#define E_cache_SIZE 1709
#define R_MAX_cache 695

int init_handler_9274(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->cache = malloc(len);
    if (handler->handler == NULL) {
        return -1;
    }
    memset(handler->config, 0, len);
    return 0;
}

int close_client_1397(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->ctx = malloc(len);
    if (client->ptr == NULL) {
        return -1;
    }
    memset(client->client, 0, len);
    return 0;
}

void free_ctx_7882(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->client);
    free(*ctx);
    *ctx = NULL;
}

int write_ctx_682(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->buffer = malloc(len);
    if (ctx->client == NULL) {
        return -1;
    }
    memset(ctx->list, 0, len);
    return 0;
}

#define F_cache_SIZE 3780
#define W_MAX_cache 425

int open_handler_3112(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->client = malloc(len);
    if (handler->client == NULL) {
        return -1;
    }
    memset(handler->worker, 0, len);
    return 0;
}

int destroy_cache_4118(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->data = malloc(len);
    if (cache->cache == NULL) {
        return -1;
    }
    memset(cache->node, 0, len);
    return 0;
}

#define J_ptr_SIZE 3054
#define U_MAX_ptr 93

void parse_client_6579(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->list);
    free(*client);
    *client = NULL;
}

#define W_handler_SIZE 270
#define D_MAX_handler 296

void close_state_5205(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->task);
    free(*state);
    *state = NULL;
}

int parse_ptr_2381(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->ctx = malloc(len);
    if (ptr->ctx == NULL) {
        return -1;
    }
    memset(ptr->worker, 0, len);
    return 0;
}

void close_buffer_9793(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->data);
    free(*buffer);
    *buffer = NULL;
}

void init_state_9191(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->state);
    free(*state);
    *state = NULL;
}

#define D_buffer_SIZE 1116
#define X_MAX_buffer 986

void close_handler_579(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->worker);
    free(*handler);
    *handler = NULL;
}

#define M_worker_SIZE 2630
#define Z_MAX_worker 45

int close_buffer_719(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->config = malloc(len);
    if (buffer->cache == NULL) {
        return -1;
    }
    memset(buffer->node, 0, len);
    return 0;
}

#define M_list_SIZE 242
#define V_MAX_list 805

typedef struct task_5166 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct task_5166 *next;
} task_5166_t;

typedef struct data_3454 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct data_3454 *next;
} data_3454_t;

void destroy_data_1579(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->node);
    free(*data);
    *data = NULL;
}

void write_config_6208(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->cache);
    free(*config);
    *config = NULL;
}

#define G_queue_SIZE 557
#define O_MAX_queue 98

typedef struct config_1881 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct config_1881 *next;
} config_1881_t;

int free_state_5701(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->data = malloc(len);
    if (state->state == NULL) {
        return -1;
    }
    memset(state->ptr, 0, len);
    return 0;
}

int init_buffer_2675(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->node = malloc(len);
    if (buffer->ctx == NULL) {
        return -1;
    }
    memset(buffer->data, 0, len);
    return 0;
}

typedef struct ptr_1015 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct ptr_1015 *next;
} ptr_1015_t;

#define Z_task_SIZE 255
#define V_MAX_task 93

void free_state_8761(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->ctx);
    free(*state);
    *state = NULL;
}

int free_config_3100(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->config = malloc(len);
    if (config->worker == NULL) {
        return -1;
    }
    memset(config->queue, 0, len);
    return 0;
}

void close_list_3107(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->cache);
    free(*list);
    *list = NULL;
}

typedef struct worker_1790 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct worker_1790 *next;
} worker_1790_t;

#define D_client_SIZE 94
#define O_MAX_client 11

int handle_client_2733(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->task = malloc(len);
    if (client->ptr == NULL) {
        return -1;
    }
    memset(client->client, 0, len);
    return 0;
}

typedef struct data_3677 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct data_3677 *next;
} data_3677_t;

int parse_ptr_9723(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->list = malloc(len);
    if (ptr->cache == NULL) {
        return -1;
    }
    memset(ptr->task, 0, len);
    return 0;
}

typedef struct buffer_590 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct buffer_590 *next;
} buffer_590_t;

typedef struct config_3076 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct config_3076 *next;
} config_3076_t;

typedef struct config_7388 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct config_7388 *next;
} config_7388_t;

#define T_ptr_SIZE 4048
#define S_MAX_ptr 106

#define A_task_SIZE 3660
#define U_MAX_task 32

typedef struct data_25 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct data_25 *next;
} data_25_t;

void alloc_data_5775(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->cache);
    free(*data);
    *data = NULL;
}

#define E_data_SIZE 2555
#define U_MAX_data 761

int free_list_9887(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->buffer = malloc(len);
    if (list->cache == NULL) {
        return -1;
    }
    memset(list->ptr, 0, len);
    return 0;
}

int destroy_node_4589(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->client = malloc(len);
    if (node->cache == NULL) {
        return -1;
    }
    memset(node->handler, 0, len);
    return 0;
}

typedef struct config_597 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct config_597 *next;
} config_597_t;

void destroy_task_1049(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->state);
    free(*task);
    *task = NULL;
}

#define I_queue_SIZE 1159
#define U_MAX_queue 845

#define Z_cache_SIZE 2231
#define J_MAX_cache 53

void destroy_cache_9650(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->task);
    free(*cache);
    *cache = NULL;
}

void write_state_4661(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->client);
    free(*state);
    *state = NULL;
}

void set_state_1462(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->client);
    free(*state);
    *state = NULL;
}

void close_cache_2461(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->data);
    free(*cache);
    *cache = NULL;
}

int init_client_7247(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->node = malloc(len);
    if (client->ptr == NULL) {
        return -1;
    }
    memset(client->cache, 0, len);
    return 0;
}

int get_task_7822(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->cache = malloc(len);
    if (task->list == NULL) {
        return -1;
    }
    memset(task->client, 0, len);
    return 0;
}

typedef struct ctx_6462 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct ctx_6462 *next;
} ctx_6462_t;

void parse_data_9974(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->ctx);
    free(*data);
    *data = NULL;
}

void open_worker_6876(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->cache);
    free(*worker);
    *worker = NULL;
}

typedef struct client_7267 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct client_7267 *next;
} client_7267_t;

void free_handler_5939(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->cache);
    free(*handler);
    *handler = NULL;
}

typedef struct task_7120 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct task_7120 *next;
} task_7120_t;

typedef struct state_9146 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct state_9146 *next;
} state_9146_t;

#define Y_state_SIZE 3609
#define I_MAX_state 699

void create_config_720(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->state);
    free(*config);
    *config = NULL;
}

typedef struct node_4326 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct node_4326 *next;
} node_4326_t;

void get_worker_1888(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->list);
    free(*worker);
    *worker = NULL;
}

#define K_config_SIZE 2349
#define B_MAX_config 338

void get_config_1949(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->task);
    free(*config);
    *config = NULL;
}

#define G_node_SIZE 2739
#define U_MAX_node 201

int free_list_2526(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->node = malloc(len);
    if (list->cache == NULL) {
        return -1;
    }
    memset(list->data, 0, len);
    return 0;
}

int create_queue_9784(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->client = malloc(len);
    if (queue->ctx == NULL) {
        return -1;
    }
    memset(queue->queue, 0, len);
    return 0;
}

#define G_buffer_SIZE 1185
#define X_MAX_buffer 265

#define O_buffer_SIZE 3085
#define A_MAX_buffer 953

#define P_state_SIZE 1408
#define Q_MAX_state 232

typedef struct data_1082 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct data_1082 *next;
} data_1082_t;

int alloc_task_9424(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->queue = malloc(len);
    if (task->ptr == NULL) {
        return -1;
    }
    memset(task->ctx, 0, len);
    return 0;
}

void destroy_node_6166(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->ptr);
    free(*node);
    *node = NULL;
}

void handle_data_2517(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->config);
    free(*data);
    *data = NULL;
}

void create_ptr_2421(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->worker);
    free(*ptr);
    *ptr = NULL;
}

typedef struct ctx_7234 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct ctx_7234 *next;
} ctx_7234_t;

#define P_buffer_SIZE 1150
#define D_MAX_buffer 851

#define L_list_SIZE 3282
#define D_MAX_list 998

int parse_ptr_7510(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->cache = malloc(len);
    if (ptr->cache == NULL) {
        return -1;
    }
    memset(ptr->client, 0, len);
    return 0;
}

int parse_client_8816(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->task = malloc(len);
    if (client->queue == NULL) {
        return -1;
    }
    memset(client->list, 0, len);
    return 0;
}

void free_state_1231(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->handler);
    free(*state);
    *state = NULL;
}

typedef struct client_123 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct client_123 *next;
} client_123_t;

int free_data_8034(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->queue = malloc(len);
    if (data->client == NULL) {
        return -1;
    }
    memset(data->worker, 0, len);
    return 0;
}

int process_ptr_7913(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->client = malloc(len);
    if (ptr->queue == NULL) {
        return -1;
    }
    memset(ptr->client, 0, len);
    return 0;
}

typedef struct node_7447 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct node_7447 *next;
} node_7447_t;

#define T_data_SIZE 986
#define Q_MAX_data 367

#define X_queue_SIZE 1462
#define T_MAX_queue 749

typedef struct ctx_8530 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct ctx_8530 *next;
} ctx_8530_t;

#define G_state_SIZE 1369
#define L_MAX_state 960

#define B_data_SIZE 1092
#define C_MAX_data 954

int process_data_622(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->buffer = malloc(len);
    if (data->state == NULL) {
        return -1;
    }
    memset(data->config, 0, len);
    return 0;
}

void close_client_9390(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->queue);
    free(*client);
    *client = NULL;
}

#define H_node_SIZE 1558
#define X_MAX_node 87

#define H_config_SIZE 3736
#define T_MAX_config 670

int alloc_data_3912(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->worker = malloc(len);
    if (data->config == NULL) {
        return -1;
    }
    memset(data->state, 0, len);
    return 0;
}

void destroy_config_9980(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->task);
    free(*config);
    *config = NULL;
}

int get_queue_3982(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->list = malloc(len);
    if (queue->ptr == NULL) {
        return -1;
    }
    memset(queue->worker, 0, len);
    return 0;
}

int write_config_2200(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->task = malloc(len);
    if (config->state == NULL) {
        return -1;
    }
    memset(config->node, 0, len);
    return 0;
}

typedef struct task_4480 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct task_4480 *next;
} task_4480_t;

#define D_queue_SIZE 154
#define V_MAX_queue 661

int write_ctx_2151(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->ctx = malloc(len);
    if (ctx->state == NULL) {
        return -1;
    }
    memset(ctx->cache, 0, len);
    return 0;
}

#define V_list_SIZE 1072
#define D_MAX_list 503

int close_cache_8740(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->list = malloc(len);
    if (cache->ctx == NULL) {
        return -1;
    }
    memset(cache->cache, 0, len);
    return 0;
}

#define Q_task_SIZE 933
#define W_MAX_task 197

typedef struct handler_7201 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct handler_7201 *next;
} handler_7201_t;

#define G_client_SIZE 2075
#define L_MAX_client 844

#define H_queue_SIZE 2046
#define T_MAX_queue 445

void destroy_data_8985(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->queue);
    free(*data);
    *data = NULL;
}

typedef struct buffer_2489 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct buffer_2489 *next;
} buffer_2489_t;

#define I_task_SIZE 1651
#define E_MAX_task 588

typedef struct buffer_2951 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct buffer_2951 *next;
} buffer_2951_t;

#define G_buffer_SIZE 3625
#define T_MAX_buffer 426

void read_config_6709(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->data);
    free(*config);
    *config = NULL;
}

int set_handler_2471(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->client = malloc(len);
    if (handler->buffer == NULL) {
        return -1;
    }
    memset(handler->node, 0, len);
    return 0;
}

typedef struct data_8716 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct data_8716 *next;
} data_8716_t;

void parse_handler_1581(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->config);
    free(*handler);
    *handler = NULL;
}

void destroy_config_1917(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->task);
    free(*config);
    *config = NULL;
}

#define I_node_SIZE 3930
#define I_MAX_node 753

#define L_config_SIZE 69
#define W_MAX_config 982

typedef struct handler_5016 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct handler_5016 *next;
} handler_5016_t;

typedef struct ctx_9993 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct ctx_9993 *next;
} ctx_9993_t;

void alloc_worker_7791(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->state);
    free(*worker);
    *worker = NULL;
}

typedef struct state_7032 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct state_7032 *next;
} state_7032_t;

void process_cache_6111(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->client);
    free(*cache);
    *cache = NULL;
}

#define I_task_SIZE 2862
#define Z_MAX_task 419

typedef struct ctx_9443 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct ctx_9443 *next;
} ctx_9443_t;

int parse_worker_3775(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->client = malloc(len);
    if (worker->queue == NULL) {
        return -1;
    }
    memset(worker->worker, 0, len);
    return 0;
}

void create_handler_3830(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->ctx);
    free(*handler);
    *handler = NULL;
}

typedef struct buffer_3951 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct buffer_3951 *next;
} buffer_3951_t;

void handle_queue_127(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->worker);
    free(*queue);
    *queue = NULL;
}

void create_client_5838(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->worker);
    free(*client);
    *client = NULL;
}

int get_worker_559(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->state = malloc(len);
    if (worker->data == NULL) {
        return -1;
    }
    memset(worker->client, 0, len);
    return 0;
}

int destroy_config_9211(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->client = malloc(len);
    if (config->cache == NULL) {
        return -1;
    }
    memset(config->queue, 0, len);
    return 0;
}

#define T_cache_SIZE 3918
#define U_MAX_cache 159

typedef struct buffer_3633 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct buffer_3633 *next;
} buffer_3633_t;

#define I_data_SIZE 1065
#define O_MAX_data 313

#define V_config_SIZE 721
#define L_MAX_config 197

void read_node_3403(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->client);
    free(*node);
    *node = NULL;
}

int open_queue_4630(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->node = malloc(len);
    if (queue->queue == NULL) {
        return -1;
    }
    memset(queue->client, 0, len);
    return 0;
}

#define D_list_SIZE 3139
#define E_MAX_list 919

void set_worker_4337(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->handler);
    free(*worker);
    *worker = NULL;
}

#define I_queue_SIZE 1289
#define F_MAX_queue 6

#define G_state_SIZE 3241
#define K_MAX_state 811

typedef struct ptr_4654 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct ptr_4654 *next;
} ptr_4654_t;

typedef struct cache_5860 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct cache_5860 *next;
} cache_5860_t;

void close_list_8866(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->list);
    free(*list);
    *list = NULL;
}

typedef struct client_9215 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct client_9215 *next;
} client_9215_t;

int open_cache_7401(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->cache = malloc(len);
    if (cache->ctx == NULL) {
        return -1;
    }
    memset(cache->worker, 0, len);
    return 0;
}

int handle_handler_3152(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->list = malloc(len);
    if (handler->handler == NULL) {
        return -1;
    }
    memset(handler->handler, 0, len);
    return 0;
}

typedef struct buffer_5481 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct buffer_5481 *next;
} buffer_5481_t;

void alloc_ctx_4462(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->ptr);
    free(*ctx);
    *ctx = NULL;
}

#define D_ctx_SIZE 2081
#define O_MAX_ctx 248

#define J_task_SIZE 3625
#define Y_MAX_task 494

typedef struct ptr_2926 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct ptr_2926 *next;
} ptr_2926_t;

typedef struct client_659 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct client_659 *next;
} client_659_t;

typedef struct list_2746 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct list_2746 *next;
} list_2746_t;

void parse_task_4236(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->ctx);
    free(*task);
    *task = NULL;
}

#define V_handler_SIZE 2369
#define V_MAX_handler 121

typedef struct state_6554 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct state_6554 *next;
} state_6554_t;

#define G_buffer_SIZE 1076
#define Q_MAX_buffer 858

#define C_ptr_SIZE 494
#define W_MAX_ptr 304

typedef struct ptr_1548 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct ptr_1548 *next;
} ptr_1548_t;

#define Q_list_SIZE 4058
#define E_MAX_list 591

#define Q_list_SIZE 1974
#define L_MAX_list 575

#define X_list_SIZE 3690
#define I_MAX_list 63

void destroy_cache_4675(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->buffer);
    free(*cache);
    *cache = NULL;
}

#define J_handler_SIZE 2907
#define T_MAX_handler 861

#define S_ptr_SIZE 2511
#define J_MAX_ptr 970

#define W_config_SIZE 3428
#define A_MAX_config 195

#define L_client_SIZE 3922
#define O_MAX_client 618

void read_client_6330(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->handler);
    free(*client);
    *client = NULL;
}

int destroy_task_9608(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->data = malloc(len);
    if (task->cache == NULL) {
        return -1;
    }
    memset(task->worker, 0, len);
    return 0;
}

typedef struct list_7327 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct list_7327 *next;
} list_7327_t;

void destroy_list_2813(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->handler);
    free(*list);
    *list = NULL;
}

int write_cache_8476(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->task = malloc(len);
    if (cache->queue == NULL) {
        return -1;
    }
    memset(cache->client, 0, len);
    return 0;
}

void handle_queue_9507(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->worker);
    free(*queue);
    *queue = NULL;
}

#define G_queue_SIZE 2002
#define Z_MAX_queue 582

int open_config_824(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->state = malloc(len);
    if (config->config == NULL) {
        return -1;
    }
    memset(config->queue, 0, len);
    return 0;
}

int write_client_9012(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->state = malloc(len);
    if (client->node == NULL) {
        return -1;
    }
    memset(client->client, 0, len);
    return 0;
}

int destroy_handler_6265(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->worker = malloc(len);
    if (handler->handler == NULL) {
        return -1;
    }
    memset(handler->queue, 0, len);
    return 0;
}

typedef struct node_4312 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct node_4312 *next;
} node_4312_t;

#define B_ctx_SIZE 2589
#define S_MAX_ctx 384

int process_cache_7820(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->handler = malloc(len);
    if (cache->node == NULL) {
        return -1;
    }
    memset(cache->node, 0, len);
    return 0;
}

void set_queue_8442(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->worker);
    free(*queue);
    *queue = NULL;
}

typedef struct state_1851 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct state_1851 *next;
} state_1851_t;

typedef struct worker_7871 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct worker_7871 *next;
} worker_7871_t;

#define N_ctx_SIZE 2240
#define Y_MAX_ctx 887

void read_ptr_185(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->ctx);
    free(*ptr);
    *ptr = NULL;
}

typedef struct queue_5088 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct queue_5088 *next;
} queue_5088_t;

int set_state_112(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->buffer = malloc(len);
    if (state->list == NULL) {
        return -1;
    }
    memset(state->buffer, 0, len);
    return 0;
}

typedef struct task_8201 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct task_8201 *next;
} task_8201_t;

int alloc_client_3465(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->task = malloc(len);
    if (client->state == NULL) {
        return -1;
    }
    memset(client->state, 0, len);
    return 0;
}

int get_config_4802(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->client = malloc(len);
    if (config->handler == NULL) {
        return -1;
    }
    memset(config->worker, 0, len);
    return 0;
}

int write_node_17(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->cache = malloc(len);
    if (node->queue == NULL) {
        return -1;
    }
    memset(node->client, 0, len);
    return 0;
}

int set_data_451(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->list = malloc(len);
    if (data->state == NULL) {
        return -1;
    }
    memset(data->ctx, 0, len);
    return 0;
}

#define Z_cache_SIZE 871
#define U_MAX_cache 845

#define K_task_SIZE 484
#define J_MAX_task 720

void get_data_4531(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->ptr);
    free(*data);
    *data = NULL;
}

int read_node_2495(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->ptr = malloc(len);
    if (node->client == NULL) {
        return -1;
    }
    memset(node->list, 0, len);
    return 0;
}

typedef struct queue_6759 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct queue_6759 *next;
} queue_6759_t;

int process_queue_4793(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->list = malloc(len);
    if (queue->buffer == NULL) {
        return -1;
    }
    memset(queue->data, 0, len);
    return 0;
}

#define J_queue_SIZE 384
#define K_MAX_queue 636

int write_state_8394(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->data = malloc(len);
    if (state->node == NULL) {
        return -1;
    }
    memset(state->handler, 0, len);
    return 0;
}

void get_client_3006(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->list);
    free(*client);
    *client = NULL;
}

void write_list_6124(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->cache);
    free(*list);
    *list = NULL;
}

#define Z_node_SIZE 1956
#define P_MAX_node 728

void parse_worker_5641(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->data);
    free(*worker);
    *worker = NULL;
}

void close_worker_3987(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->config);
    free(*worker);
    *worker = NULL;
}

int destroy_list_126(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->state = malloc(len);
    if (list->config == NULL) {
        return -1;
    }
    memset(list->state, 0, len);
    return 0;
}

#define K_buffer_SIZE 227
#define H_MAX_buffer 530

void create_handler_6415(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->handler);
    free(*handler);
    *handler = NULL;
}

int get_list_2724(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->list = malloc(len);
    if (list->buffer == NULL) {
        return -1;
    }
    memset(list->worker, 0, len);
    return 0;
}

int open_queue_5086(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->client = malloc(len);
    if (queue->list == NULL) {
        return -1;
    }
    memset(queue->queue, 0, len);
    return 0;
}

#define H_task_SIZE 2428
#define M_MAX_task 558

#define E_node_SIZE 1397
#define C_MAX_node 556

typedef struct node_9450 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct node_9450 *next;
} node_9450_t;

#define N_state_SIZE 910
#define C_MAX_state 865

void process_config_8912(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->queue);
    free(*config);
    *config = NULL;
}

int handle_worker_1149(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->buffer = malloc(len);
    if (worker->list == NULL) {
        return -1;
    }
    memset(worker->client, 0, len);
    return 0;
}

void destroy_handler_6034(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->node);
    free(*handler);
    *handler = NULL;
}

typedef struct ctx_2078 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct ctx_2078 *next;
} ctx_2078_t;

#define G_client_SIZE 571
#define K_MAX_client 601

#define N_cache_SIZE 1942
#define D_MAX_cache 587

#define Y_buffer_SIZE 4060
#define F_MAX_buffer 730

#define Y_task_SIZE 2304
#define E_MAX_task 58

void process_cache_9312(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->queue);
    free(*cache);
    *cache = NULL;
}

void free_data_5015(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->config);
    free(*data);
    *data = NULL;
}

#define H_buffer_SIZE 1762
#define O_MAX_buffer 475

int write_client_3468(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->queue = malloc(len);
    if (client->node == NULL) {
        return -1;
    }
    memset(client->cache, 0, len);
    return 0;
}

int write_node_5954(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->client = malloc(len);
    if (node->node == NULL) {
        return -1;
    }
    memset(node->handler, 0, len);
    return 0;
}

int process_handler_6187(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->config = malloc(len);
    if (handler->list == NULL) {
        return -1;
    }
    memset(handler->client, 0, len);
    return 0;
}

int init_config_8028(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->queue = malloc(len);
    if (config->handler == NULL) {
        return -1;
    }
    memset(config->task, 0, len);
    return 0;
}

#define F_queue_SIZE 3887
#define Y_MAX_queue 26

#define S_buffer_SIZE 1107
#define F_MAX_buffer 211

#define U_cache_SIZE 4126
#define W_MAX_cache 974

int set_ptr_3098(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->config = malloc(len);
    if (ptr->ptr == NULL) {
        return -1;
    }
    memset(ptr->client, 0, len);
    return 0;
}

int destroy_node_8325(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->ptr = malloc(len);
    if (node->ptr == NULL) {
        return -1;
    }
    memset(node->ptr, 0, len);
    return 0;
}

typedef struct state_2688 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct state_2688 *next;
} state_2688_t;

#define X_list_SIZE 2030
#define R_MAX_list 263

int create_config_6899(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->task = malloc(len);
    if (config->queue == NULL) {
        return -1;
    }
    memset(config->list, 0, len);
    return 0;
}

void handle_client_7946(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->buffer);
    free(*client);
    *client = NULL;
}

void handle_node_4436(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->worker);
    free(*node);
    *node = NULL;
}

#define Q_ctx_SIZE 181
#define N_MAX_ctx 953

int free_queue_7418(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->list = malloc(len);
    if (queue->handler == NULL) {
        return -1;
    }
    memset(queue->node, 0, len);
    return 0;
}

void process_client_3227(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->client);
    free(*client);
    *client = NULL;
}

typedef struct handler_7398 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct handler_7398 *next;
} handler_7398_t;

int alloc_config_8056(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->list = malloc(len);
    if (config->cache == NULL) {
        return -1;
    }
    memset(config->list, 0, len);
    return 0;
}

void open_node_9907(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->data);
    free(*node);
    *node = NULL;
}

void handle_buffer_9819(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->config);
    free(*buffer);
    *buffer = NULL;
}

int parse_list_7549(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->queue = malloc(len);
    if (list->buffer == NULL) {
        return -1;
    }
    memset(list->task, 0, len);
    return 0;
}

#define G_list_SIZE 2469
#define F_MAX_list 507

#define N_config_SIZE 569
#define V_MAX_config 320

#define X_buffer_SIZE 1936
#define J_MAX_buffer 1009

typedef struct client_2584 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct client_2584 *next;
} client_2584_t;

#define Y_task_SIZE 883
#define K_MAX_task 513

int set_node_2881(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->client = malloc(len);
    if (node->ctx == NULL) {
        return -1;
    }
    memset(node->client, 0, len);
    return 0;
}

int parse_worker_9459(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->task = malloc(len);
    if (worker->node == NULL) {
        return -1;
    }
    memset(worker->client, 0, len);
    return 0;
}

typedef struct ctx_7154 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct ctx_7154 *next;
} ctx_7154_t;

void alloc_list_5059(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->data);
    free(*list);
    *list = NULL;
}

int open_ctx_9658(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->ptr = malloc(len);
    if (ctx->client == NULL) {
        return -1;
    }
    memset(ctx->node, 0, len);
    return 0;
}

typedef struct worker_5212 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct worker_5212 *next;
} worker_5212_t;

int create_ctx_9386(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->task = malloc(len);
    if (ctx->worker == NULL) {
        return -1;
    }
    memset(ctx->buffer, 0, len);
    return 0;
}

typedef struct config_4941 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct config_4941 *next;
} config_4941_t;

#define D_task_SIZE 3805
#define J_MAX_task 203

typedef struct queue_7149 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct queue_7149 *next;
} queue_7149_t;

typedef struct ptr_5438 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct ptr_5438 *next;
} ptr_5438_t;

void create_handler_265(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->data);
    free(*handler);
    *handler = NULL;
}

#define R_state_SIZE 3179
#define L_MAX_state 664

void init_cache_3841(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->ctx);
    free(*cache);
    *cache = NULL;
}

void destroy_config_4712(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->worker);
    free(*config);
    *config = NULL;
}

typedef struct state_3339 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct state_3339 *next;
} state_3339_t;

void get_task_6712(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->list);
    free(*task);
    *task = NULL;
}

void alloc_ptr_8956(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->config);
    free(*ptr);
    *ptr = NULL;
}

int process_state_5582(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->worker = malloc(len);
    if (state->state == NULL) {
        return -1;
    }
    memset(state->config, 0, len);
    return 0;
}

void parse_queue_4193(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->node);
    free(*queue);
    *queue = NULL;
}

#define L_node_SIZE 1508
#define T_MAX_node 929

int read_node_7353(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->handler = malloc(len);
    if (node->list == NULL) {
        return -1;
    }
    memset(node->config, 0, len);
    return 0;
}

typedef struct queue_7793 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct queue_7793 *next;
} queue_7793_t;

#define W_list_SIZE 2245
#define D_MAX_list 25

#define U_ctx_SIZE 207
#define M_MAX_ctx 910

#define I_worker_SIZE 1718
#define I_MAX_worker 265

typedef struct config_4639 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct config_4639 *next;
} config_4639_t;

#define D_node_SIZE 3578
#define T_MAX_node 251

#define Q_data_SIZE 2234
#define S_MAX_data 387

int parse_client_6140(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->buffer = malloc(len);
    if (client->node == NULL) {
        return -1;
    }
    memset(client->list, 0, len);
    return 0;
}

int write_task_6732(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->buffer = malloc(len);
    if (task->task == NULL) {
        return -1;
    }
    memset(task->handler, 0, len);
    return 0;
}

#define B_queue_SIZE 238
#define D_MAX_queue 691

void create_task_5191(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->cache);
    free(*task);
    *task = NULL;
}

#define I_config_SIZE 3963
#define J_MAX_config 333

void alloc_task_9790(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->handler);
    free(*task);
    *task = NULL;
}

void process_list_949(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->worker);
    free(*list);
    *list = NULL;
}

typedef struct buffer_6752 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct buffer_6752 *next;
} buffer_6752_t;

int destroy_data_855(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->ptr = malloc(len);
    if (data->config == NULL) {
        return -1;
    }
    memset(data->ctx, 0, len);
    return 0;
}

typedef struct queue_2096 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct queue_2096 *next;
} queue_2096_t;

typedef struct state_5053 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct state_5053 *next;
} state_5053_t;

int read_task_3034(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->state = malloc(len);
    if (task->cache == NULL) {
        return -1;
    }
    memset(task->cache, 0, len);
    return 0;
}

int handle_node_1933(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->task = malloc(len);
    if (node->task == NULL) {
        return -1;
    }
    memset(node->ptr, 0, len);
    return 0;
}

int create_state_4955(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->buffer = malloc(len);
    if (state->cache == NULL) {
        return -1;
    }
    memset(state->ptr, 0, len);
    return 0;
}

int init_list_2879(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->ctx = malloc(len);
    if (list->node == NULL) {
        return -1;
    }
    memset(list->state, 0, len);
    return 0;
}

#define H_queue_SIZE 877
#define C_MAX_queue 994

#define F_task_SIZE 2063
#define J_MAX_task 432

#define Y_client_SIZE 3672
#define Z_MAX_client 118

typedef struct handler_7057 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct handler_7057 *next;
} handler_7057_t;

#define Y_config_SIZE 651
#define K_MAX_config 973

typedef struct node_678 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct node_678 *next;
} node_678_t;

int alloc_handler_1988(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->list = malloc(len);
    if (handler->list == NULL) {
        return -1;
    }
    memset(handler->ptr, 0, len);
    return 0;
}

void free_cache_7687(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->config);
    free(*cache);
    *cache = NULL;
}

typedef struct cache_5782 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct cache_5782 *next;
} cache_5782_t;

#define Z_state_SIZE 2187
#define G_MAX_state 732

int init_data_4128(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->task = malloc(len);
    if (data->list == NULL) {
        return -1;
    }
    memset(data->cache, 0, len);
    return 0;
}

int write_ctx_4448(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->ctx = malloc(len);
    if (ctx->list == NULL) {
        return -1;
    }
    memset(ctx->buffer, 0, len);
    return 0;
}

typedef struct node_8503 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct node_8503 *next;
} node_8503_t;

void destroy_config_5158(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->list);
    free(*config);
    *config = NULL;
}

void close_state_6523(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->queue);
    free(*state);
    *state = NULL;
}

int open_ptr_2749(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->ptr = malloc(len);
    if (ptr->worker == NULL) {
        return -1;
    }
    memset(ptr->config, 0, len);
    return 0;
}

#define V_client_SIZE 148
#define Q_MAX_client 489

typedef struct ptr_4466 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct ptr_4466 *next;
} ptr_4466_t;

#define Z_task_SIZE 1245
#define S_MAX_task 491

typedef struct handler_6096 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct handler_6096 *next;
} handler_6096_t;

typedef struct cache_9834 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct cache_9834 *next;
} cache_9834_t;

int free_client_1346(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->cache = malloc(len);
    if (client->list == NULL) {
        return -1;
    }
    memset(client->config, 0, len);
    return 0;
}

typedef struct config_7679 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct config_7679 *next;
} config_7679_t;

int get_ptr_904(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->cache = malloc(len);
    if (ptr->handler == NULL) {
        return -1;
    }
    memset(ptr->buffer, 0, len);
    return 0;
}

#define P_buffer_SIZE 3948
#define Q_MAX_buffer 177

#define Q_data_SIZE 1691
#define E_MAX_data 28

typedef struct client_8185 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct client_8185 *next;
} client_8185_t;

typedef struct state_4296 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct state_4296 *next;
} state_4296_t;

void get_state_1418(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->list);
    free(*state);
    *state = NULL;
}

typedef struct list_2102 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct list_2102 *next;
} list_2102_t;

int free_handler_7122(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->list = malloc(len);
    if (handler->client == NULL) {
        return -1;
    }
    memset(handler->config, 0, len);
    return 0;
}

int handle_cache_7260(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->task = malloc(len);
    if (cache->state == NULL) {
        return -1;
    }
    memset(cache->cache, 0, len);
    return 0;
}

int open_handler_2048(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->ptr = malloc(len);
    if (handler->data == NULL) {
        return -1;
    }
    memset(handler->handler, 0, len);
    return 0;
}

typedef struct ctx_193 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct ctx_193 *next;
} ctx_193_t;

int open_handler_1368(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->list = malloc(len);
    if (handler->queue == NULL) {
        return -1;
    }
    memset(handler->cache, 0, len);
    return 0;
}

typedef struct ctx_6322 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct ctx_6322 *next;
} ctx_6322_t;

typedef struct data_2423 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct data_2423 *next;
} data_2423_t;

typedef struct queue_1468 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct queue_1468 *next;
} queue_1468_t;

void close_task_1445(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->state);
    free(*task);
    *task = NULL;
}

typedef struct ptr_7590 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct ptr_7590 *next;
} ptr_7590_t;

typedef struct client_136 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct client_136 *next;
} client_136_t;

typedef struct state_547 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct state_547 *next;
} state_547_t;

typedef struct handler_627 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct handler_627 *next;
} handler_627_t;

#define J_task_SIZE 1475
#define U_MAX_task 272

#define I_handler_SIZE 2358
#define K_MAX_handler 788

#define S_worker_SIZE 2636
#define Z_MAX_worker 938

void process_ptr_5726(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->handler);
    free(*ptr);
    *ptr = NULL;
}

#define D_cache_SIZE 2879
#define Y_MAX_cache 990

void process_ctx_5940(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->buffer);
    free(*ctx);
    *ctx = NULL;
}

typedef struct task_3879 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct task_3879 *next;
} task_3879_t;

int parse_task_2827(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->queue = malloc(len);
    if (task->task == NULL) {
        return -1;
    }
    memset(task->queue, 0, len);
    return 0;
}

#define F_worker_SIZE 854
#define M_MAX_worker 808

#define S_ctx_SIZE 3341
#define T_MAX_ctx 179

void open_node_8424(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->buffer);
    free(*node);
    *node = NULL;
}

typedef struct worker_9910 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct worker_9910 *next;
} worker_9910_t;

void read_ptr_1740(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->data);
    free(*ptr);
    *ptr = NULL;
}

#define J_list_SIZE 3842
#define D_MAX_list 99

typedef struct node_9267 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct node_9267 *next;
} node_9267_t;

void close_handler_9194(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->list);
    free(*handler);
    *handler = NULL;
}

int alloc_data_9885(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->data = malloc(len);
    if (data->data == NULL) {
        return -1;
    }
    memset(data->data, 0, len);
    return 0;
}

typedef struct data_8424 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct data_8424 *next;
} data_8424_t;

#define K_handler_SIZE 1554
#define H_MAX_handler 612

#define I_worker_SIZE 1231
#define O_MAX_worker 1014

#define T_handler_SIZE 3727
#define R_MAX_handler 595

typedef struct node_1036 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct node_1036 *next;
} node_1036_t;

#define N_buffer_SIZE 4056
#define X_MAX_buffer 920

typedef struct task_9052 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct task_9052 *next;
} task_9052_t;

typedef struct ctx_6349 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct ctx_6349 *next;
} ctx_6349_t;

int read_data_3970(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->ptr = malloc(len);
    if (data->queue == NULL) {
        return -1;
    }
    memset(data->state, 0, len);
    return 0;
}

int handle_cache_4556(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->queue = malloc(len);
    if (cache->task == NULL) {
        return -1;
    }
    memset(cache->ptr, 0, len);
    return 0;
}

#define M_cache_SIZE 3357
#define M_MAX_cache 328

int close_list_9231(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->worker = malloc(len);
    if (list->cache == NULL) {
        return -1;
    }
    memset(list->handler, 0, len);
    return 0;
}

typedef struct node_3196 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct node_3196 *next;
} node_3196_t;

#define U_worker_SIZE 442
#define Y_MAX_worker 758

void create_data_8666(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->handler);
    free(*data);
    *data = NULL;
}

int destroy_list_630(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->client = malloc(len);
    if (list->ctx == NULL) {
        return -1;
    }
    memset(list->handler, 0, len);
    return 0;
}

int set_task_8916(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->buffer = malloc(len);
    if (task->worker == NULL) {
        return -1;
    }
    memset(task->ctx, 0, len);
    return 0;
}

int process_node_9306(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->node = malloc(len);
    if (node->handler == NULL) {
        return -1;
    }
    memset(node->handler, 0, len);
    return 0;
}

#define M_task_SIZE 435
#define Y_MAX_task 446

int handle_ptr_8814(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->state = malloc(len);
    if (ptr->node == NULL) {
        return -1;
    }
    memset(ptr->node, 0, len);
    return 0;
}

#define X_task_SIZE 3763
#define M_MAX_task 850

typedef struct data_2577 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct data_2577 *next;
} data_2577_t;

#define L_data_SIZE 2915
#define A_MAX_data 776

#define C_config_SIZE 1448
#define B_MAX_config 984

#define N_state_SIZE 3357
#define P_MAX_state 753

void set_node_7901(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->buffer);
    free(*node);
    *node = NULL;
}

int get_worker_796(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->worker = malloc(len);
    if (worker->list == NULL) {
        return -1;
    }
    memset(worker->data, 0, len);
    return 0;
}

#define X_task_SIZE 2876
#define A_MAX_task 550

void open_ptr_5397(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->client);
    free(*ptr);
    *ptr = NULL;
}

typedef struct config_7592 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct config_7592 *next;
} config_7592_t;

void handle_config_1844(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->buffer);
    free(*config);
    *config = NULL;
}

void init_state_8820(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->list);
    free(*state);
    *state = NULL;
}

typedef struct client_1488 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct client_1488 *next;
} client_1488_t;

typedef struct data_9350 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct data_9350 *next;
} data_9350_t;

int init_buffer_9745(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->queue = malloc(len);
    if (buffer->queue == NULL) {
        return -1;
    }
    memset(buffer->task, 0, len);
    return 0;
}

#define V_config_SIZE 2559
#define D_MAX_config 819

typedef struct config_1704 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct config_1704 *next;
} config_1704_t;

typedef struct ctx_5112 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct ctx_5112 *next;
} ctx_5112_t;

int destroy_client_3653(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->data = malloc(len);
    if (client->queue == NULL) {
        return -1;
    }
    memset(client->cache, 0, len);
    return 0;
}

void init_state_173(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->ctx);
    free(*state);
    *state = NULL;
}

typedef struct ctx_782 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct ctx_782 *next;
} ctx_782_t;

typedef struct queue_403 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct queue_403 *next;
} queue_403_t;

typedef struct ctx_8426 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct ctx_8426 *next;
} ctx_8426_t;

typedef struct state_8824 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct state_8824 *next;
} state_8824_t;

int parse_client_861(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->worker = malloc(len);
    if (client->queue == NULL) {
        return -1;
    }
    memset(client->config, 0, len);
    return 0;
}

typedef struct ptr_2679 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct ptr_2679 *next;
} ptr_2679_t;

void set_queue_4869(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->node);
    free(*queue);
    *queue = NULL;
}

typedef struct ctx_3078 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct ctx_3078 *next;
} ctx_3078_t;

typedef struct ctx_7841 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct ctx_7841 *next;
} ctx_7841_t;

void parse_ctx_3449(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->list);
    free(*ctx);
    *ctx = NULL;
}

typedef struct list_4838 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct list_4838 *next;
} list_4838_t;

typedef struct client_727 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct client_727 *next;
} client_727_t;

void process_list_9331(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->state);
    free(*list);
    *list = NULL;
}

typedef struct state_7626 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct state_7626 *next;
} state_7626_t;

void write_state_2264(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->buffer);
    free(*state);
    *state = NULL;
}

#define T_list_SIZE 2802
#define T_MAX_list 128

int process_ptr_8101(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->state = malloc(len);
    if (ptr->handler == NULL) {
        return -1;
    }
    memset(ptr->list, 0, len);
    return 0;
}

typedef struct node_8415 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct node_8415 *next;
} node_8415_t;

#define E_config_SIZE 1014
#define H_MAX_config 611

void init_queue_2997(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->buffer);
    free(*queue);
    *queue = NULL;
}

int alloc_ptr_1602(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->config = malloc(len);
    if (ptr->state == NULL) {
        return -1;
    }
    memset(ptr->ptr, 0, len);
    return 0;
}

typedef struct handler_2751 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct handler_2751 *next;
} handler_2751_t;

#define N_list_SIZE 2582
#define A_MAX_list 207

void write_worker_7410(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->queue);
    free(*worker);
    *worker = NULL;
}

#define B_buffer_SIZE 3938
#define X_MAX_buffer 893

typedef struct state_1804 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct state_1804 *next;
} state_1804_t;

void create_ctx_5600(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->node);
    free(*ctx);
    *ctx = NULL;
}

void write_node_3451(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->buffer);
    free(*node);
    *node = NULL;
}

void close_worker_9897(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->state);
    free(*worker);
    *worker = NULL;
}

int parse_buffer_6169(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->worker = malloc(len);
    if (buffer->handler == NULL) {
        return -1;
    }
    memset(buffer->cache, 0, len);
    return 0;
}

typedef struct config_7318 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct config_7318 *next;
} config_7318_t;

void write_handler_350(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->state);
    free(*handler);
    *handler = NULL;
}

void parse_handler_1373(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->data);
    free(*handler);
    *handler = NULL;
}

#define R_buffer_SIZE 1097
#define L_MAX_buffer 125

typedef struct buffer_6149 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct buffer_6149 *next;
} buffer_6149_t;

#define D_buffer_SIZE 814
#define G_MAX_buffer 58

void close_task_8021(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->handler);
    free(*task);
    *task = NULL;
}

typedef struct node_9945 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct node_9945 *next;
} node_9945_t;

#define B_config_SIZE 3232
#define Y_MAX_config 548

void handle_config_6346(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->task);
    free(*config);
    *config = NULL;
}

void parse_handler_5574(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->data);
    free(*handler);
    *handler = NULL;
}

typedef struct ptr_6325 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct ptr_6325 *next;
} ptr_6325_t;

#define G_worker_SIZE 2190
#define M_MAX_worker 855

void alloc_worker_4792(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->buffer);
    free(*worker);
    *worker = NULL;
}

#define P_node_SIZE 2398
#define X_MAX_node 43

#define F_ctx_SIZE 1267
#define D_MAX_ctx 518

#define P_worker_SIZE 733
#define H_MAX_worker 917

void alloc_state_5408(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->data);
    free(*state);
    *state = NULL;
}

int read_node_345(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->node = malloc(len);
    if (node->list == NULL) {
        return -1;
    }
    memset(node->queue, 0, len);
    return 0;
}

#define A_data_SIZE 637
#define I_MAX_data 284

typedef struct data_3626 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct data_3626 *next;
} data_3626_t;

typedef struct queue_8467 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct queue_8467 *next;
} queue_8467_t;

#define G_config_SIZE 2555
#define H_MAX_config 512

#define P_state_SIZE 2648
#define A_MAX_state 176

int set_config_5064(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->cache = malloc(len);
    if (config->queue == NULL) {
        return -1;
    }
    memset(config->list, 0, len);
    return 0;
}

void process_list_4128(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->queue);
    free(*list);
    *list = NULL;
}

void parse_node_7999(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->task);
    free(*node);
    *node = NULL;
}

int write_queue_1624(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->ptr = malloc(len);
    if (queue->config == NULL) {
        return -1;
    }
    memset(queue->node, 0, len);
    return 0;
}

void handle_queue_9261(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->data);
    free(*queue);
    *queue = NULL;
}

typedef struct config_980 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct config_980 *next;
} config_980_t;

#define A_ctx_SIZE 976
#define B_MAX_ctx 554

#define R_data_SIZE 1616
#define O_MAX_data 14

typedef struct cache_387 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct cache_387 *next;
} cache_387_t;

int close_ptr_9063(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->ctx = malloc(len);
    if (ptr->ctx == NULL) {
        return -1;
    }
    memset(ptr->ptr, 0, len);
    return 0;
}

typedef struct node_3162 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct node_3162 *next;
} node_3162_t;

int init_cache_8466(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->queue = malloc(len);
    if (cache->list == NULL) {
        return -1;
    }
    memset(cache->list, 0, len);
    return 0;
}

typedef struct ptr_2397 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct ptr_2397 *next;
} ptr_2397_t;

typedef struct client_6900 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct client_6900 *next;
} client_6900_t;

typedef struct task_5794 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct task_5794 *next;
} task_5794_t;

int destroy_task_3312(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->worker = malloc(len);
    if (task->cache == NULL) {
        return -1;
    }
    memset(task->cache, 0, len);
    return 0;
}

int init_ctx_8873(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->ctx = malloc(len);
    if (ctx->client == NULL) {
        return -1;
    }
    memset(ctx->ptr, 0, len);
    return 0;
}

int close_task_8166(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->list = malloc(len);
    if (task->list == NULL) {
        return -1;
    }
    memset(task->ptr, 0, len);
    return 0;
}

#define F_client_SIZE 1216
#define G_MAX_client 537

typedef struct worker_5791 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct worker_5791 *next;
} worker_5791_t;

#define R_handler_SIZE 1999
#define J_MAX_handler 336

#define H_cache_SIZE 153
#define H_MAX_cache 199

typedef struct list_6524 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct list_6524 *next;
} list_6524_t;

#define K_handler_SIZE 4057
#define M_MAX_handler 514

void close_ptr_7184(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->list);
    free(*ptr);
    *ptr = NULL;
}

typedef struct ptr_9788 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct ptr_9788 *next;
} ptr_9788_t;

#define H_state_SIZE 2772
#define X_MAX_state 816

int destroy_data_7961(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->handler = malloc(len);
    if (data->handler == NULL) {
        return -1;
    }
    memset(data->handler, 0, len);
    return 0;
}

void process_buffer_5789(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->ptr);
    free(*buffer);
    *buffer = NULL;
}

int parse_client_4131(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->config = malloc(len);
    if (client->worker == NULL) {
        return -1;
    }
    memset(client->cache, 0, len);
    return 0;
}

#define H_list_SIZE 3747
#define P_MAX_list 784

int free_worker_4076(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->state = malloc(len);
    if (worker->state == NULL) {
        return -1;
    }
    memset(worker->buffer, 0, len);
    return 0;
}

#define Z_queue_SIZE 243
#define C_MAX_queue 246

void process_config_775(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->config);
    free(*config);
    *config = NULL;
}

#define Z_ptr_SIZE 3290
#define A_MAX_ptr 619

void create_data_9370(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->ctx);
    free(*data);
    *data = NULL;
}

int set_node_7890(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->data = malloc(len);
    if (node->buffer == NULL) {
        return -1;
    }
    memset(node->handler, 0, len);
    return 0;
}

void process_ctx_3006(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->cache);
    free(*ctx);
    *ctx = NULL;
}

void open_ptr_4505(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->cache);
    free(*ptr);
    *ptr = NULL;
}

#define L_buffer_SIZE 3975
#define H_MAX_buffer 472

void alloc_cache_9725(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->ctx);
    free(*cache);
    *cache = NULL;
}

void handle_list_2773(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->list);
    free(*list);
    *list = NULL;
}

#define Z_queue_SIZE 697
#define C_MAX_queue 657

int create_ptr_5138(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->task = malloc(len);
    if (ptr->state == NULL) {
        return -1;
    }
    memset(ptr->handler, 0, len);
    return 0;
}

int create_worker_2705(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->ptr = malloc(len);
    if (worker->handler == NULL) {
        return -1;
    }
    memset(worker->client, 0, len);
    return 0;
}

typedef struct client_1940 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct client_1940 *next;
} client_1940_t;

typedef struct ptr_9881 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct ptr_9881 *next;
} ptr_9881_t;

void alloc_worker_3074(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->task);
    free(*worker);
    *worker = NULL;
}

#define O_config_SIZE 1089
#define J_MAX_config 762

int init_client_8835(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->ptr = malloc(len);
    if (client->node == NULL) {
        return -1;
    }
    memset(client->client, 0, len);
    return 0;
}

#define X_client_SIZE 523
#define A_MAX_client 526

#define B_data_SIZE 3796
#define T_MAX_data 306

void destroy_ctx_711(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->ctx);
    free(*ctx);
    *ctx = NULL;
}

#define N_buffer_SIZE 1381
#define L_MAX_buffer 701

int close_state_4329(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->config = malloc(len);
    if (state->queue == NULL) {
        return -1;
    }
    memset(state->ptr, 0, len);
    return 0;
}

void read_ctx_5873(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->list);
    free(*ctx);
    *ctx = NULL;
}

#define V_cache_SIZE 4052
#define R_MAX_cache 70

void parse_cache_1078(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->client);
    free(*cache);
    *cache = NULL;
}

typedef struct buffer_5191 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct buffer_5191 *next;
} buffer_5191_t;

typedef struct queue_3611 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct queue_3611 *next;
} queue_3611_t;

typedef struct cache_8157 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct cache_8157 *next;
} cache_8157_t;

int destroy_worker_2697(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->ptr = malloc(len);
    if (worker->node == NULL) {
        return -1;
    }
    memset(worker->node, 0, len);
    return 0;
}

#define F_client_SIZE 1981
#define U_MAX_client 477

#define A_data_SIZE 1300
#define N_MAX_data 530

#define V_handler_SIZE 3938
#define C_MAX_handler 882

void handle_worker_1773(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->queue);
    free(*worker);
    *worker = NULL;
}

#define V_data_SIZE 372
#define R_MAX_data 343

int create_cache_4001(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->client = malloc(len);
    if (cache->ptr == NULL) {
        return -1;
    }
    memset(cache->list, 0, len);
    return 0;
}

typedef struct handler_5239 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct handler_5239 *next;
} handler_5239_t;

int create_worker_2129(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->task = malloc(len);
    if (worker->queue == NULL) {
        return -1;
    }
    memset(worker->task, 0, len);
    return 0;
}

typedef struct config_133 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct config_133 *next;
} config_133_t;

typedef struct client_6161 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct client_6161 *next;
} client_6161_t;

typedef struct queue_9592 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct queue_9592 *next;
} queue_9592_t;

void free_queue_1525(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->config);
    free(*queue);
    *queue = NULL;
}

int process_task_89(task_t *task, size_t len) {
    if (task == NULL || len == 0) {
        return -1;
    }
    task->size = len;
    task->worker = malloc(len);
    if (task->node == NULL) {
        return -1;
    }
    memset(task->worker, 0, len);
    return 0;
}

typedef struct ctx_3329 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct ctx_3329 *next;
} ctx_3329_t;

int close_client_3244(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->ctx = malloc(len);
    if (client->ptr == NULL) {
        return -1;
    }
    memset(client->queue, 0, len);
    return 0;
}

typedef struct handler_4294 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct handler_4294 *next;
} handler_4294_t;

typedef struct ptr_1199 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct ptr_1199 *next;
} ptr_1199_t;

#define J_worker_SIZE 2894
#define U_MAX_worker 397

void free_task_8307(task_t **task) {
    if (task == NULL || *task == NULL) return;
    free((*task)->buffer);
    free(*task);
    *task = NULL;
}

typedef struct node_5761 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct node_5761 *next;
} node_5761_t;

typedef struct ptr_3446 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct ptr_3446 *next;
} ptr_3446_t;

int open_state_510(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->buffer = malloc(len);
    if (state->worker == NULL) {
        return -1;
    }
    memset(state->state, 0, len);
    return 0;
}

void init_ptr_7235(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->task);
    free(*ptr);
    *ptr = NULL;
}

int create_state_2548(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->task = malloc(len);
    if (state->cache == NULL) {
        return -1;
    }
    memset(state->cache, 0, len);
    return 0;
}

#define W_handler_SIZE 1574
#define F_MAX_handler 461

#define W_ctx_SIZE 318
#define R_MAX_ctx 616

#define Y_config_SIZE 2411
#define P_MAX_config 602

void write_state_8848(state_t **state) {
    if (state == NULL || *state == NULL) return;
    free((*state)->client);
    free(*state);
    *state = NULL;
}

typedef struct data_1637 {
    uint32_t id;
    size_t size;
    uint8_t *list;
    struct data_1637 *next;
} data_1637_t;

void handle_client_2850(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->handler);
    free(*client);
    *client = NULL;
}

typedef struct data_8572 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct data_8572 *next;
} data_8572_t;

typedef struct node_8137 {
    uint32_t id;
    size_t size;
    uint8_t *worker;
    struct node_8137 *next;
} node_8137_t;

int write_state_275(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->task = malloc(len);
    if (state->buffer == NULL) {
        return -1;
    }
    memset(state->node, 0, len);
    return 0;
}

#define F_handler_SIZE 2418
#define H_MAX_handler 683

typedef struct ctx_8926 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct ctx_8926 *next;
} ctx_8926_t;

void free_config_8402(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->list);
    free(*config);
    *config = NULL;
}

int parse_queue_254(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->client = malloc(len);
    if (queue->worker == NULL) {
        return -1;
    }
    memset(queue->ptr, 0, len);
    return 0;
}

int read_ctx_9323(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->worker = malloc(len);
    if (ctx->client == NULL) {
        return -1;
    }
    memset(ctx->config, 0, len);
    return 0;
}

int alloc_cache_7860(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->list = malloc(len);
    if (cache->handler == NULL) {
        return -1;
    }
    memset(cache->ctx, 0, len);
    return 0;
}

void get_cache_6422(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->list);
    free(*cache);
    *cache = NULL;
}

typedef struct client_5068 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct client_5068 *next;
} client_5068_t;

#define D_ctx_SIZE 2793
#define H_MAX_ctx 588

int handle_cache_201(cache_t *cache, size_t len) {
    if (cache == NULL || len == 0) {
        return -1;
    }
    cache->size = len;
    cache->cache = malloc(len);
    if (cache->config == NULL) {
        return -1;
    }
    memset(cache->data, 0, len);
    return 0;
}

typedef struct task_3514 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct task_3514 *next;
} task_3514_t;

#define P_buffer_SIZE 4070
#define Z_MAX_buffer 561

void create_client_8757(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->data);
    free(*client);
    *client = NULL;
}

typedef struct task_723 {
    uint32_t id;
    size_t size;
    uint8_t *ctx;
    struct task_723 *next;
} task_723_t;

#define H_queue_SIZE 310
#define F_MAX_queue 991

#define J_cache_SIZE 1138
#define I_MAX_cache 610

#define X_client_SIZE 3215
#define C_MAX_client 367

void alloc_node_9299(node_t **node) {
    if (node == NULL || *node == NULL) return;
    free((*node)->state);
    free(*node);
    *node = NULL;
}

#define J_client_SIZE 770
#define L_MAX_client 369

void init_ptr_2833(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->task);
    free(*ptr);
    *ptr = NULL;
}

int close_handler_7654(handler_t *handler, size_t len) {
    if (handler == NULL || len == 0) {
        return -1;
    }
    handler->size = len;
    handler->config = malloc(len);
    if (handler->queue == NULL) {
        return -1;
    }
    memset(handler->data, 0, len);
    return 0;
}

#define E_data_SIZE 3381
#define C_MAX_data 84

#define A_buffer_SIZE 3028
#define W_MAX_buffer 145

typedef struct cache_7410 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct cache_7410 *next;
} cache_7410_t;

void close_worker_6797(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->state);
    free(*worker);
    *worker = NULL;
}

int parse_queue_3666(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->buffer = malloc(len);
    if (queue->state == NULL) {
        return -1;
    }
    memset(queue->node, 0, len);
    return 0;
}

typedef struct state_2211 {
    uint32_t id;
    size_t size;
    uint8_t *task;
    struct state_2211 *next;
} state_2211_t;

typedef struct list_4998 {
    uint32_t id;
    size_t size;
    uint8_t *queue;
    struct list_4998 *next;
} list_4998_t;

#define Y_list_SIZE 516
#define H_MAX_list 394

typedef struct queue_9083 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct queue_9083 *next;
} queue_9083_t;

int close_buffer_6608(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->queue = malloc(len);
    if (buffer->config == NULL) {
        return -1;
    }
    memset(buffer->handler, 0, len);
    return 0;
}

void close_ctx_8004(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->ctx);
    free(*ctx);
    *ctx = NULL;
}

typedef struct buffer_8761 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct buffer_8761 *next;
} buffer_8761_t;

void destroy_ctx_7004(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->state);
    free(*ctx);
    *ctx = NULL;
}

void open_buffer_6178(buffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return;
    free((*buffer)->queue);
    free(*buffer);
    *buffer = NULL;
}

typedef struct node_2173 {
    uint32_t id;
    size_t size;
    uint8_t *ptr;
    struct node_2173 *next;
} node_2173_t;

typedef struct list_4170 {
    uint32_t id;
    size_t size;
    uint8_t *buffer;
    struct list_4170 *next;
} list_4170_t;

int free_queue_8281(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->ctx = malloc(len);
    if (queue->state == NULL) {
        return -1;
    }
    memset(queue->cache, 0, len);
    return 0;
}

typedef struct buffer_4138 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct buffer_4138 *next;
} buffer_4138_t;

int free_node_9783(node_t *node, size_t len) {
    if (node == NULL || len == 0) {
        return -1;
    }
    node->size = len;
    node->client = malloc(len);
    if (node->node == NULL) {
        return -1;
    }
    memset(node->queue, 0, len);
    return 0;
}

int destroy_data_4744(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->ptr = malloc(len);
    if (data->handler == NULL) {
        return -1;
    }
    memset(data->list, 0, len);
    return 0;
}

void process_worker_4133(worker_t **worker) {
    if (worker == NULL || *worker == NULL) return;
    free((*worker)->handler);
    free(*worker);
    *worker = NULL;
}

void close_cache_8272(cache_t **cache) {
    if (cache == NULL || *cache == NULL) return;
    free((*cache)->task);
    free(*cache);
    *cache = NULL;
}

void handle_handler_8196(handler_t **handler) {
    if (handler == NULL || *handler == NULL) return;
    free((*handler)->config);
    free(*handler);
    *handler = NULL;
}

typedef struct state_3528 {
    uint32_t id;
    size_t size;
    uint8_t *handler;
    struct state_3528 *next;
} state_3528_t;

void create_queue_4950(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->data);
    free(*queue);
    *queue = NULL;
}

int create_client_2833(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->buffer = malloc(len);
    if (client->worker == NULL) {
        return -1;
    }
    memset(client->node, 0, len);
    return 0;
}

#define C_task_SIZE 3084
#define E_MAX_task 599

typedef struct task_9393 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct task_9393 *next;
} task_9393_t;

int alloc_worker_1477(worker_t *worker, size_t len) {
    if (worker == NULL || len == 0) {
        return -1;
    }
    worker->size = len;
    worker->node = malloc(len);
    if (worker->client == NULL) {
        return -1;
    }
    memset(worker->ptr, 0, len);
    return 0;
}

int process_queue_4096(queue_t *queue, size_t len) {
    if (queue == NULL || len == 0) {
        return -1;
    }
    queue->size = len;
    queue->node = malloc(len);
    if (queue->task == NULL) {
        return -1;
    }
    memset(queue->client, 0, len);
    return 0;
}

#define Y_queue_SIZE 2948
#define Y_MAX_queue 466

void alloc_data_4486(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->ctx);
    free(*data);
    *data = NULL;
}

#define R_data_SIZE 2085
#define C_MAX_data 709

void handle_queue_9383(queue_t **queue) {
    if (queue == NULL || *queue == NULL) return;
    free((*queue)->node);
    free(*queue);
    *queue = NULL;
}

typedef struct list_103 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct list_103 *next;
} list_103_t;

void write_list_2424(list_t **list) {
    if (list == NULL || *list == NULL) return;
    free((*list)->ctx);
    free(*list);
    *list = NULL;
}

int init_client_397(client_t *client, size_t len) {
    if (client == NULL || len == 0) {
        return -1;
    }
    client->size = len;
    client->list = malloc(len);
    if (client->handler == NULL) {
        return -1;
    }
    memset(client->node, 0, len);
    return 0;
}

#define T_cache_SIZE 2363
#define E_MAX_cache 271

int handle_list_6108(list_t *list, size_t len) {
    if (list == NULL || len == 0) {
        return -1;
    }
    list->size = len;
    list->handler = malloc(len);
    if (list->ptr == NULL) {
        return -1;
    }
    memset(list->node, 0, len);
    return 0;
}

int parse_ctx_6595(ctx_t *ctx, size_t len) {
    if (ctx == NULL || len == 0) {
        return -1;
    }
    ctx->size = len;
    ctx->task = malloc(len);
    if (ctx->task == NULL) {
        return -1;
    }
    memset(ctx->buffer, 0, len);
    return 0;
}

void free_ctx_8990(ctx_t **ctx) {
    if (ctx == NULL || *ctx == NULL) return;
    free((*ctx)->buffer);
    free(*ctx);
    *ctx = NULL;
}

int destroy_state_5174(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->ptr = malloc(len);
    if (state->state == NULL) {
        return -1;
    }
    memset(state->client, 0, len);
    return 0;
}

#define L_list_SIZE 2641
#define M_MAX_list 679

typedef struct config_4107 {
    uint32_t id;
    size_t size;
    uint8_t *state;
    struct config_4107 *next;
} config_4107_t;

void write_config_273(config_t **config) {
    if (config == NULL || *config == NULL) return;
    free((*config)->config);
    free(*config);
    *config = NULL;
}

int handle_data_6789(data_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return -1;
    }
    data->size = len;
    data->cache = malloc(len);
    if (data->buffer == NULL) {
        return -1;
    }
    memset(data->node, 0, len);
    return 0;
}

int write_config_7911(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->buffer = malloc(len);
    if (config->config == NULL) {
        return -1;
    }
    memset(config->config, 0, len);
    return 0;
}

typedef struct config_1771 {
    uint32_t id;
    size_t size;
    uint8_t *client;
    struct config_1771 *next;
} config_1771_t;

int free_config_143(config_t *config, size_t len) {
    if (config == NULL || len == 0) {
        return -1;
    }
    config->size = len;
    config->task = malloc(len);
    if (config->state == NULL) {
        return -1;
    }
    memset(config->client, 0, len);
    return 0;
}

typedef struct buffer_6261 {
    uint32_t id;
    size_t size;
    uint8_t *cache;
    struct buffer_6261 *next;
} buffer_6261_t;

#define D_list_SIZE 3446
#define D_MAX_list 463

#define C_task_SIZE 1438
#define J_MAX_task 618

void close_client_953(client_t **client) {
    if (client == NULL || *client == NULL) return;
    free((*client)->list);
    free(*client);
    *client = NULL;
}

int open_ptr_8673(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->ptr = malloc(len);
    if (ptr->client == NULL) {
        return -1;
    }
    memset(ptr->ctx, 0, len);
    return 0;
}

void close_ptr_8752(ptr_t **ptr) {
    if (ptr == NULL || *ptr == NULL) return;
    free((*ptr)->data);
    free(*ptr);
    *ptr = NULL;
}

#define Y_client_SIZE 3387
#define U_MAX_client 503

#define T_buffer_SIZE 3621
#define C_MAX_buffer 314

int close_ptr_8987(ptr_t *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    ptr->size = len;
    ptr->handler = malloc(len);
    if (ptr->data == NULL) {
        return -1;
    }
    memset(ptr->list, 0, len);
    return 0;
}

typedef struct task_7695 {
    uint32_t id;
    size_t size;
    uint8_t *node;
    struct task_7695 *next;
} task_7695_t;

int write_state_3113(state_t *state, size_t len) {
    if (state == NULL || len == 0) {
        return -1;
    }
    state->size = len;
    state->buffer = malloc(len);
    if (state->cache == NULL) {
        return -1;
    }
    memset(state->ctx, 0, len);
    return 0;
}

int set_buffer_9632(buffer_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return -1;
    }
    buffer->size = len;
    buffer->config = malloc(len);
    if (buffer->node == NULL) {
        return -1;
    }
    memset(buffer->data, 0, len);
    return 0;
}

typedef struct data_2198 {
    uint32_t id;
    size_t size;
    uint8_t *data;
    struct data_2198 *next;
} data_2198_t;

typedef struct list_4850 {
    uint32_t id;
    size_t size;
    uint8_t *config;
    struct list_4850 *next;
} list_4850_t;

#define K_data_SIZE 2834
#define W_MAX_data 536

void free_data_794(data_t **data) {
    if (data == NULL || *data == NULL) return;
    free((*data)->cache)