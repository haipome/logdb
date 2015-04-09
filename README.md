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

