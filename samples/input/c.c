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

int create_buffer_5133(buffer_t *buffer, size