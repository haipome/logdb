# LogDB

A high-performance logging system based on MySQL for storing structured messages.

## Features

- **Custom fields** — Define columns via configuration with support for integers, floats, strings, binary, and time types.
- **Auto table management** — Automatic creation, schema updates, and time-based table rotation (hourly, daily, monthly, yearly).
- **Hash-based sharding** — Distribute data across multiple tables with optional MERGE table for unified queries.
- **Async batch insertion** — Receiver process buffers incoming logs and dispatches them to worker processes for batch INSERT.
- **Persistent queues** — Shared memory circular queues with file-based overflow and crash recovery.
- **Global sequence generation** — Built-in distributed auto-increment sequence.
- **Auto-generated C API** — Type-safe client API headers generated from your column definitions.

## Dependencies

- [libmysqlclient](https://dev.mysql.com/downloads/c-api/) — MySQL C client library
- [packf](https://github.com/haipome/packf) — Lightweight binary serialization library using printf-style format strings

## Directory Structure

LogDB is usually deployed in the following directory structure:

```
logdb_test/
├── bin/
│   ├── logdb
│   └── loginf
├── conf/
│   └── default.ini
├── log/
├── manage.sh
└── shell/
    ├── check_alive.sh.template
    └── load_fail_log.sh
```

Make sure to run only one instance of logdb from one directory. If you need more instances on the same machine, use different directories.

## Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/haipome/logdb.git
   cd logdb
   ```

2. Clone and build the [packf](https://github.com/haipome/packf) dependency:
   ```bash
   git clone https://github.com/haipome/packf.git
   ```

3. Make sure `libmysqlclient` is installed on your system.

4. Build:
   ```bash
   cd src
   make
   make install
   ```

5. Edit the configuration file `conf/default.ini`.

6. Deploy:
   ```bash
   ./manage.sh deploy
   ```

## Usage

```bash
# Synchronize database schema (create/update tables)
./bin/logdb -c conf/default.ini --syncdb

# Generate C client API
./bin/logdb -c conf/default.ini --api

# Start the server
./bin/logdb -c conf/default.ini
```

### Command-Line Options

| Option | Description |
|--------|-------------|
| `-c <file>` | Specify configuration file (required) |
| `-s, --syncdb` | Synchronize database table schema |
| `-a, --api` | Generate C API header and source files |
| `-m, --merge` | Create MERGE table for sharded tables |
| `-q, --queue-stat` | Print queue statistics |
| `-r, --rm-queue` | Remove all shared memory queues |
| `-f N, --offset=N` | Time offset for backfilling old partitions |

## Configuration

Edit `conf/default.ini` to configure the server. Key sections:

### Global

```ini
[global]
server name = mylog
listen port = 22060
worker process num = 1
queue base shm key = 10000
```

### Database

```ini
db name = my_database
db host = localhost
db port = 3306
db table name = my_log
db engine = MyISAM
db charset = utf8
```

### Table Rotation & Sharding

```ini
; Supported shift types: hour, day, month, year
db shift table type = day
db keep time = 30

hash table num = 4
hash table column = uid
```

### Column Definitions

```ini
columns = id, time, uid, name

[id]
type              = unsigned int
global sequence   = true

[time]
type              = unsigned int
current timestamp = true

[uid]
type              = unsigned int
index             = true

[name]
type              = varchar
length            = 64
```

#### Supported Column Types

| Category | Types |
|----------|-------|
| Integer | `tinyint`, `smallint`, `int`, `bigint` (with optional `unsigned`) |
| Float | `float`, `double` |
| String | `char`, `varchar`, `tinytext`, `text` |
| Binary | `binary`, `varbinary`, `tinyblob`, `blob` |
| Time | `date`, `time`, `datetime` |

#### Column Attributes

| Attribute | Applies To | Default | Description |
|-----------|-----------|---------|-------------|
| `index` | All | false | Create database index |
| `primary` | All | false | Primary key |
| `storage` | All | true | Store in database |
| `zero` | All | false | Default to zero value |
| `current timestamp` | Integer, Time | false | Auto-fill with current time |
| `global sequence` | Integer | false | Use global sequence generator |
| `auto increment` | Integer | false | MySQL auto increment |
| `sender ip` | Integer | false | Auto-fill with sender's IP |
| `sender port` | Integer | false | Auto-fill with sender's port |
| `length` | String, Binary | — | Field length (required) |
| `const length` | String, Binary | false | Fixed-length field |
| `zero end` | String | false | Null-terminated string |
| `unix timestamp` | Time | false | Store as unix timestamp |

## License

See [LICENSE](LICENSE) for details.
