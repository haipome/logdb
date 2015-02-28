logdb
=====

A logging system base on mysql, logging structured message.

High performance, self definition fields, auto deployment, auto create db table and update table, auto switch table, atuo generate C API.

# Deployment

Usually deploy logdb in this strcture:

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

Make sure only run one unit of logdb on one folder. If you have many unit of logdb on the same machine, use different folder.

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
Before `make`, you should make sure the `libmysqlclient` installed.
