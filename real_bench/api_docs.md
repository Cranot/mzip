# API Documentation

This document describes the REST API endpoints for our application.

## Table of Contents

- [response API](#close-api)
- [node API](#size-api)
- [name API](#mode-api)
- [queue API](#buffer-api)
- [set API](#buffer-api)
- [client API](#value-api)
- [client API](#warning-api)
- [message API](#name-api)
- [trace API](#config-api)
- [start API](#query-api)
- [count API](#info-api)
- [mode API](#count-api)
- [set API](#size-api)
- [name API](#offset-api)
- [tree API](#queue-api)
- [total API](#query-api)
- [id API](#value-api)
- [create API](#error-api)
- [stack API](#create-api)
- [init API](#stop-api)
- [connection API](#offset-api)
- [length API](#error-api)
- [update API](#status-api)
- [close API](#message-api)
- [warning API](#request-api)

## session API

Endpoints for managing size resources.

### GET /api/session

Retrieve a list of session items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/session

Create a new session item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "size_id": "integer"
}
```

## result API

Endpoints for managing message resources.

### GET /api/result

Retrieve a list of result items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/result

Create a new result item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "offset_id": "integer"
}
```

## trace API

Endpoints for managing user resources.

### GET /api/trace

Retrieve a list of trace items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/trace

Create a new trace item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "open_id": "integer"
}
```

## config API

Endpoints for managing total resources.

### GET /api/config

Retrieve a list of config items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/config

Create a new config item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "message_id": "integer"
}
```

## info API

Endpoints for managing type resources.

### GET /api/info

Retrieve a list of info items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/info

Create a new info item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "write_id": "integer"
}
```

## queue API

Endpoints for managing server resources.

### GET /api/queue

Retrieve a list of queue items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/queue

Create a new queue item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "error_id": "integer"
}
```

## info API

Endpoints for managing set resources.

### GET /api/info

Retrieve a list of info items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/info

Create a new info item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "error_id": "integer"
}
```

## close API

Endpoints for managing list resources.

### GET /api/close

Retrieve a list of close items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/close

Create a new close item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "length_id": "integer"
}
```

## buffer API

Endpoints for managing index resources.

### GET /api/buffer

Retrieve a list of buffer items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/buffer

Create a new buffer item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "delete_id": "integer"
}
```

## client API

Endpoints for managing user resources.

### GET /api/client

Retrieve a list of client items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/client

Create a new client item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "update_id": "integer"
}
```

## delete API

Endpoints for managing transaction resources.

### GET /api/delete

Retrieve a list of delete items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/delete

Create a new delete item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "tree_id": "integer"
}
```

## server API

Endpoints for managing queue resources.

### GET /api/server

Retrieve a list of server items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/server

Create a new server item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "set_id": "integer"
}
```

## open API

Endpoints for managing session resources.

### GET /api/open

Retrieve a list of open items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/open

Create a new open item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "delete_id": "integer"
}
```

## read API

Endpoints for managing connection resources.

### GET /api/read

Retrieve a list of read items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/read

Create a new read item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "connection_id": "integer"
}
```

## info API

Endpoints for managing tree resources.

### GET /api/info

Retrieve a list of info items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/info

Create a new info item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "session_id": "integer"
}
```

## request API

Endpoints for managing action resources.

### GET /api/request

Retrieve a list of request items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/request

Create a new request item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "response_id": "integer"
}
```

## update API

Endpoints for managing query resources.

### GET /api/update

Retrieve a list of update items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/update

Create a new update item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "type_id": "integer"
}
```

## close API

Endpoints for managing request resources.

### GET /api/close

Retrieve a list of close items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/close

Create a new close item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "write_id": "integer"
}
```

## update API

Endpoints for managing mode resources.

### GET /api/update

Retrieve a list of update items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/update

Create a new update item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "map_id": "integer"
}
```

## error API

Endpoints for managing write resources.

### GET /api/error

Retrieve a list of error items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/error

Create a new error item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "open_id": "integer"
}
```

## buffer API

Endpoints for managing transaction resources.

### GET /api/buffer

Retrieve a list of buffer items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/buffer

Create a new buffer item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "init_id": "integer"
}
```

## create API

Endpoints for managing info resources.

### GET /api/create

Retrieve a list of create items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/create

Create a new create item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "create_id": "integer"
}
```

## data API

Endpoints for managing server resources.

### GET /api/data

Retrieve a list of data items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/data

Create a new data item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "data_id": "integer"
}
```

## map API

Endpoints for managing name resources.

### GET /api/map

Retrieve a list of map items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/map

Create a new map item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "delete_id": "integer"
}
```

## init API

Endpoints for managing update resources.

### GET /api/init

Retrieve a list of init items.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| page | integer | Page number (default: 1) |
| limit | integer | Items per page (default: 20) |
| sort | string | Sort field |

**Example Response:**

```json
{
  "data": [
    {"id": 1, "name": "example", "created_at": "2024-01-01T00:00:00Z"}
  ],
  "pagination": {"page": 1, "total": 100}
}
```

### POST /api/init

Create a new init item.

**Request Body:**

```json
{
  "name": "string",
  "description": "string",
  "open_id": "integer"
}
```

