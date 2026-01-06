// generators.hpp - Single source of truth for benchmark data generators
// Usage: #include "generators.hpp"
// Then: auto data = gen::python(65536);
#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdint>
#include <vector>
#include <string>
#include <sstream>
#include <random>
#include <functional>
#include <cstring>
#include <iomanip>
#include <algorithm>

namespace gen {

// ============================================================================
// NUMERIC TYPES
// ============================================================================

inline std::vector<uint8_t> timestamps_regular(size_t n) {
    std::vector<uint8_t> out(n);
    uint32_t* p = (uint32_t*)out.data();
    uint32_t t = 1700000000;
    for (size_t i = 0; i < n/4; i++) { p[i] = t; t += 1000; }
    return out;
}

inline std::vector<uint8_t> timestamps_jitter(size_t n) {
    std::mt19937 rng(42);
    std::vector<uint8_t> out(n);
    uint32_t* p = (uint32_t*)out.data();
    uint32_t t = 1700000000;
    for (size_t i = 0; i < n/4; i++) { p[i] = t; t += 1000 + (rng() % 100 - 50); }
    return out;
}

inline std::vector<uint8_t> db_ids(size_t n) {
    std::vector<uint8_t> out(n);
    uint64_t* p = (uint64_t*)out.data();
    uint64_t id = 1000000000000ULL;
    for (size_t i = 0; i < n/8; i++) { p[i] = id; id += 1; }
    return out;
}

inline std::vector<uint8_t> sensor_16bit(size_t n) {
    std::mt19937 rng(42);
    std::vector<uint8_t> out(n);
    uint16_t* p = (uint16_t*)out.data();
    for (size_t i = 0; i < n/2; i++) { p[i] = 2048 + (rng() % 100 - 50); }
    return out;
}

inline std::vector<uint8_t> gps_track(size_t n) {
    std::mt19937 rng(42);
    std::vector<uint8_t> out(n);
    int32_t* p = (int32_t*)out.data();
    int32_t lat = 40000000, lon = -74000000;
    for (size_t i = 0; i < n/8; i++) {
        p[i*2] = lat; p[i*2+1] = lon;
        lat += 100 + (rng() % 20 - 10);
        lon += 150 + (rng() % 20 - 10);
    }
    return out;
}

inline std::vector<uint8_t> floats(size_t n) {
    std::mt19937 rng(42);
    std::vector<uint8_t> out(n);
    double* p = (double*)out.data();
    double t = 20.0;
    for (size_t i = 0; i < n/8; i++) {
        p[i] = t;
        t += 0.01 * (static_cast<int>(rng() % 100) - 50) / 50.0;
    }
    return out;
}

inline std::vector<uint8_t> random_bytes(size_t n) {
    std::mt19937 rng(42);
    std::vector<uint8_t> out(n);
    for (size_t i = 0; i < n; i++) out[i] = rng() % 256;
    return out;
}

inline std::vector<uint8_t> int_array(size_t n) {
    std::mt19937 rng(42);
    std::vector<uint8_t> out(n);
    int32_t* p = (int32_t*)out.data();
    int32_t v = 0;
    for (size_t i = 0; i < n/4; i++) {
        p[i] = v;
        v += 1 + (rng() % 3);
    }
    return out;
}

// ============================================================================
// TEXT TYPES
// ============================================================================

inline std::vector<uint8_t> natural_text(size_t n) {
    std::mt19937 rng(42);
    const char* words[] = {"the", "quick", "brown", "fox", "jumps", "over", "lazy", "dog",
        "and", "runs", "through", "forest", "while", "birds", "sing", "above"};
    std::ostringstream oss;
    while (static_cast<size_t>(oss.tellp()) < n) {
        size_t len = 8 + rng() % 8;
        for (size_t i = 0; i < len; i++) {
            oss << words[rng() % 16];
            if (i < len - 1) oss << " ";
        }
        oss << ". ";
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> access_log(size_t n) {
    std::mt19937 rng(42);
    const char* methods[] = {"GET", "POST", "PUT", "DELETE"};
    const char* paths[] = {"/api/users", "/api/products", "/api/orders", "/api/items"};
    int codes[] = {200, 200, 200, 201, 204, 301, 400, 404, 500};
    std::ostringstream oss;
    int h = 12, m = 0, s = 0;
    while (static_cast<size_t>(oss.tellp()) < n) {
        s++; if (s >= 60) { s = 0; m++; } if (m >= 60) { m = 0; h = (h + 1) % 24; }
        char buf[256];
        snprintf(buf, sizeof(buf),
            "192.168.%d.%d - - [01/Dec/2024:%02d:%02d:%02d +0000] \"%s %s HTTP/1.1\" %d %d\n",
            (int)(rng() % 256), (int)(rng() % 256), h, m, s,
            methods[rng() % 4], paths[rng() % 4], codes[rng() % 9], 100 + (int)(rng() % 50000));
        oss << buf;
    }
    auto s2 = oss.str().substr(0, n);
    return std::vector<uint8_t>(s2.begin(), s2.end());
}

inline std::vector<uint8_t> syslog(size_t n) {
    std::mt19937 rng(42);
    const char* services[] = {"sshd", "nginx", "mysql", "docker"};
    const char* levels[] = {"INFO", "WARN", "ERROR", "DEBUG"};
    const char* hosts[] = {"server01", "server02", "db-primary"};
    std::ostringstream oss;
    int h = 0, m = 0, s = 0;
    while (static_cast<size_t>(oss.tellp()) < n) {
        s++; if (s >= 60) { s = 0; m++; } if (m >= 60) { m = 0; h = (h + 1) % 24; }
        char buf[256];
        snprintf(buf, sizeof(buf), "Dec 01 %02d:%02d:%02d %s %s[%d]: %s connection from %d.%d.%d.%d\n",
            h, m, s, hosts[rng() % 3], services[rng() % 4], (int)(1000 + rng() % 9000),
            levels[rng() % 4], (int)(rng() % 256), (int)(rng() % 256), (int)(rng() % 256), (int)(rng() % 256));
        oss << buf;
    }
    auto s2 = oss.str().substr(0, n);
    return std::vector<uint8_t>(s2.begin(), s2.end());
}

// ============================================================================
// STRUCTURED TEXT
// ============================================================================

inline std::vector<uint8_t> json(size_t n) {
    std::mt19937 rng(42);
    std::ostringstream oss;
    oss << "[\n";
    int i = 0;
    while (static_cast<size_t>(oss.tellp()) + 150 < n) {
        if (i > 0) oss << ",\n";
        oss << "  {\"id\": " << i++ << ", \"name\": \"User" << i << "\", \"email\": \"user" << i << "@example.com\", "
            << "\"age\": " << (20 + rng() % 50) << ", \"active\": " << (rng() % 2 ? "true" : "false") << "}";
    }
    oss << "\n]\n";
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> csv(size_t n) {
    std::mt19937 rng(42);
    std::ostringstream oss;
    oss << "id,name,email,age,salary\n";
    int id = 1;
    while (static_cast<size_t>(oss.tellp()) + 50 < n) {
        oss << id << ",User" << id << ",user" << id << "@example.com," << (20 + rng() % 50) << "," << (30000 + rng() % 70000) << "\n";
        id++;
    }
    auto s = oss.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> sql(size_t n) {
    std::mt19937 rng(42);
    std::ostringstream oss;

    const char* tables[] = {"users", "orders", "products", "customers", "invoices", "payments", "sessions"};
    const char* cols[] = {"id", "name", "email", "status", "created_at", "updated_at", "amount", "quantity"};
    const char* names[] = {"Alice", "Bob", "Carol", "David", "Emma", "Frank", "Grace", "Henry", "Ivy", "Jack"};
    const char* domains[] = {"gmail.com", "outlook.com", "company.org", "example.com", "mail.io"};
    const char* statuses[] = {"active", "pending", "completed", "cancelled", "processing"};
    const int nTables = 7, nCols = 8, nNames = 10, nDomains = 5, nStatuses = 5;

    // Create tables with varied schemas
    for (int t = 0; t < 4; t++) {
        oss << "CREATE TABLE " << tables[t] << " (\n";
        oss << "    id SERIAL PRIMARY KEY,\n";
        for (int c = 0; c < 3 + (int)(rng() % 4); c++) {
            oss << "    " << cols[rng() % nCols] << "_" << c << " VARCHAR(" << (50 + rng() % 200) << ")";
            if (c < 2 + (int)(rng() % 4)) oss << ",";
            oss << "\n";
        }
        oss << ");\n\n";
    }

    int id = 1;
    while (static_cast<size_t>(oss.tellp()) + 200 < n) {
        int pattern = rng() % 10;
        const char* table = tables[rng() % nTables];

        if (pattern < 4) {
            // INSERT with varied data
            oss << "INSERT INTO " << table << " (id, name, email, status) VALUES (";
            oss << id++ << ", '" << names[rng() % nNames] << " " << names[rng() % nNames] << "', '";
            oss << (char)('a' + rng() % 26) << names[rng() % nNames] << (rng() % 1000) << "@" << domains[rng() % nDomains];
            oss << "', '" << statuses[rng() % nStatuses] << "');\n";
        } else if (pattern < 6) {
            // UPDATE
            oss << "UPDATE " << table << " SET status = '" << statuses[rng() % nStatuses];
            oss << "', updated_at = NOW() WHERE id = " << (rng() % 10000) << ";\n";
        } else if (pattern < 8) {
            // SELECT with JOIN
            oss << "SELECT t1.id, t1.name, t2." << cols[rng() % nCols] << " FROM " << table << " t1\n";
            oss << "    LEFT JOIN " << tables[rng() % nTables] << " t2 ON t1.id = t2.id\n";
            oss << "    WHERE t1.status = '" << statuses[rng() % nStatuses] << "'\n";
            oss << "    ORDER BY t1.id LIMIT " << (10 + rng() % 100) << ";\n\n";
        } else {
            // DELETE or transaction
            if (rng() % 2) {
                oss << "DELETE FROM " << table << " WHERE id = " << (rng() % 10000) << ";\n";
            } else {
                oss << "BEGIN;\n";
                oss << "UPDATE " << table << " SET " << cols[rng() % nCols] << " = " << (rng() % 1000) << " WHERE id = " << (rng() % 1000) << ";\n";
                oss << "COMMIT;\n\n";
            }
        }
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> xml(size_t n) {
    std::mt19937 rng(42);
    std::ostringstream oss;
    oss << "<?xml version=\"1.0\"?>\n<users>\n";
    int id = 1;
    while (static_cast<size_t>(oss.tellp()) + 150 < n) {
        oss << "  <user id=\"" << id++ << "\">\n"
            << "    <name>User" << id << "</name>\n"
            << "    <email>user" << id << "@example.com</email>\n"
            << "    <age>" << (20 + rng() % 50) << "</age>\n"
            << "  </user>\n";
    }
    oss << "</users>\n";
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> base64(size_t n) {
    std::mt19937 rng(42);
    const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::ostringstream oss;
    while (static_cast<size_t>(oss.tellp()) < n) {
        for (int i = 0; i < 76 && oss.str().size() < n; i++) {
            oss << b64[rng() % 64];
        }
        oss << "\n";
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> ini_config(size_t n) {
    std::mt19937 rng(42);
    std::ostringstream oss;
    const char* sections[] = {"database", "server", "cache", "logging", "security"};
    const char* keys[] = {"host", "port", "timeout", "enabled", "path", "level", "size"};
    int section = 0;
    while (static_cast<size_t>(oss.tellp()) < n) {
        oss << "[" << sections[section % 5] << "_" << section / 5 << "]\n";
        for (int i = 0; i < 5; i++) {
            oss << keys[rng() % 7] << " = " << (rng() % 1000) << "\n";
        }
        oss << "\n";
        section++;
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

// ============================================================================
// CODE TYPES
// ============================================================================

inline std::vector<uint8_t> javascript(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* nouns[] = {"user", "item", "data", "config", "state", "event", "node", "view",
                           "model", "list", "form", "page", "task", "file", "cache", "queue",
                           "token", "session", "order", "product", "cart", "payment", "address"};
    const char* verbs[] = {"get", "set", "create", "update", "delete", "fetch", "load", "save",
                           "parse", "format", "validate", "transform", "process", "handle", "render",
                           "submit", "cancel", "reset", "init", "sync", "filter", "sort", "merge"};
    const char* modules[] = {"react", "lodash", "axios", "moment", "uuid", "path", "fs", "crypto",
                             "express", "mongoose", "sequelize", "redis", "socket.io", "winston"};
    const int nNouns = 23, nVerbs = 23, nMods = 14;

    for (int i = 0; i < 5 + (int)(rng() % 8); i++) {
        int style = rng() % 4;
        const char* mod = modules[rng() % nMods];
        if (style == 0) oss << "import " << mod << " from '" << mod << "';\n";
        else if (style == 1) oss << "import { " << verbs[rng() % nVerbs] << " } from './" << nouns[rng() % nNouns] << "';\n";
        else if (style == 2) oss << "const " << mod << " = require('" << mod << "');\n";
        else oss << "import * as " << nouns[rng() % nNouns] << "Utils from './" << nouns[rng() % nNouns] << "-utils';\n";
    }
    oss << "\n";

    while (static_cast<size_t>(oss.tellp()) < n) {
        int pattern = rng() % 10;
        const char* n1 = nouns[rng() % nNouns];
        const char* n2 = nouns[rng() % nNouns];
        const char* v1 = verbs[rng() % nVerbs];
        int id = rng() % 10000;

        if (pattern < 3) {
            oss << "class " << (char)('A' + rng() % 26) << n1 << id << " {\n";
            oss << "  constructor(" << n2 << ") {\n";
            oss << "    this." << n2 << " = " << n2 << ";\n";
            oss << "    this.id = " << (rng() % 1000) << ";\n";
            oss << "  }\n\n";
            for (int m = 0; m < 1 + (int)(rng() % 3); m++) {
                const char* mv = verbs[rng() % nVerbs];
                oss << "  " << mv << "() {\n";
                if (rng() % 2) oss << "    if (!this." << n2 << ") return null;\n";
                oss << "    return this." << n2 << ";\n";
                oss << "  }\n";
            }
            oss << "}\n\n";
        } else if (pattern < 5) {
            oss << "async function " << v1 << (char)('A' + rng() % 26) << n1 << "(" << n2 << ") {\n";
            oss << "  try {\n";
            oss << "    const response = await fetch(`/api/" << n1 << "/${" << n2 << ".id}`);\n";
            oss << "    if (!response.ok) throw new Error(`HTTP ${response.status}`);\n";
            oss << "    return await response.json();\n";
            oss << "  } catch (err) {\n";
            oss << "    console.error('" << v1 << " failed:', err);\n";
            oss << "    throw err;\n";
            oss << "  }\n";
            oss << "}\n\n";
        } else if (pattern < 7) {
            oss << "const " << v1 << (char)('A' + rng() % 26) << n1 << " = ({ " << n2 << ", id = " << id << " }) => {\n";
            oss << "  const result = " << n2 << " ? " << n2 << ".length : 0;\n";
            oss << "  return { " << n2 << ", id, count: result, timestamp: Date.now() };\n";
            oss << "};\n\n";
        } else if (pattern < 8) {
            oss << "const " << n1 << "Service" << id << " = {\n";
            oss << "  " << n2 << ": [],\n";
            oss << "  " << v1 << "(" << n2 << ") {\n";
            oss << "    this." << n2 << ".push(" << n2 << ");\n";
            oss << "  },\n";
            oss << "  getAll() { return this." << n2 << "; }\n";
            oss << "};\n\n";
        } else {
            oss << "function " << v1 << (char)('A' + rng() % 26) << n1 << "(" << n2 << "List) {\n";
            oss << "  const results = [];\n";
            oss << "  for (let i = 0; i < " << n2 << "List.length; i++) {\n";
            oss << "    const item = " << n2 << "List[i];\n";
            oss << "    if (item.active) results.push(item.value * " << (rng() % 100) << ");\n";
            oss << "  }\n";
            oss << "  return results;\n";
            oss << "}\n\n";
        }
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> python(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* nouns[] = {"user", "item", "data", "config", "state", "record", "model", "view",
                           "task", "file", "cache", "queue", "token", "session", "order", "product",
                           "request", "response", "client", "server", "handler", "manager", "service"};
    const char* verbs[] = {"get", "set", "create", "update", "delete", "fetch", "load", "save",
                           "parse", "format", "validate", "transform", "process", "handle", "render",
                           "compute", "calculate", "analyze", "build", "generate", "filter", "sort"};
    const char* types[] = {"str", "int", "float", "bool", "list", "dict", "Any", "Optional[str]",
                           "List[int]", "Dict[str, Any]", "Tuple[int, int]", "Set[str]"};
    const char* decorators[] = {"@property", "@staticmethod", "@classmethod", "@abstractmethod",
                                "@functools.lru_cache", "@dataclass", "@pytest.fixture"};
    const int nNouns = 23, nVerbs = 22, nTypes = 12, nDecs = 7;

    oss << "#!/usr/bin/env python3\n";
    oss << "\"\"\"Module for " << nouns[rng() % nNouns] << " operations.\"\"\"\n\n";
    const char* mods[] = {"os", "sys", "json", "logging", "typing", "dataclasses", "functools",
                          "pathlib", "datetime", "collections", "itertools", "re", "asyncio"};
    for (int i = 0; i < 4 + (int)(rng() % 5); i++) {
        int style = rng() % 3;
        if (style == 0) oss << "import " << mods[rng() % 13] << "\n";
        else if (style == 1) oss << "from " << mods[rng() % 13] << " import " << nouns[rng() % nNouns] << "\n";
        else oss << "from typing import " << types[rng() % 6] << ", " << types[rng() % 6] << "\n";
    }
    oss << "\nlogger = logging.getLogger(__name__)\n\n";

    while (static_cast<size_t>(oss.tellp()) < n) {
        int pattern = rng() % 10;
        const char* n1 = nouns[rng() % nNouns];
        const char* n2 = nouns[rng() % nNouns];
        const char* v1 = verbs[rng() % nVerbs];
        const char* t1 = types[rng() % nTypes];
        int id = rng() % 10000;

        if (pattern < 3) {
            oss << "class " << (char)('A' + rng() % 26) << n1 << id << ":\n";
            oss << "    \"\"\"Handles " << n1 << " operations.\"\"\"\n\n";
            oss << "    def __init__(self, " << n2 << ": " << t1 << ") -> None:\n";
            oss << "        self._" << n2 << " = " << n2 << "\n";
            oss << "        self._id = " << (rng() % 1000) << "\n\n";
            for (int m = 0; m < 1 + (int)(rng() % 3); m++) {
                if (rng() % 3 == 0) oss << "    " << decorators[rng() % 4] << "\n";
                oss << "    def " << verbs[rng() % nVerbs] << "(self";
                if (rng() % 2) oss << ", value: " << types[rng() % nTypes];
                oss << "):\n";
                oss << "        return self._" << n2 << "\n\n";
            }
        } else if (pattern < 5) {
            oss << "async def " << v1 << "_" << n1 << "_" << id << "(" << n2 << ": " << t1 << "):\n";
            oss << "    \"\"\"Async " << v1 << " for " << n1 << ".\"\"\"\n";
            oss << "    try:\n";
            oss << "        async with aiohttp.ClientSession() as session:\n";
            oss << "            response = await session.get(f'/api/" << n1 << "/{" << n2 << "}')\n";
            oss << "            return await response.json()\n";
            oss << "    except Exception as e:\n";
            oss << "        logger.error(f'" << v1 << " failed: {e}')\n";
            oss << "        raise\n\n";
        } else if (pattern < 7) {
            oss << "def " << v1 << "_" << n1 << "s(" << n2 << "_list: List[" << t1 << "]) -> List[" << t1 << "]:\n";
            oss << "    return [\n";
            oss << "        " << n2 << " * " << (rng() % 10 + 1) << "\n";
            oss << "        for " << n2 << " in " << n2 << "_list\n";
            oss << "        if " << n2 << " is not None\n";
            oss << "    ]\n\n";
        } else if (pattern < 8) {
            oss << "def " << v1 << "_" << n1 << "_file(" << n2 << ": str) -> dict:\n";
            oss << "    with open(" << n2 << ", 'r') as f:\n";
            oss << "        " << n1 << " = json.load(f)\n";
            oss << "    return {'" << n1 << "': " << n1 << ", 'path': " << n2 << "}\n\n";
        } else {
            oss << "def " << v1 << "_all_" << n1 << "s(" << n2 << "s: list) -> list:\n";
            oss << "    results = []\n";
            oss << "    for idx, " << n2 << " in enumerate(" << n2 << "s):\n";
            oss << "        if " << n2 << ".get('active', False):\n";
            oss << "            results.append({'" << n2 << "': " << n2 << ", 'idx': idx})\n";
            oss << "    return results\n\n";
        }
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> html(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* tags[] = {"div", "section", "article", "main", "aside", "nav", "header", "footer"};
    const char* classes[] = {"container", "wrapper", "content", "card", "panel", "box", "row", "col",
                             "primary", "secondary", "featured", "sidebar", "main-content", "header-bar"};
    const char* words[] = {"Lorem", "ipsum", "dolor", "sit", "amet", "consectetur", "adipiscing", "elit",
                           "sed", "do", "eiusmod", "tempor", "incididunt", "ut", "labore", "et"};
    const int nTags = 8, nClasses = 14, nWords = 16;

    oss << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
    oss << "  <meta charset=\"UTF-8\">\n  <title>Application</title>\n";
    oss << "  <link rel=\"stylesheet\" href=\"/css/main.css\">\n</head>\n<body>\n";

    while (static_cast<size_t>(oss.tellp()) + 200 < n) {
        int pattern = rng() % 6;
        const char* tag = tags[rng() % nTags];
        int id = rng() % 10000;

        if (pattern < 2) {
            oss << "  <" << tag << " id=\"" << tags[rng() % nTags] << "-" << id << "\" class=\"" << classes[rng() % nClasses] << "\">\n";
            oss << "    <h" << (1 + rng() % 4) << " class=\"title\">";
            for (int w = 0; w < 3 + (int)(rng() % 4); w++) oss << words[rng() % nWords] << " ";
            oss << "</h" << (1 + rng() % 4) << ">\n";
            oss << "    <p class=\"" << classes[rng() % nClasses] << "\">";
            for (int w = 0; w < 10 + (int)(rng() % 20); w++) oss << words[rng() % nWords] << " ";
            oss << "</p>\n";
            oss << "    <a href=\"/" << tags[rng() % nTags] << "/" << id << "\" class=\"btn\">";
            oss << words[rng() % nWords] << " " << words[rng() % nWords] << "</a>\n";
            oss << "  </" << tag << ">\n\n";
        } else if (pattern < 4) {
            oss << "  <ul class=\"" << classes[rng() % nClasses] << "\" id=\"list-" << id << "\">\n";
            for (int li = 0; li < 3 + (int)(rng() % 5); li++) {
                oss << "    <li class=\"item-" << li << "\">";
                for (int w = 0; w < 3 + (int)(rng() % 6); w++) oss << words[rng() % nWords] << " ";
                oss << "</li>\n";
            }
            oss << "  </ul>\n\n";
        } else {
            oss << "  <form action=\"/api/" << tags[rng() % nTags] << "\" method=\"post\" class=\"form-" << id << "\">\n";
            oss << "    <label for=\"input-" << id << "\">" << words[rng() % nWords] << "</label>\n";
            oss << "    <input type=\"text\" id=\"input-" << id << "\" name=\"" << words[rng() % nWords] << "\" required>\n";
            oss << "    <button type=\"submit\" class=\"btn " << classes[rng() % nClasses] << "\">" << words[rng() % nWords] << "</button>\n";
            oss << "  </form>\n\n";
        }
    }
    oss << "</body>\n</html>\n";
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> typescript(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* nouns[] = {"User", "Item", "Data", "Config", "State", "Event", "Node", "View",
                           "Model", "List", "Form", "Page", "Task", "File", "Cache", "Queue"};
    const char* props[] = {"id", "name", "value", "label", "title", "count", "active", "enabled",
                           "visible", "selected", "loading", "error", "status", "type", "mode"};
    const char* types[] = {"string", "number", "boolean", "Date", "unknown", "never", "void"};
    const int nNouns = 16, nProps = 15, nTypes = 7;

    oss << "import React, { useState, useEffect, useCallback } from 'react';\n";
    oss << "import { " << nouns[rng() % nNouns] << "Service } from './services';\n\n";

    while (static_cast<size_t>(oss.tellp()) < n) {
        int pattern = rng() % 6;
        const char* noun = nouns[rng() % nNouns];
        int id = rng() % 10000;

        if (pattern < 2) {
            oss << "interface " << noun << "Props" << id << " {\n";
            for (int p = 0; p < 2 + (int)(rng() % 4); p++) {
                const char* prop = props[rng() % nProps];
                const char* type = types[rng() % nTypes];
                oss << "  " << prop << (rng() % 3 == 0 ? "?" : "") << ": " << type << ";\n";
            }
            oss << "}\n\n";
        } else if (pattern < 4) {
            oss << "const " << noun << "Component" << id << ": React.FC<" << noun << "Props" << (rng() % 1000) << "> = (props) => {\n";
            oss << "  const [" << props[rng() % nProps] << ", set" << noun << "] = useState<" << types[rng() % nTypes] << ">();\n";
            oss << "  const [loading, setLoading] = useState(false);\n\n";
            oss << "  useEffect(() => {\n";
            oss << "    setLoading(true);\n";
            oss << "    // Load " << noun << " data\n";
            oss << "  }, [props." << props[rng() % nProps] << "]);\n\n";
            oss << "  return (\n";
            oss << "    <div className=\"" << noun << "-" << id << "\">\n";
            oss << "      {loading ? <span>Loading...</span> : <span>{props." << props[rng() % nProps] << "}</span>}\n";
            oss << "    </div>\n";
            oss << "  );\n";
            oss << "};\n\n";
        } else {
            oss << "type " << noun << "Result" << id << "<T> = {\n";
            oss << "  data: T;\n";
            oss << "  " << props[rng() % nProps] << ": " << types[rng() % nTypes] << ";\n";
            oss << "  meta: Record<string, unknown>;\n";
            oss << "};\n\n";
        }
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> golang(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* nouns[] = {"user", "item", "data", "config", "state", "event", "node", "cache",
                           "client", "server", "handler", "service", "worker", "task", "queue"};
    const char* verbs[] = {"Get", "Set", "Create", "Update", "Delete", "Fetch", "Load", "Save",
                           "Parse", "Format", "Validate", "Process", "Handle", "Build", "Run"};
    const char* types[] = {"string", "int", "int64", "bool", "error", "[]byte", "map[string]interface{}",
                           "context.Context", "*http.Request", "*http.Response", "io.Reader"};
    const int nNouns = 15, nVerbs = 15, nTypes = 11;

    oss << "package main\n\nimport (\n\t\"context\"\n\t\"encoding/json\"\n\t\"fmt\"\n\t\"io\"\n\t\"net/http\"\n\t\"sync\"\n)\n\n";

    while (static_cast<size_t>(oss.tellp()) < n) {
        int pattern = rng() % 8;
        const char* n1 = nouns[rng() % nNouns];
        const char* v1 = verbs[rng() % nVerbs];
        int id = rng() % 10000;

        if (pattern < 2) {
            oss << "type " << (char)('A' + rng() % 26) << n1 << id << " struct {\n";
            oss << "\tID        int    `json:\"id\"`\n";
            oss << "\tName      string `json:\"name\"`\n";
            oss << "\tCreatedAt int64  `json:\"created_at\"`\n";
            oss << "\tmu        sync.RWMutex\n";
            oss << "}\n\n";
        } else if (pattern < 4) {
            oss << "func " << v1 << (char)('A' + rng() % 26) << n1 << "(ctx context.Context, " << n1 << " " << types[rng() % nTypes] << ") (" << types[rng() % 5] << ", error) {\n";
            oss << "\tif ctx.Err() != nil {\n";
            oss << "\t\treturn " << (rng() % 2 ? "nil" : "\"\"") << ", ctx.Err()\n";
            oss << "\t}\n";
            oss << "\tresult := fmt.Sprintf(\"%v\", " << n1 << ")\n";
            oss << "\treturn result, nil\n";
            oss << "}\n\n";
        } else if (pattern < 6) {
            oss << "func (s *" << (char)('A' + rng() % 26) << n1 << id << ") " << v1 << "() error {\n";
            oss << "\ts.mu.Lock()\n";
            oss << "\tdefer s.mu.Unlock()\n";
            oss << "\tif s.ID == 0 {\n";
            oss << "\t\treturn fmt.Errorf(\"invalid " << n1 << " id\")\n";
            oss << "\t}\n";
            oss << "\treturn nil\n";
            oss << "}\n\n";
        } else {
            oss << "func " << v1 << "Handler" << id << "(w http.ResponseWriter, r *http.Request) {\n";
            oss << "\tvar " << n1 << " map[string]interface{}\n";
            oss << "\tif err := json.NewDecoder(r.Body).Decode(&" << n1 << "); err != nil {\n";
            oss << "\t\thttp.Error(w, err.Error(), http.StatusBadRequest)\n";
            oss << "\t\treturn\n";
            oss << "\t}\n";
            oss << "\tjson.NewEncoder(w).Encode(" << n1 << ")\n";
            oss << "}\n\n";
        }
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> rust(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* nouns[] = {"user", "item", "data", "config", "state", "event", "node", "cache",
                           "client", "handler", "service", "worker", "task", "buffer", "stream"};
    const char* verbs[] = {"get", "set", "create", "update", "delete", "fetch", "load", "save",
                           "parse", "format", "validate", "process", "handle", "build", "run"};
    const char* types[] = {"String", "i32", "i64", "u64", "bool", "Vec<u8>", "Option<String>",
                           "Result<(), Error>", "&str", "&[u8]", "Box<dyn Error>"};
    const int nNouns = 15, nVerbs = 15, nTypes = 11;

    oss << "use std::collections::HashMap;\nuse std::sync::{Arc, Mutex};\nuse std::error::Error;\n\n";

    while (static_cast<size_t>(oss.tellp()) < n) {
        int pattern = rng() % 8;
        const char* n1 = nouns[rng() % nNouns];
        const char* v1 = verbs[rng() % nVerbs];
        int id = rng() % 10000;

        if (pattern < 2) {
            oss << "#[derive(Debug, Clone, Serialize, Deserialize)]\n";
            oss << "pub struct " << (char)('A' + rng() % 26) << n1 << id << " {\n";
            oss << "    pub id: u64,\n";
            oss << "    pub name: String,\n";
            oss << "    pub " << nouns[rng() % nNouns] << ": Option<" << types[rng() % 5] << ">,\n";
            oss << "}\n\n";
        } else if (pattern < 4) {
            oss << "impl " << (char)('A' + rng() % 26) << n1 << id << " {\n";
            oss << "    pub fn new(name: &str) -> Self {\n";
            oss << "        Self { id: 0, name: name.to_string(), " << nouns[rng() % nNouns] << ": None }\n";
            oss << "    }\n\n";
            oss << "    pub fn " << v1 << "(&self) -> Result<(), Box<dyn Error>> {\n";
            oss << "        if self.id == 0 {\n";
            oss << "            return Err(\"invalid id\".into());\n";
            oss << "        }\n";
            oss << "        Ok(())\n";
            oss << "    }\n";
            oss << "}\n\n";
        } else if (pattern < 6) {
            oss << "pub fn " << v1 << "_" << n1 << "_" << id << "(" << n1 << ": &" << types[rng() % 5] << ") -> Result<" << types[rng() % 5] << ", Box<dyn Error>> {\n";
            oss << "    let result = " << n1 << ".clone();\n";
            oss << "    if result.is_empty() {\n";
            oss << "        return Err(\"empty " << n1 << "\".into());\n";
            oss << "    }\n";
            oss << "    Ok(result)\n";
            oss << "}\n\n";
        } else {
            oss << "pub async fn " << v1 << "_async_" << id << "(" << n1 << ": Arc<Mutex<Vec<u8>>>) -> Result<(), Box<dyn Error>> {\n";
            oss << "    let mut guard = " << n1 << ".lock().unwrap();\n";
            oss << "    guard.push(" << (rng() % 256) << ");\n";
            oss << "    Ok(())\n";
            oss << "}\n\n";
        }
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> c_code(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* nouns[] = {"buffer", "data", "config", "state", "node", "list", "cache",
                           "client", "handler", "worker", "task", "queue", "ctx", "ptr"};
    const char* verbs[] = {"init", "free", "create", "destroy", "get", "set", "alloc",
                           "parse", "process", "handle", "read", "write", "open", "close"};
    const int nNouns = 14, nVerbs = 14;

    oss << "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdint.h>\n\n";

    while (static_cast<size_t>(oss.tellp()) < n) {
        int pattern = rng() % 8;
        const char* n1 = nouns[rng() % nNouns];
        const char* v1 = verbs[rng() % nVerbs];
        int id = rng() % 10000;

        if (pattern < 2) {
            oss << "typedef struct " << n1 << "_" << id << " {\n";
            oss << "    uint32_t id;\n";
            oss << "    size_t size;\n";
            oss << "    uint8_t *" << nouns[rng() % nNouns] << ";\n";
            oss << "    struct " << n1 << "_" << id << " *next;\n";
            oss << "} " << n1 << "_" << id << "_t;\n\n";
        } else if (pattern < 4) {
            oss << "int " << v1 << "_" << n1 << "_" << id << "(" << n1 << "_t *" << n1 << ", size_t len) {\n";
            oss << "    if (" << n1 << " == NULL || len == 0) {\n";
            oss << "        return -1;\n";
            oss << "    }\n";
            oss << "    " << n1 << "->size = len;\n";
            oss << "    " << n1 << "->" << nouns[rng() % nNouns] << " = malloc(len);\n";
            oss << "    if (" << n1 << "->" << nouns[rng() % nNouns] << " == NULL) {\n";
            oss << "        return -1;\n";
            oss << "    }\n";
            oss << "    memset(" << n1 << "->" << nouns[rng() % nNouns] << ", 0, len);\n";
            oss << "    return 0;\n";
            oss << "}\n\n";
        } else if (pattern < 6) {
            oss << "void " << v1 << "_" << n1 << "_" << id << "(" << n1 << "_t **" << n1 << ") {\n";
            oss << "    if (" << n1 << " == NULL || *" << n1 << " == NULL) return;\n";
            oss << "    free((*" << n1 << ")->" << nouns[rng() % nNouns] << ");\n";
            oss << "    free(*" << n1 << ");\n";
            oss << "    *" << n1 << " = NULL;\n";
            oss << "}\n\n";
        } else {
            oss << "#define " << (char)('A' + rng() % 26) << "_" << n1 << "_SIZE " << (64 + rng() % 4096) << "\n";
            oss << "#define " << (char)('A' + rng() % 26) << "_MAX_" << n1 << " " << (1 + rng() % 1024) << "\n\n";
        }
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> java(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* nouns[] = {"User", "Item", "Data", "Config", "State", "Event", "Node", "Cache",
                           "Client", "Handler", "Service", "Worker", "Task", "Queue", "Manager"};
    const char* verbs[] = {"get", "set", "create", "update", "delete", "fetch", "load", "save",
                           "parse", "format", "validate", "process", "handle", "build", "run"};
    const int nNouns = 15, nVerbs = 15;

    oss << "package com.example.service;\n\nimport java.util.*;\nimport java.util.concurrent.*;\nimport org.springframework.stereotype.Service;\n\n";

    while (static_cast<size_t>(oss.tellp()) < n) {
        int pattern = rng() % 8;
        const char* n1 = nouns[rng() % nNouns];
        const char* v1 = verbs[rng() % nVerbs];
        int id = rng() % 10000;

        if (pattern < 3) {
            oss << "@Service\n";
            oss << "public class " << n1 << "Service" << id << " {\n\n";
            oss << "    private final Map<Long, " << n1 << "> " << (char)('a' + rng() % 26) << n1 << "Map = new ConcurrentHashMap<>();\n\n";
            oss << "    public " << n1 << " " << v1 << "(Long id) {\n";
            oss << "        return " << (char)('a' + rng() % 26) << n1 << "Map.get(id);\n";
            oss << "    }\n\n";
            oss << "    public void save(" << n1 << " " << (char)('a' + rng() % 26) << n1 << ") {\n";
            oss << "        " << (char)('a' + rng() % 26) << n1 << "Map.put(" << (char)('a' + rng() % 26) << n1 << ".getId(), " << (char)('a' + rng() % 26) << n1 << ");\n";
            oss << "    }\n";
            oss << "}\n\n";
        } else if (pattern < 5) {
            oss << "public class " << n1 << "DTO" << id << " {\n";
            oss << "    private Long id;\n";
            oss << "    private String name;\n";
            oss << "    private LocalDateTime createdAt;\n\n";
            oss << "    public Long getId() { return id; }\n";
            oss << "    public void setId(Long id) { this.id = id; }\n";
            oss << "    public String getName() { return name; }\n";
            oss << "    public void setName(String name) { this.name = name; }\n";
            oss << "}\n\n";
        } else {
            oss << "public interface " << n1 << "Repository" << id << " extends JpaRepository<" << n1 << ", Long> {\n";
            oss << "    List<" << n1 << "> findBy" << nouns[rng() % nNouns] << "(String " << nouns[rng() % nNouns] << ");\n";
            oss << "    Optional<" << n1 << "> findById(Long id);\n";
            oss << "}\n\n";
        }
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> css(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* selectors[] = {".container", ".wrapper", ".card", ".button", ".nav", ".header", ".footer",
                                ".sidebar", ".modal", ".form", ".input", ".list", ".item", ".panel", ".grid"};
    const char* props[] = {"display", "flex-direction", "justify-content", "align-items", "padding", "margin",
                           "background-color", "color", "border-radius", "box-shadow", "font-size", "font-weight",
                           "width", "height", "position", "top", "left", "z-index", "opacity", "transition"};
    const char* colors[] = {"#333", "#fff", "#f0f0f0", "#007bff", "#28a745", "#dc3545", "#ffc107", "#17a2b8",
                            "rgba(0,0,0,0.1)", "transparent", "inherit", "currentColor"};
    const int nSel = 15, nProps = 20, nColors = 12;

    oss << "/* Main stylesheet */\n:root {\n  --primary: #007bff;\n  --secondary: #6c757d;\n  --spacing: 1rem;\n}\n\n";
    oss << "*, *::before, *::after {\n  box-sizing: border-box;\n}\n\n";

    while (static_cast<size_t>(oss.tellp()) < n) {
        int pattern = rng() % 6;
        const char* sel = selectors[rng() % nSel];
        int id = rng() % 1000;

        if (pattern < 2) {
            oss << sel << "-" << id << " {\n";
            oss << "  display: " << (rng() % 3 == 0 ? "flex" : (rng() % 2 ? "block" : "grid")) << ";\n";
            oss << "  " << props[rng() % nProps] << ": " << (rng() % 100) << (rng() % 2 ? "px" : "rem") << ";\n";
            oss << "  background-color: " << colors[rng() % nColors] << ";\n";
            oss << "  border-radius: " << (rng() % 16) << "px;\n";
            oss << "}\n\n";
        } else if (pattern < 4) {
            oss << sel << "-" << id << ":hover {\n";
            oss << "  opacity: 0." << (7 + rng() % 3) << ";\n";
            oss << "  transform: translateY(-" << (rng() % 4) << "px);\n";
            oss << "  transition: all 0." << (2 + rng() % 3) << "s ease;\n";
            oss << "}\n\n";
        } else {
            oss << "@media (max-width: " << (576 + rng() % 3 * 192) << "px) {\n";
            oss << "  " << sel << "-" << id << " {\n";
            oss << "    flex-direction: column;\n";
            oss << "    padding: " << (rng() % 20) << "px;\n";
            oss << "  }\n";
            oss << "}\n\n";
        }
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> bash_script(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* vars[] = {"FILE", "DIR", "OUTPUT", "INPUT", "CONFIG", "LOG", "TMP", "RESULT", "COUNT", "STATUS"};
    const char* cmds[] = {"echo", "cat", "grep", "awk", "sed", "cut", "sort", "uniq", "wc", "head", "tail", "find"};
    const int nVars = 10, nCmds = 12;

    oss << "#!/bin/bash\nset -euo pipefail\n\n# Configuration\nSCRIPT_DIR=\"$(cd \"$(dirname \"${BASH_SOURCE[0]}\")\" && pwd)\"\nLOG_FILE=\"${SCRIPT_DIR}/output.log\"\n\n";

    while (static_cast<size_t>(oss.tellp()) < n) {
        int pattern = rng() % 8;
        const char* v1 = vars[rng() % nVars];
        int id = rng() % 1000;

        if (pattern < 2) {
            oss << v1 << "_" << id << "=\"/path/to/" << vars[rng() % nVars] << "\"\n";
            oss << "if [[ ! -f \"$" << v1 << "_" << id << "\" ]]; then\n";
            oss << "    echo \"Error: File not found: $" << v1 << "_" << id << "\" >&2\n";
            oss << "    exit 1\n";
            oss << "fi\n\n";
        } else if (pattern < 4) {
            oss << "function " << cmds[rng() % nCmds] << "_" << vars[rng() % nVars] << "_" << id << "() {\n";
            oss << "    local input=\"$1\"\n";
            oss << "    local output=\"$2\"\n";
            oss << "    " << cmds[rng() % nCmds] << " \"$input\" | " << cmds[rng() % nCmds] << " -c > \"$output\"\n";
            oss << "    return $?\n";
            oss << "}\n\n";
        } else if (pattern < 6) {
            oss << "for file in \"$" << v1 << "_" << id << "\"/*.txt; do\n";
            oss << "    [[ -f \"$file\" ]] || continue\n";
            oss << "    echo \"Processing: $file\"\n";
            oss << "    " << cmds[rng() % nCmds] << " \"$file\" >> \"$LOG_FILE\"\n";
            oss << "done\n\n";
        } else {
            oss << "# Check dependencies\n";
            oss << "command -v " << cmds[rng() % nCmds] << " >/dev/null 2>&1 || { echo \"" << cmds[rng() % nCmds] << " required\"; exit 1; }\n\n";
        }
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> php(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* nouns[] = {"User", "Item", "Data", "Config", "State", "Event", "Cache",
                           "Client", "Handler", "Service", "Controller", "Model", "Request"};
    const char* verbs[] = {"get", "set", "create", "update", "delete", "find", "load", "save",
                           "validate", "process", "handle", "render", "index", "show", "store"};
    const int nNouns = 13, nVerbs = 15;

    oss << "<?php\n\nnamespace App\\Services;\n\nuse Illuminate\\Support\\Facades\\DB;\nuse Illuminate\\Support\\Facades\\Cache;\n\n";

    while (static_cast<size_t>(oss.tellp()) < n) {
        int pattern = rng() % 8;
        const char* n1 = nouns[rng() % nNouns];
        const char* v1 = verbs[rng() % nVerbs];
        int id = rng() % 10000;

        if (pattern < 3) {
            oss << "class " << n1 << "Service" << id << "\n{\n";
            oss << "    private $" << (char)('a' + rng() % 26) << n1 << "Repository;\n\n";
            oss << "    public function __construct(" << n1 << "Repository $repository)\n";
            oss << "    {\n";
            oss << "        $this->" << (char)('a' + rng() % 26) << n1 << "Repository = $repository;\n";
            oss << "    }\n\n";
            oss << "    public function " << v1 << "(int $id): ?" << n1 << "\n";
            oss << "    {\n";
            oss << "        return Cache::remember('" << n1 << "_' . $id, 3600, function() use ($id) {\n";
            oss << "            return $this->" << (char)('a' + rng() % 26) << n1 << "Repository->find($id);\n";
            oss << "        });\n";
            oss << "    }\n";
            oss << "}\n\n";
        } else if (pattern < 5) {
            oss << "class " << n1 << "Controller" << id << " extends Controller\n{\n";
            oss << "    public function " << v1 << "(Request $request)\n";
            oss << "    {\n";
            oss << "        $validated = $request->validate([\n";
            oss << "            'name' => 'required|string|max:255',\n";
            oss << "            'email' => 'required|email|unique:" << (char)('a' + rng() % 26) << n1 << "s',\n";
            oss << "        ]);\n\n";
            oss << "        return response()->json($validated);\n";
            oss << "    }\n";
            oss << "}\n\n";
        } else {
            oss << "trait " << n1 << "Trait" << id << "\n{\n";
            oss << "    public function " << v1 << n1 << "(): array\n";
            oss << "    {\n";
            oss << "        return DB::table('" << (char)('a' + rng() % 26) << n1 << "s')->where('active', true)->get()->toArray();\n";
            oss << "    }\n";
            oss << "}\n\n";
        }
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

// ============================================================================
// LOG TYPES
// ============================================================================

inline std::vector<uint8_t> nginx_log(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* methods[] = {"GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS"};
    const char* paths[] = {"/", "/index.html", "/api/v1/users", "/api/v1/products", "/api/v1/orders",
                           "/static/css/main.css", "/static/js/app.js", "/images/logo.png",
                           "/favicon.ico", "/robots.txt", "/sitemap.xml", "/health", "/metrics"};
    const char* refs[] = {"-", "https://google.com/", "https://example.com/", "https://bing.com/search?q=test"};
    const char* agents[] = {
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/120.0.0.0 Safari/537.36",
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 Safari/605.1.15",
        "Mozilla/5.0 (X11; Linux x86_64; rv:121.0) Gecko/20100101 Firefox/121.0",
        "curl/8.4.0", "python-requests/2.31.0", "Go-http-client/2.0"
    };
    int codes[] = {200, 200, 200, 200, 201, 204, 301, 302, 304, 400, 401, 403, 404, 500, 502, 503};

    int day = 1, hour = 0, min = 0, sec = 0;
    while (static_cast<size_t>(oss.tellp()) < n) {
        sec += 1 + rng() % 3;
        if (sec >= 60) { sec = 0; min++; }
        if (min >= 60) { min = 0; hour++; }
        if (hour >= 24) { hour = 0; day++; }

        int code = codes[rng() % 16];
        int bytes = (code >= 200 && code < 300) ? 500 + rng() % 50000 : 100 + rng() % 500;
        double time = 0.001 + (rng() % 5000) / 1000.0;

        char buf[512];
        snprintf(buf, sizeof(buf),
            "%d.%d.%d.%d - - [%02d/Dec/2024:%02d:%02d:%02d +0000] \"%s %s HTTP/1.1\" %d %d \"%s\" \"%s\" %.3f\n",
            (int)(rng() % 256), (int)(rng() % 256), (int)(rng() % 256), (int)(rng() % 256),
            day, hour, min, sec,
            methods[rng() % 6], paths[rng() % 13], code, bytes,
            refs[rng() % 4], agents[rng() % 6], time);
        oss << buf;
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> json_log(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* levels[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    const char* services[] = {"api-gateway", "user-service", "order-service", "payment-service", "notification-service"};
    const char* msgs[] = {
        "Request processed successfully",
        "Database query completed",
        "Cache hit for key",
        "External API call completed",
        "Connection pool exhausted",
        "Rate limit exceeded",
        "Invalid request payload",
        "Authentication failed",
        "Service unavailable"
    };

    uint64_t ts = 1701388800000ULL; // Dec 1 2024 00:00:00 UTC
    int id = 1000000;

    while (static_cast<size_t>(oss.tellp()) < n) {
        ts += 10 + rng() % 500;
        int level_idx = rng() % 100;
        const char* level = levels[level_idx < 60 ? 1 : (level_idx < 85 ? 0 : (level_idx < 95 ? 2 : (level_idx < 99 ? 3 : 4)))];

        char buf[512];
        snprintf(buf, sizeof(buf),
            "{\"timestamp\":%llu,\"level\":\"%s\",\"service\":\"%s\",\"traceId\":\"%08x%08x\","
            "\"spanId\":\"%08x\",\"message\":\"%s\",\"requestId\":\"%d\",\"duration_ms\":%d}\n",
            (unsigned long long)ts, level, services[rng() % 5],
            (unsigned)(rng()), (unsigned)(rng()), (unsigned)(rng() & 0xFFFFFF),
            msgs[rng() % 9], id++, (int)(rng() % 2000));
        oss << buf;
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

// ============================================================================
// CONFIG TYPES
// ============================================================================

inline std::vector<uint8_t> docker_compose(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* services[] = {"web", "api", "db", "cache", "worker", "nginx", "rabbitmq", "elasticsearch"};
    const char* images[] = {"node:20-alpine", "python:3.12-slim", "postgres:16", "redis:7-alpine",
                            "nginx:alpine", "rabbitmq:3-management", "elasticsearch:8.11.0"};

    oss << "version: '3.8'\n\nservices:\n";

    int svc = 0;
    while (static_cast<size_t>(oss.tellp()) < n) {
        const char* name = services[svc % 8];
        oss << "  " << name << "-" << svc << ":\n";
        oss << "    image: " << images[rng() % 7] << "\n";
        oss << "    container_name: " << name << "-" << svc << "-container\n";
        oss << "    restart: unless-stopped\n";
        oss << "    environment:\n";
        oss << "      - NODE_ENV=production\n";
        oss << "      - DATABASE_URL=postgres://user:pass@db:5432/app\n";
        oss << "      - REDIS_URL=redis://cache:6379\n";
        oss << "      - LOG_LEVEL=" << (rng() % 2 ? "info" : "debug") << "\n";
        oss << "    ports:\n";
        oss << "      - \"" << (3000 + svc) << ":" << (3000 + rng() % 100) << "\"\n";
        oss << "    volumes:\n";
        oss << "      - ./data/" << name << ":/app/data\n";
        oss << "      - ./logs:/app/logs\n";
        oss << "    depends_on:\n";
        oss << "      - db-0\n";
        oss << "      - cache-0\n";
        oss << "    networks:\n";
        oss << "      - backend\n";
        oss << "    healthcheck:\n";
        oss << "      test: [\"CMD\", \"curl\", \"-f\", \"http://localhost:" << (3000 + rng() % 100) << "/health\"]\n";
        oss << "      interval: 30s\n";
        oss << "      timeout: 10s\n";
        oss << "      retries: 3\n\n";
        svc++;
    }

    oss << "networks:\n  backend:\n    driver: bridge\n\nvolumes:\n  data:\n  logs:\n";
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> terraform(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* regions[] = {"us-east-1", "us-west-2", "eu-west-1", "ap-southeast-1"};
    const char* types[] = {"t3.micro", "t3.small", "t3.medium", "m5.large", "r5.xlarge"};

    oss << "terraform {\n  required_providers {\n    aws = {\n      source  = \"hashicorp/aws\"\n      version = \"~> 5.0\"\n    }\n  }\n}\n\n";
    oss << "provider \"aws\" {\n  region = \"" << regions[0] << "\"\n}\n\n";

    int id = 0;
    while (static_cast<size_t>(oss.tellp()) < n) {
        int pattern = rng() % 5;

        if (pattern < 2) {
            oss << "resource \"aws_instance\" \"server_" << id << "\" {\n";
            oss << "  ami           = \"ami-0" << std::hex << (rng() % 0xFFFFFF) << std::dec << "\"\n";
            oss << "  instance_type = \"" << types[rng() % 5] << "\"\n";
            oss << "  \n";
            oss << "  tags = {\n";
            oss << "    Name        = \"server-" << id << "\"\n";
            oss << "    Environment = \"" << (rng() % 2 ? "production" : "staging") << "\"\n";
            oss << "    Project     = \"app-" << (rng() % 10) << "\"\n";
            oss << "  }\n";
            oss << "  \n";
            oss << "  vpc_security_group_ids = [aws_security_group.main.id]\n";
            oss << "  subnet_id              = aws_subnet.main.id\n";
            oss << "}\n\n";
        } else if (pattern < 3) {
            oss << "resource \"aws_s3_bucket\" \"bucket_" << id << "\" {\n";
            oss << "  bucket = \"my-app-bucket-" << id << "-" << (rng() % 1000000) << "\"\n";
            oss << "  \n";
            oss << "  tags = {\n";
            oss << "    Name = \"bucket-" << id << "\"\n";
            oss << "  }\n";
            oss << "}\n\n";
            oss << "resource \"aws_s3_bucket_versioning\" \"bucket_" << id << "_versioning\" {\n";
            oss << "  bucket = aws_s3_bucket.bucket_" << id << ".id\n";
            oss << "  versioning_configuration {\n";
            oss << "    status = \"Enabled\"\n";
            oss << "  }\n";
            oss << "}\n\n";
        } else {
            oss << "variable \"instance_count_" << id << "\" {\n";
            oss << "  description = \"Number of instances to create\"\n";
            oss << "  type        = number\n";
            oss << "  default     = " << (1 + rng() % 5) << "\n";
            oss << "}\n\n";
            oss << "output \"instance_" << id << "_ip\" {\n";
            oss << "  value       = aws_instance.server_" << id << ".public_ip\n";
            oss << "  description = \"Public IP of server " << id << "\"\n";
            oss << "}\n\n";
        }
        id++;
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> env_file(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    // Common .env patterns
    oss << "# Application Configuration\n";
    oss << "NODE_ENV=production\n";
    oss << "APP_NAME=myapp\n";
    oss << "APP_PORT=3000\n";
    oss << "APP_HOST=0.0.0.0\n\n";

    const char* prefixes[] = {"DATABASE", "REDIS", "RABBITMQ", "ELASTICSEARCH", "AWS", "SMTP", "AUTH", "API"};
    const char* suffixes[] = {"_HOST", "_PORT", "_USER", "_PASSWORD", "_NAME", "_URL", "_KEY", "_SECRET", "_TIMEOUT", "_ENABLED"};

    int section = 0;
    while (static_cast<size_t>(oss.tellp()) < n) {
        const char* prefix = prefixes[section % 8];
        oss << "# " << prefix << " Configuration\n";

        for (int i = 0; i < 5 + (int)(rng() % 5); i++) {
            const char* suffix = suffixes[rng() % 10];
            oss << prefix << suffix << "=";

            if (strstr(suffix, "PORT")) {
                oss << (1000 + rng() % 9000);
            } else if (strstr(suffix, "HOST")) {
                oss << (rng() % 2 ? "localhost" : "10.0.0.") << (1 + rng() % 254);
            } else if (strstr(suffix, "PASSWORD") || strstr(suffix, "SECRET") || strstr(suffix, "KEY")) {
                for (int j = 0; j < 32; j++) {
                    char c = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"[rng() % 62];
                    oss << c;
                }
            } else if (strstr(suffix, "ENABLED")) {
                oss << (rng() % 2 ? "true" : "false");
            } else if (strstr(suffix, "TIMEOUT")) {
                oss << (1000 + rng() % 30000);
            } else if (strstr(suffix, "URL")) {
                oss << "https://" << prefix << ".example.com/api/v1";
            } else {
                oss << "value_" << (rng() % 10000);
            }
            oss << "\n";
        }
        oss << "\n";
        section++;
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> toml_config(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* sections[] = {"server", "database", "cache", "logging", "security", "api", "metrics", "features"};
    const char* keys[] = {"host", "port", "timeout", "enabled", "level", "max_connections", "retry_count", "buffer_size"};

    oss << "# Application Configuration\n\n";
    oss << "[package]\n";
    oss << "name = \"myapp\"\n";
    oss << "version = \"1.0.0\"\n";
    oss << "authors = [\"Developer <dev@example.com>\"]\n\n";

    int section = 0;
    while (static_cast<size_t>(oss.tellp()) < n) {
        oss << "[" << sections[section % 8] << "]\n";

        for (int i = 0; i < 4 + (int)(rng() % 4); i++) {
            const char* key = keys[rng() % 8];
            oss << key << " = ";

            if (strstr(key, "port") || strstr(key, "timeout") || strstr(key, "count") || strstr(key, "size") || strstr(key, "connections")) {
                oss << (rng() % 65536);
            } else if (strstr(key, "enabled")) {
                oss << (rng() % 2 ? "true" : "false");
            } else if (strstr(key, "level")) {
                const char* levels[] = {"debug", "info", "warn", "error"};
                oss << "\"" << levels[rng() % 4] << "\"";
            } else {
                oss << "\"" << sections[rng() % 8] << "_value_" << (rng() % 1000) << "\"";
            }
            oss << "\n";
        }

        // Add array or inline table sometimes
        if (rng() % 3 == 0) {
            oss << "tags = [\"" << sections[rng() % 8] << "\", \"" << sections[rng() % 8] << "\"]\n";
        }
        oss << "\n";

        // Nested table
        if (rng() % 4 == 0) {
            oss << "[" << sections[section % 8] << ".advanced]\n";
            oss << "custom_option = " << (rng() % 1000) << "\n";
            oss << "experimental = " << (rng() % 2 ? "true" : "false") << "\n\n";
        }
        section++;
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

// ============================================================================
// BUILD TYPES
// ============================================================================

inline std::vector<uint8_t> package_json(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* deps[] = {"react", "vue", "angular", "express", "fastify", "lodash", "axios", "moment",
                          "dayjs", "uuid", "dotenv", "cors", "helmet", "winston", "pino", "jest",
                          "typescript", "eslint", "prettier", "webpack", "vite", "esbuild", "rollup"};
    const char* scripts[] = {"dev", "build", "start", "test", "lint", "format", "typecheck", "deploy"};

    int pkg = 0;
    while (static_cast<size_t>(oss.tellp()) < n) {
        oss << "{\n";
        oss << "  \"name\": \"@myorg/package-" << pkg << "\",\n";
        oss << "  \"version\": \"" << (rng() % 10) << "." << (rng() % 20) << "." << (rng() % 100) << "\",\n";
        oss << "  \"description\": \"Package " << pkg << " for application\",\n";
        oss << "  \"main\": \"dist/index.js\",\n";
        oss << "  \"types\": \"dist/index.d.ts\",\n";
        oss << "  \"scripts\": {\n";
        for (int i = 0; i < 6; i++) {
            oss << "    \"" << scripts[i] << "\": \"" << scripts[i] << " src/\",\n";
        }
        oss << "    \"prepare\": \"husky install\"\n";
        oss << "  },\n";
        oss << "  \"dependencies\": {\n";
        for (int i = 0; i < 8 + (int)(rng() % 8); i++) {
            oss << "    \"" << deps[rng() % 23] << "\": \"^" << (rng() % 10) << "." << (rng() % 20) << "." << (rng() % 50) << "\"";
            if (i < 7 + (int)(rng() % 8)) oss << ",";
            oss << "\n";
        }
        oss << "  },\n";
        oss << "  \"devDependencies\": {\n";
        for (int i = 0; i < 5 + (int)(rng() % 5); i++) {
            oss << "    \"@types/" << deps[rng() % 23] << "\": \"^" << (rng() % 10) << "." << (rng() % 10) << "." << (rng() % 20) << "\"";
            if (i < 4 + (int)(rng() % 5)) oss << ",";
            oss << "\n";
        }
        oss << "  },\n";
        oss << "  \"repository\": {\n";
        oss << "    \"type\": \"git\",\n";
        oss << "    \"url\": \"git+https://github.com/myorg/package-" << pkg << ".git\"\n";
        oss << "  },\n";
        oss << "  \"license\": \"MIT\"\n";
        oss << "}\n\n";
        pkg++;
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> cargo_toml(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* crates[] = {"serde", "tokio", "actix-web", "axum", "reqwest", "clap", "anyhow", "thiserror",
                            "tracing", "log", "env_logger", "chrono", "uuid", "rand", "regex", "lazy_static",
                            "futures", "async-trait", "parking_lot", "crossbeam", "rayon", "num_cpus"};
    const char* features[] = {"derive", "full", "macros", "runtime-multi-thread", "json", "async", "serde"};

    int pkg = 0;
    while (static_cast<size_t>(oss.tellp()) < n) {
        oss << "[package]\n";
        oss << "name = \"myapp-" << pkg << "\"\n";
        oss << "version = \"" << (rng() % 2) << "." << (rng() % 10) << "." << (rng() % 20) << "\"\n";
        oss << "edition = \"2021\"\n";
        oss << "authors = [\"Developer <dev@example.com>\"]\n";
        oss << "description = \"A Rust application " << pkg << "\"\n";
        oss << "license = \"MIT OR Apache-2.0\"\n";
        oss << "repository = \"https://github.com/myorg/myapp-" << pkg << "\"\n";
        oss << "readme = \"README.md\"\n\n";

        oss << "[dependencies]\n";
        for (int i = 0; i < 10 + (int)(rng() % 10); i++) {
            const char* crate = crates[rng() % 22];
            oss << crate << " = { version = \"" << (rng() % 2) << "." << (rng() % 30) << "\"";
            if (rng() % 3 == 0) {
                oss << ", features = [\"" << features[rng() % 7] << "\"";
                if (rng() % 2) oss << ", \"" << features[rng() % 7] << "\"";
                oss << "]";
            }
            if (rng() % 4 == 0) oss << ", optional = true";
            oss << " }\n";
        }
        oss << "\n";

        oss << "[dev-dependencies]\n";
        oss << "criterion = \"0.5\"\n";
        oss << "proptest = \"1.4\"\n\n";

        oss << "[[bin]]\n";
        oss << "name = \"myapp-" << pkg << "\"\n";
        oss << "path = \"src/main.rs\"\n\n";

        oss << "[profile.release]\n";
        oss << "lto = true\n";
        oss << "codegen-units = 1\n";
        oss << "opt-level = 3\n\n";
        pkg++;
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> makefile(size_t n) {
    std::ostringstream oss;
    std::mt19937 rng(42);

    const char* targets[] = {"all", "build", "clean", "test", "install", "run", "dist", "docs", "lint", "check"};
    const char* vars[] = {"CC", "CXX", "CFLAGS", "CXXFLAGS", "LDFLAGS", "PREFIX", "BINDIR", "SRCDIR", "OBJDIR"};

    oss << "# Makefile generated by generator\n\n";
    oss << "CC      := gcc\n";
    oss << "CXX     := g++\n";
    oss << "CFLAGS  := -Wall -Wextra -O2\n";
    oss << "CXXFLAGS := -Wall -Wextra -O2 -std=c++17\n";
    oss << "LDFLAGS := -lpthread\n";
    oss << "PREFIX  := /usr/local\n";
    oss << "SRCDIR  := src\n";
    oss << "OBJDIR  := obj\n";
    oss << "BINDIR  := bin\n\n";

    int block = 0;
    while (static_cast<size_t>(oss.tellp()) < n) {
        int pattern = rng() % 5;

        if (pattern < 2) {
            const char* target = targets[block % 10];
            oss << ".PHONY: " << target << "-" << block << "\n";
            oss << target << "-" << block << ":";
            if (rng() % 2) oss << " " << targets[rng() % 10] << "-" << (block > 0 ? block - 1 : 0);
            oss << "\n";
            oss << "\t@echo \"Running " << target << " " << block << "...\"\n";
            if (strstr(target, "build") || strstr(target, "all")) {
                oss << "\t$(CXX) $(CXXFLAGS) -o $(BINDIR)/app-" << block << " $(SRCDIR)/main-" << block << ".cpp $(LDFLAGS)\n";
            } else if (strstr(target, "clean")) {
                oss << "\trm -rf $(OBJDIR)/*.o $(BINDIR)/app-" << block << "\n";
            } else if (strstr(target, "test")) {
                oss << "\t./$(BINDIR)/test-" << block << "\n";
            } else {
                oss << "\t@echo \"Done with " << target << " " << block << "\"\n";
            }
            oss << "\n";
        } else if (pattern < 3) {
            oss << "# Object files for module " << block << "\n";
            oss << "OBJS_" << block << " := ";
            for (int i = 0; i < 3 + (int)(rng() % 5); i++) {
                oss << "$(OBJDIR)/file" << block << "_" << i << ".o ";
            }
            oss << "\n\n";
            oss << "$(OBJDIR)/file" << block << "_%.o: $(SRCDIR)/file" << block << "_%.cpp\n";
            oss << "\t$(CXX) $(CXXFLAGS) -c $< -o $@\n\n";
        } else {
            oss << "ifeq ($(DEBUG),1)\n";
            oss << "CFLAGS += -g -DDEBUG\n";
            oss << "CXXFLAGS += -g -DDEBUG\n";
            oss << "endif\n\n";
        }
        block++;
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

// ============================================================================
// BINARY TYPES
// ============================================================================

inline std::vector<uint8_t> image(size_t n) {
    std::vector<uint8_t> out(n);
    for (size_t i = 0; i < n; i += 3) {
        int y = i / 3 / 256;
        out[i] = (y * 2) & 0xFF;
        if (i+1 < n) out[i+1] = (y * 2) & 0xFF;
        if (i+2 < n) out[i+2] = (y * 2) & 0xFF;
    }
    return out;
}

inline std::vector<uint8_t> audio(size_t n) {
    std::vector<uint8_t> out(n);
    int16_t* p = (int16_t*)out.data();
    for (size_t i = 0; i < n/2; i++) {
        double t = i / 44100.0;
        p[i] = (int16_t)(16000 * sin(2 * M_PI * 440 * t));
    }
    return out;
}

inline std::vector<uint8_t> executable(size_t n) {
    std::mt19937 rng(42);
    std::vector<uint8_t> out(n);
    size_t pos = 0;
    if (n >= 512) {
        out[0] = 0x4D; out[1] = 0x5A;
        out[60] = 0x80; out[61] = 0; out[62] = 0; out[63] = 0;
        out[0x80] = 'P'; out[0x81] = 'E'; out[0x82] = 0; out[0x83] = 0;
        for (size_t i = 2; i < 0x80; i++) if (i < 60 || i >= 64) out[i] = 0x90;
        pos = 0x100;
    }
    std::vector<int32_t> targets = {0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000};
    while (pos < n) {
        int choice = rng() % 100;
        if (choice < 20 && pos + 5 <= n) {
            out[pos++] = 0xE8;
            int32_t off = targets[rng() % 6] - (int32_t)(pos + 4);
            out[pos++] = off & 0xFF; out[pos++] = (off >> 8) & 0xFF;
            out[pos++] = (off >> 16) & 0xFF; out[pos++] = (off >> 24) & 0xFF;
        } else if (choice < 25 && pos + 5 <= n) {
            out[pos++] = 0xE9;
            int32_t off = targets[rng() % 6] - (int32_t)(pos + 4);
            out[pos++] = off & 0xFF; out[pos++] = (off >> 8) & 0xFF;
            out[pos++] = (off >> 16) & 0xFF; out[pos++] = (off >> 24) & 0xFF;
        } else if (choice < 40 && pos + 2 <= n) {
            uint8_t ops[] = {0x89, 0x8B, 0x31, 0x33, 0x01, 0x03, 0x29, 0x2B};
            out[pos++] = ops[rng() % 8]; out[pos++] = 0xC0 + (rng() % 64);
        } else if (choice < 50) {
            uint8_t ops[] = {0x90, 0x50, 0x51, 0x52, 0x53, 0x58, 0x59, 0x5A, 0xC3, 0xCC};
            out[pos++] = ops[rng() % 10];
        } else if (choice < 70 && pos + 5 <= n) {
            out[pos++] = 0xB8 + (rng() % 8);
            uint32_t imm = (rng() % 10 < 5) ? 0 : rng() % 256;
            out[pos++] = imm & 0xFF; out[pos++] = (imm >> 8) & 0xFF;
            out[pos++] = (imm >> 16) & 0xFF; out[pos++] = (imm >> 24) & 0xFF;
        } else {
            out[pos++] = rng() % 256;
        }
    }
    return out;
}

inline std::vector<uint8_t> sparse_bitmap(size_t n) {
    std::mt19937 rng(42);
    std::vector<uint8_t> out(n, 0);
    for (size_t i = 0; i < n / 100; i++) {
        out[rng() % n] = 0xFF;
    }
    return out;
}

// ============================================================================
// ADDITIONAL TYPES
// ============================================================================

inline std::vector<uint8_t> markdown(size_t n) {
    std::mt19937 rng(42);
    std::ostringstream oss;

    // Realistic documentation topics and vocabulary
    const char* topics[] = {"Getting Started", "Installation", "Configuration", "API Reference",
                            "Troubleshooting", "Best Practices", "Performance", "Security"};
    const char* verbs[] = {"configure", "install", "setup", "deploy", "initialize", "validate", "optimize"};
    const char* nouns[] = {"database", "server", "client", "connection", "endpoint", "module", "service"};
    const char* adjectives[] = {"primary", "secondary", "custom", "default", "required", "optional"};
    const int nTopics = 8, nVerbs = 7, nNouns = 7, nAdj = 6;

    int section = 0;
    while (static_cast<size_t>(oss.tellp()) < n) {
        int pattern = rng() % 10;

        if (pattern < 2 && section < 20) {
            // Header with topic
            int level = 1 + (rng() % 3);
            for (int i = 0; i < level; i++) oss << "#";
            oss << " " << topics[rng() % nTopics] << "\n\n";
            section++;
        } else if (pattern < 5) {
            // Natural paragraph
            int sentences = 2 + (rng() % 4);
            for (int s = 0; s < sentences; s++) {
                oss << "The " << adjectives[rng() % nAdj] << " " << nouns[rng() % nNouns];
                oss << " should " << verbs[rng() % nVerbs] << " the " << nouns[rng() % nNouns];
                if (rng() % 2) oss << " with **" << adjectives[rng() % nAdj] << "** settings";
                oss << ". ";
            }
            oss << "\n\n";
        } else if (pattern < 6) {
            // Bullet list
            int items = 3 + (rng() % 4);
            for (int i = 0; i < items; i++) {
                oss << "- " << verbs[rng() % nVerbs] << " the " << nouns[rng() % nNouns] << "\n";
            }
            oss << "\n";
        } else if (pattern < 7) {
            // Code block with varied language
            const char* langs[] = {"python", "bash", "javascript", "yaml", "json"};
            oss << "```" << langs[rng() % 5] << "\n";
            oss << "# " << topics[rng() % nTopics] << " example\n";
            oss << nouns[rng() % nNouns] << " = " << verbs[rng() % nVerbs] << "()\n";
            oss << nouns[rng() % nNouns] << "." << verbs[rng() % nVerbs] << "('" << nouns[rng() % nNouns] << "')\n";
            oss << "```\n\n";
        } else if (pattern < 8) {
            // Table
            oss << "| Option | Description | Default |\n";
            oss << "|--------|-------------|--------|\n";
            for (int r = 0; r < 3 + (int)(rng() % 3); r++) {
                oss << "| " << nouns[rng() % nNouns] << " | " << adjectives[rng() % nAdj] << " " << nouns[rng() % nNouns] << " | `" << (rng() % 100) << "` |\n";
            }
            oss << "\n";
        } else if (pattern < 9) {
            // Blockquote / note
            oss << "> **Note**: " << verbs[rng() % nVerbs] << " the " << adjectives[rng() % nAdj];
            oss << " " << nouns[rng() % nNouns] << " before proceeding.\n\n";
        } else {
            // Inline code mentions
            oss << "Use `" << nouns[rng() % nNouns] << "." << verbs[rng() % nVerbs] << "()` to ";
            oss << verbs[rng() % nVerbs] << " the " << nouns[rng() % nNouns] << ".\n\n";
        }
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

// WORD_TEMPLATE: Repeating sections where one word varies throughout
// Each section uses the same word 7 times - perfect for WORD_TEMPLATE detection
inline std::vector<uint8_t> word_template(size_t n) {
    // Template uses {W} placeholder 7 times per section
    std::string templ =
        "## {W} API\n\n"
        "The {W} component provides {W} management functionality.\n\n"
        "### GET /api/{W}\n\n"
        "Retrieve {W} data from the {W} store.\n\n"
        "### POST /api/{W}\n\n"
        "Create a new {W} entry.\n\n";

    // Words to cycle through - diverse enough for realistic content
    std::vector<std::string> words = {
        "session", "result", "config", "buffer", "stream", "handler",
        "worker", "manager", "client", "server", "request", "response",
        "cache", "queue", "event", "signal", "thread", "process",
        "socket", "channel", "context", "router", "filter", "adapter"
    };

    std::string header = "# API Documentation\n\nThis document describes all available APIs.\n\n";
    std::string data = header;

    size_t word_idx = 0;
    while (data.size() < n) {
        std::string section = templ;
        std::string word = words[word_idx % words.size()];
        size_t pos = 0;
        while ((pos = section.find("{W}", pos)) != std::string::npos) {
            section.replace(pos, 3, word);
            pos += word.size();
        }
        data += section;
        word_idx++;
    }

    data.resize(n);
    return std::vector<uint8_t>(data.begin(), data.end());
}

inline std::vector<uint8_t> protobuf_like(size_t n) {
    std::mt19937 rng(42);
    std::vector<uint8_t> out(n);
    for (size_t i = 0; i < n; ) {
        out[i++] = 0x08 + (rng() % 8);
        if (i >= n) break;
        out[i++] = rng() % 128;
    }
    return out;
}

inline std::vector<uint8_t> metrics(size_t n) {
    std::mt19937 rng(42);
    std::ostringstream oss;

    // Prometheus-style metric names
    const char* metrics[] = {"http_requests_total", "cpu_usage_percent", "memory_bytes",
                             "disk_io_bytes", "network_rx_bytes", "network_tx_bytes",
                             "process_open_fds", "gc_pause_seconds", "request_duration_seconds",
                             "queue_length", "cache_hits_total", "cache_misses_total"};
    const char* hosts[] = {"web01", "web02", "db01", "db02", "cache01", "api01", "worker01"};
    const char* envs[] = {"prod", "staging", "dev"};
    const char* methods[] = {"GET", "POST", "PUT", "DELETE"};
    const char* codes[] = {"200", "201", "400", "404", "500"};
    const int nMetrics = 12, nHosts = 7, nEnvs = 3, nMethods = 4, nCodes = 5;

    uint64_t ts = 1700000000000;
    while (static_cast<size_t>(oss.tellp()) < n) {
        const char* metric = metrics[rng() % nMetrics];
        oss << metric << "{";
        oss << "host=\"" << hosts[rng() % nHosts] << "\"";
        oss << ",env=\"" << envs[rng() % nEnvs] << "\"";

        // Add type-specific labels
        if (strstr(metric, "http") || strstr(metric, "request")) {
            oss << ",method=\"" << methods[rng() % nMethods] << "\"";
            oss << ",status=\"" << codes[rng() % nCodes] << "\"";
        } else if (strstr(metric, "disk") || strstr(metric, "network")) {
            oss << ",device=\"" << (rng() % 2 ? "sda" : "eth0") << "\"";
        }

        oss << "} ";
        // Varied value ranges per metric type
        if (strstr(metric, "percent")) {
            oss << std::fixed << std::setprecision(2) << (rng() % 10000) / 100.0;
        } else if (strstr(metric, "bytes")) {
            oss << (rng() % 1000000000);
        } else if (strstr(metric, "seconds")) {
            oss << std::fixed << std::setprecision(6) << (rng() % 1000000) / 1000000.0;
        } else {
            oss << (rng() % 10000);
        }
        ts += rng() % 1000;
        oss << " " << ts << "\n";
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> yaml_config(size_t n) {
    std::mt19937 rng(42);
    std::ostringstream oss;

    const char* services[] = {"api", "auth", "cache", "db", "queue", "worker", "gateway", "metrics"};
    const char* envs[] = {"development", "staging", "production"};
    const char* loglevels[] = {"debug", "info", "warn", "error"};
    const char* drivers[] = {"postgres", "mysql", "redis", "mongodb"};
    const int nSvc = 8, nEnv = 3, nLog = 4, nDrv = 4;

    while (static_cast<size_t>(oss.tellp()) < n) {
        int pattern = rng() % 5;

        if (pattern == 0) {
            // Service definition with nested config
            const char* svc = services[rng() % nSvc];
            oss << svc << ":\n";
            oss << "  enabled: " << (rng() % 2 ? "true" : "false") << "\n";
            oss << "  port: " << (3000 + rng() % 6000) << "\n";
            oss << "  host: " << (rng() % 2 ? "0.0.0.0" : "localhost") << "\n";
            oss << "  logging:\n";
            oss << "    level: " << loglevels[rng() % nLog] << "\n";
            oss << "    format: " << (rng() % 2 ? "json" : "text") << "\n";
            oss << "  resources:\n";
            oss << "    cpu: " << (100 + rng() % 900) << "m\n";
            oss << "    memory: " << (128 + rng() % 896) << "Mi\n\n";
        } else if (pattern == 1) {
            // Database config
            oss << "database:\n";
            oss << "  driver: " << drivers[rng() % nDrv] << "\n";
            oss << "  host: db." << services[rng() % nSvc] << ".local\n";
            oss << "  port: " << (5432 + rng() % 1000) << "\n";
            oss << "  pool:\n";
            oss << "    min: " << (1 + rng() % 5) << "\n";
            oss << "    max: " << (10 + rng() % 90) << "\n";
            oss << "    idle_timeout: " << (30 + rng() % 270) << "s\n\n";
        } else if (pattern == 2) {
            // Environment block
            oss << "environment: " << envs[rng() % nEnv] << "\n\n";
            oss << "secrets:\n";
            oss << "  - name: api_key\n";
            oss << "    vault_path: secret/" << services[rng() % nSvc] << "/api_key\n";
            oss << "  - name: db_password\n";
            oss << "    vault_path: secret/database/password\n\n";
        } else if (pattern == 3) {
            // Feature flags
            oss << "features:\n";
            for (int f = 0; f < 3 + (int)(rng() % 4); f++) {
                oss << "  " << services[rng() % nSvc] << "_" << (rng() % 2 ? "enabled" : "beta") << ": ";
                oss << (rng() % 2 ? "true" : "false") << "\n";
            }
            oss << "\n";
        } else {
            // Replica/scaling config
            oss << "replicas:\n";
            oss << "  min: " << (1 + rng() % 3) << "\n";
            oss << "  max: " << (5 + rng() % 15) << "\n";
            oss << "  target_cpu: " << (50 + rng() % 30) << "\n";
            oss << "  scale_down_delay: " << (60 + rng() % 240) << "s\n\n";
        }
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> unicode_text(size_t n) {
    std::mt19937 rng(42);
    std::ostringstream oss;
    // Actual multilingual Unicode phrases
    const char* phrases[] = {
        u8"Hello world! ",           // English
        u8"Bonjour le monde! ",      // French
        u8"Hallo Welt! ",            // German
        u8"Привет мир! ",            // Russian (Cyrillic)
        u8"你好世界！",               // Chinese
        u8"こんにちは世界！",         // Japanese
        u8"안녕하세요 세계! ",        // Korean
        u8"مرحبا بالعالم! ",         // Arabic
        u8"שלום עולם! ",             // Hebrew
        u8"Γειά σου κόσμε! ",        // Greek
        u8"สวัสดีโลก! ",              // Thai
        u8"Xin chào thế giới! ",     // Vietnamese
        u8"Olá mundo! ",             // Portuguese
        u8"नमस्ते दुनिया! ",           // Hindi
        u8"Merhaba dünya! ",         // Turkish
        u8"🌍🌎🌏 Earth! ",           // Emojis
    };
    while (static_cast<size_t>(oss.tellp()) < n) {
        oss << phrases[rng() % 16];
    }
    std::string s = oss.str();
    // Truncate at UTF-8 boundary
    while (s.size() > n && !s.empty()) {
        s.pop_back();
    }
    // Ensure we don't cut in the middle of a UTF-8 sequence
    while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80) {
        s.pop_back();
    }
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> email_headers(size_t n) {
    std::mt19937 rng(42);
    std::ostringstream oss;

    const char* names[] = {"alice", "bob", "carol", "david", "emma", "frank", "grace", "henry"};
    const char* domains[] = {"gmail.com", "outlook.com", "company.org", "fastmail.com", "proton.me"};
    const char* subjects[] = {"Meeting tomorrow", "Quick question", "Project update", "Invoice attached",
                              "Vacation request", "Bug report", "Code review needed", "Weekly status"};
    const char* days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const int nNames = 8, nDom = 5, nSubj = 8;

    while (static_cast<size_t>(oss.tellp()) < n) {
        int pattern = rng() % 4;

        // Basic headers always present
        oss << "From: " << names[rng() % nNames] << "." << names[rng() % nNames] << "@" << domains[rng() % nDom] << "\n";
        oss << "To: " << names[rng() % nNames] << "@" << domains[rng() % nDom];
        if (rng() % 3 == 0) {
            oss << ", " << names[rng() % nNames] << "@" << domains[rng() % nDom];
        }
        oss << "\n";

        if (rng() % 2) {
            oss << "Cc: " << names[rng() % nNames] << "@" << domains[rng() % nDom] << "\n";
        }

        oss << "Subject: ";
        if (rng() % 4 == 0) oss << "Re: ";
        if (rng() % 5 == 0) oss << "Fwd: ";
        oss << subjects[rng() % nSubj] << "\n";

        oss << "Date: " << days[rng() % 7] << ", " << (1 + rng() % 28) << " " << months[rng() % 12];
        oss << " " << (2020 + rng() % 6) << " " << std::setw(2) << std::setfill('0') << (rng() % 24);
        oss << ":" << std::setw(2) << std::setfill('0') << (rng() % 60) << ":00 ";
        oss << (rng() % 2 ? "+" : "-") << std::setw(2) << std::setfill('0') << (rng() % 12) << "00\n";

        oss << "Message-ID: <" << std::hex << (rng() % 0xFFFFFF) << std::dec << "." << (rng() % 10000) << "@" << domains[rng() % nDom] << ">\n";

        // Varied additional headers
        if (pattern == 0) {
            oss << "X-Priority: " << (1 + rng() % 5) << "\n";
            oss << "X-Mailer: Thunderbird/" << (70 + rng() % 30) << ".0\n";
        } else if (pattern == 1) {
            oss << "MIME-Version: 1.0\n";
            oss << "Content-Type: multipart/mixed; boundary=\"----=_Part_" << (rng() % 1000) << "\"\n";
        } else if (pattern == 2) {
            oss << "In-Reply-To: <" << std::hex << (rng() % 0xFFFF) << std::dec << "@" << domains[rng() % nDom] << ">\n";
            oss << "References: <" << std::hex << (rng() % 0xFFFF) << std::dec << "@" << domains[rng() % nDom] << ">\n";
        } else {
            oss << "Content-Type: text/plain; charset=utf-8\n";
            oss << "Content-Transfer-Encoding: 7bit\n";
        }
        oss << "\n---\n\n";
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> graphql(size_t n) {
    std::mt19937 rng(42);
    std::ostringstream oss;

    const char* types[] = {"User", "Post", "Comment", "Product", "Order", "Category", "Review"};
    const char* scalars[] = {"id", "name", "email", "title", "content", "price", "quantity", "status", "createdAt"};
    const char* actions[] = {"create", "update", "delete", "get", "list", "search"};
    const int nTypes = 7, nScalars = 9, nActions = 6;

    while (static_cast<size_t>(oss.tellp()) < n) {
        int pattern = rng() % 6;

        if (pattern == 0) {
            // Query with arguments
            const char* type = types[rng() % nTypes];
            oss << "query Get" << type << " {\n";
            oss << "  " << (char)tolower(type[0]) << (type + 1) << "(id: \"" << (rng() % 10000) << "\") {\n";
            int fields = 3 + (rng() % 4);
            for (int f = 0; f < fields; f++) {
                oss << "    " << scalars[rng() % nScalars] << "\n";
            }
            oss << "  }\n}\n\n";
        } else if (pattern == 1) {
            // Mutation
            const char* type = types[rng() % nTypes];
            const char* action = actions[rng() % 3]; // create/update/delete
            oss << "mutation " << (char)toupper(action[0]) << (action + 1) << type << "($input: " << type << "Input!) {\n";
            oss << "  " << action << type << "(input: $input) {\n";
            oss << "    id\n";
            oss << "    " << scalars[rng() % nScalars] << "\n";
            if (rng() % 2) oss << "    " << scalars[rng() % nScalars] << "\n";
            oss << "  }\n}\n\n";
        } else if (pattern == 2) {
            // Query with nested relations
            oss << "query Get" << types[rng() % nTypes] << "WithRelations {\n";
            oss << "  " << scalars[0] << "\n"; // id
            oss << "  " << scalars[rng() % nScalars] << "\n";
            oss << "  " << (char)tolower(types[rng() % nTypes][0]) << "s {\n"; // relation
            oss << "    id\n";
            oss << "    " << scalars[rng() % nScalars] << "\n";
            oss << "  }\n}\n\n";
        } else if (pattern == 3) {
            // Fragment definition
            const char* type = types[rng() % nTypes];
            oss << "fragment " << type << "Fields on " << type << " {\n";
            int fields = 2 + (rng() % 5);
            for (int f = 0; f < fields; f++) {
                oss << "  " << scalars[rng() % nScalars] << "\n";
            }
            oss << "}\n\n";
        } else if (pattern == 4) {
            // List query with pagination
            const char* type = types[rng() % nTypes];
            oss << "query List" << type << "s($first: Int, $after: String) {\n";
            oss << "  " << (char)tolower(type[0]) << "s(first: $first, after: $after) {\n";
            oss << "    edges {\n";
            oss << "      node {\n";
            oss << "        id\n        " << scalars[rng() % nScalars] << "\n";
            oss << "      }\n      cursor\n    }\n";
            oss << "    pageInfo { hasNextPage endCursor }\n";
            oss << "  }\n}\n\n";
        } else {
            // Subscription
            const char* type = types[rng() % nTypes];
            oss << "subscription On" << type << "Updated {\n";
            oss << "  " << (char)tolower(type[0]) << "Updated {\n";
            oss << "    id\n    " << scalars[rng() % nScalars] << "\n";
            oss << "  }\n}\n\n";
        }
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> k8s_manifest(size_t n) {
    std::mt19937 rng(42);
    std::ostringstream oss;

    const char* apps[] = {"frontend", "backend", "api", "worker", "cache", "db", "gateway", "auth"};
    const char* images[] = {"nginx", "node", "python", "golang", "redis", "postgres", "envoy"};
    const char* envs[] = {"production", "staging", "development"};
    const int nApps = 8, nImages = 7, nEnvs = 3;

    while (static_cast<size_t>(oss.tellp()) < n) {
        int pattern = rng() % 6;
        const char* app = apps[rng() % nApps];
        const char* env = envs[rng() % nEnvs];

        if (pattern == 0) {
            // Deployment
            oss << "apiVersion: apps/v1\nkind: Deployment\nmetadata:\n";
            oss << "  name: " << app << "-deployment\n";
            oss << "  namespace: " << env << "\n";
            oss << "  labels:\n    app: " << app << "\n    env: " << env << "\n";
            oss << "spec:\n  replicas: " << (1 + rng() % 5) << "\n";
            oss << "  selector:\n    matchLabels:\n      app: " << app << "\n";
            oss << "  template:\n    metadata:\n      labels:\n        app: " << app << "\n";
            oss << "    spec:\n      containers:\n      - name: " << app << "\n";
            oss << "        image: " << images[rng() % nImages] << ":1." << (rng() % 20) << "\n";
            oss << "        ports:\n        - containerPort: " << (3000 + rng() % 5000) << "\n";
            oss << "        resources:\n          limits:\n            cpu: " << (100 + rng() % 900) << "m\n";
            oss << "            memory: " << (128 + rng() % 896) << "Mi\n";
        } else if (pattern == 1) {
            // Service
            oss << "apiVersion: v1\nkind: Service\nmetadata:\n";
            oss << "  name: " << app << "-service\n";
            oss << "  namespace: " << env << "\n";
            oss << "spec:\n  type: " << (rng() % 2 ? "ClusterIP" : "LoadBalancer") << "\n";
            oss << "  ports:\n  - port: " << (80 + rng() % 443) << "\n";
            oss << "    targetPort: " << (3000 + rng() % 5000) << "\n";
            oss << "  selector:\n    app: " << app << "\n";
        } else if (pattern == 2) {
            // ConfigMap
            oss << "apiVersion: v1\nkind: ConfigMap\nmetadata:\n";
            oss << "  name: " << app << "-config\n  namespace: " << env << "\n";
            oss << "data:\n";
            oss << "  LOG_LEVEL: " << (rng() % 2 ? "info" : "debug") << "\n";
            oss << "  API_URL: http://" << apps[rng() % nApps] << "-service:8080\n";
            oss << "  CACHE_TTL: \"" << (60 + rng() % 3600) << "\"\n";
        } else if (pattern == 3) {
            // Secret
            oss << "apiVersion: v1\nkind: Secret\nmetadata:\n";
            oss << "  name: " << app << "-secret\n  namespace: " << env << "\n";
            oss << "type: Opaque\nstringData:\n";
            oss << "  DB_PASSWORD: " << std::hex << (rng() % 0xFFFFFFFF) << std::dec << "\n";
            oss << "  API_KEY: key-" << (rng() % 100000) << "\n";
        } else if (pattern == 4) {
            // Ingress
            oss << "apiVersion: networking.k8s.io/v1\nkind: Ingress\nmetadata:\n";
            oss << "  name: " << app << "-ingress\n  namespace: " << env << "\n";
            oss << "  annotations:\n    nginx.ingress.kubernetes.io/rewrite-target: /\n";
            oss << "spec:\n  rules:\n  - host: " << app << "." << env << ".example.com\n";
            oss << "    http:\n      paths:\n      - path: /\n        pathType: Prefix\n";
            oss << "        backend:\n          service:\n            name: " << app << "-service\n";
            oss << "            port:\n              number: 80\n";
        } else {
            // HorizontalPodAutoscaler
            oss << "apiVersion: autoscaling/v2\nkind: HorizontalPodAutoscaler\nmetadata:\n";
            oss << "  name: " << app << "-hpa\n  namespace: " << env << "\n";
            oss << "spec:\n  scaleTargetRef:\n    apiVersion: apps/v1\n    kind: Deployment\n";
            oss << "    name: " << app << "-deployment\n";
            oss << "  minReplicas: " << (1 + rng() % 3) << "\n";
            oss << "  maxReplicas: " << (5 + rng() % 15) << "\n";
            oss << "  metrics:\n  - type: Resource\n    resource:\n      name: cpu\n";
            oss << "      target:\n        type: Utilization\n        averageUtilization: " << (50 + rng() % 30) << "\n";
        }
        oss << "---\n";
    }
    auto s = oss.str().substr(0, n);
    return std::vector<uint8_t>(s.begin(), s.end());
}

// ============================================================================
// TYPE REGISTRY
// ============================================================================

struct DataType {
    std::string key;
    std::string name;
    std::string category;
    std::function<std::vector<uint8_t>(size_t)> generator;
};

inline const std::vector<DataType>& all_types() {
    static std::vector<DataType> types = {
        // NUMERIC
        {"timestamps",      "Timestamps (regular)", "NUMERIC", timestamps_regular},
        {"timestamps_j",    "Timestamps (jitter)",  "NUMERIC", timestamps_jitter},
        {"db_ids",          "Database IDs",         "NUMERIC", db_ids},
        {"sensor",          "Sensor 16-bit",        "NUMERIC", sensor_16bit},
        {"gps",             "GPS coordinates",      "NUMERIC", gps_track},
        {"floats",          "Float temperature",    "NUMERIC", floats},
        {"random",          "Random bytes",         "NUMERIC", random_bytes},
        {"ints",            "Integer array",        "NUMERIC", int_array},
        // TEXT
        {"text",            "Natural text",         "TEXT", natural_text},
        {"access_log",      "Access log",           "TEXT", access_log},
        {"syslog",          "Syslog",               "TEXT", syslog},
        // STRUCTURED
        {"json",            "JSON API",             "STRUCTURED", json},
        {"csv",             "CSV data",             "STRUCTURED", csv},
        {"sql",             "SQL dump",             "STRUCTURED", sql},
        {"xml",             "XML document",         "STRUCTURED", xml},
        {"base64",          "Base64 data",          "STRUCTURED", base64},
        {"ini",             "INI config",           "STRUCTURED", ini_config},
        // CODE
        {"javascript",      "JavaScript",           "CODE", javascript},
        {"python",          "Python",               "CODE", python},
        {"html",            "HTML",                 "CODE", html},
        {"typescript",      "TypeScript/JSX",       "CODE", typescript},
        {"go",              "Go",                   "CODE", golang},
        {"rust",            "Rust",                 "CODE", rust},
        {"c",               "C",                    "CODE", c_code},
        {"java",            "Java",                 "CODE", java},
        {"css",             "CSS",                  "CODE", css},
        {"bash",            "Bash script",          "CODE", bash_script},
        {"php",             "PHP",                  "CODE", php},
        // LOG
        {"nginx_log",       "Nginx access log",     "LOG", nginx_log},
        {"json_log",        "JSON structured log",  "LOG", json_log},
        // CONFIG
        {"docker_compose",  "Docker Compose",       "CONFIG", docker_compose},
        {"terraform",       "Terraform HCL",        "CONFIG", terraform},
        {"env",             ".env file",            "CONFIG", env_file},
        {"toml",            "TOML config",          "CONFIG", toml_config},
        // BUILD
        {"package_json",    "package.json",         "BUILD", package_json},
        {"cargo_toml",      "Cargo.toml",           "BUILD", cargo_toml},
        {"makefile",        "Makefile",             "BUILD", makefile},
        // BINARY
        {"image",           "Image gradient",       "BINARY", image},
        {"audio",           "Audio PCM",            "BINARY", audio},
        {"executable",      "Executable",           "BINARY", executable},
        {"sparse",          "Sparse bitmap",        "BINARY", sparse_bitmap},
        // ADDITIONAL
        {"markdown",        "Markdown docs",        "ADDITIONAL", markdown},
        {"word_template",   "Word template",        "ADDITIONAL", word_template},
        {"protobuf",        "Protobuf-like",        "ADDITIONAL", protobuf_like},
        {"metrics",         "Metrics/TSeries",      "ADDITIONAL", metrics},
        {"yaml",            "YAML config",          "ADDITIONAL", yaml_config},
        {"unicode",         "Unicode text",         "ADDITIONAL", unicode_text},
        {"email",           "Email headers",        "ADDITIONAL", email_headers},
        {"graphql",         "GraphQL queries",      "ADDITIONAL", graphql},
        {"k8s",             "K8s manifests",        "ADDITIONAL", k8s_manifest},
    };
    return types;
}

// Get generator by key
inline std::function<std::vector<uint8_t>(size_t)> get(const std::string& key) {
    for (const auto& t : all_types()) {
        if (t.key == key) return t.generator;
    }
    return nullptr;
}

} // namespace gen
