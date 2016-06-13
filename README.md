logdb
=====

A logging system based on mysql for logging structured messages.

High performance, self defined fields, auto deployment, auto creation and updates of table structure, auto table switching and auto generation of C API.

中文介绍：http://blog.haipo.me/?p=1176

# Deployment

Logdb is usually deployed in following directory structure:

```
logdb_test
|---bin
|   |---logdb
|   |---loginf
|---conf
|   |---default.ini
|---log
|---manage.sh
|---shell
|   |---check_alive.sh.template
|   |---load_fail_log.sh
```

Make sure to run only one instance of logdb from one directory. If you need more instances of logdb on the same machine, use different directories.

Installation
-------
1. clone the github source files.
2. `cd` the `src` directory 
3. `make` and `make install`
4. edit the `default.ini` in `./conf/` directory
5. cd the `./` path, and `./manage.sh deploy`
6. done.

Compilation
-----------
Before `make`, please make sure `libmysqlclient` is installed.
