#!/bin/bash

PATH="/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin"
SERVER_NAME=""

function usleep()
{
    second=`echo $1 | python -c "import sys;print int(sys.stdin.read()) / 1000000.0"`
    sleep $second
}


function ini_read()
(
    typeset INI_FILE=$1;
    typeset INI_SECTION=$2;
    typeset INI_KEY=$3;

    if [ ! -f "${INI_FILE}" ]
    then
        echo "Config file [$INI_FILE] is not exists." >&2
        exit 1
    fi
    
    if [ -n "${INI_SECTION}" ]
    then
        sed -n "/^\[[ \t]*${INI_SECTION}[ \t]*\]/,/^\[.*\]/p" "${INI_FILE}" \
            | grep "^[ \t]*$INI_KEY[ \t]*=" \
            | tail -n 1 \
            | cut -d '=' -f 2- \
            | sed "s/^[ \t]*//g" \
            | sed "s/[ \t]*$//g"
    else
        sed -n "1,/^\[.*\]/p" "${INI_FILE}" \
            | grep "^[ \t]*${INI_KEY}[ \t]*=" \
            | tail -n 1 \
            | cut -d '=' -f 2- \
            | sed "s/^[ \t]*//g" \
            | sed "s/[ \t]*$//g"
    fi
)

function service_stop()
{
    echo "stop server ..."
    killall -s SIGQUIT logdb_${SERVER_NAME}
    usleep 500000
    echo "done"
}

function service_start()
{
    echo "start server ..."
    ./bin/logdb_${SERVER_NAME} -c conf/default.ini
    usleep 500000
    echo "done"
}

function service_syncdb()
{
    ./bin/logdb -c conf/default.ini --syncdb
}

function generate_api()
{
    ./bin/logdb -c conf/default.ini --api
}

function install_crontab()
{
    crontab_bak="/tmp/crontab.`date +%Y%m%d`"
    crontab -l > ${crontab_bak}
    check_alive_count=`cat ${crontab_bak} |
        grep "logdb_${1}_check_alive.sh" |
            grep -v grep |
                awk '/^[^#]/ { print $0 }' | wc -l`
    if [ ${check_alive_count} -eq 0 ]
    then
        project_path=`pwd`
        echo "*/1 * * * * ${project_path}/shell/logdb_${1}_check_alive.sh >/dev/null 2>&1" >> ${crontab_bak}
        crontab ${crontab_bak}
    fi
}

function uninstall_crontab()
{
    crontab_bak="/tmp/crontab.`date +%Y%m%d`"
    crontab -l > ${crontab_bak}
    check_alive_count=`cat ${crontab_bak} |
        grep "logdb_${1}_check_alive.sh" |
            grep -v grep |
                awk '/^[^#]/ { print $0 }' | wc -l`
    if [ ${check_alive_count} -gt 0 ]
    then
        cat ${crontab_bak} | 
            awk "! /logdb_${1}_check_alive.sh/" > ${crontab_bak}.tmp
        crontab ${crontab_bak}.tmp
    fi
}

function service_deploy()
{
    echo -n "rename bin file ... "
    cp -f bin/logdb  bin/logdb_${SERVER_NAME}
    cp -f bin/loginf bin/logdb_${SERVER_NAME}_inf
    chmod a+x bin/*
    echo "done"

    echo "start server ..."
    ./bin/logdb_${SERVER_NAME} -c conf/default.ini
    if [ $? -ne 0 ]
    then
        echo "fail"
        exit 1
    fi
    usleep 500000
    echo "done"

    echo "start interface ... "
    ./bin/logdb_${SERVER_NAME}_inf -c conf/default.ini
    if [ $? -ne 0 ]
    then
        echo "fail"
        killall -s SIGQUIT logdb_${SERVER_NAME}
        exit 1
    fi
    usleep 500000
    echo "done"

    echo "install check alive crontab"
    proc_num=`ini_read conf/default.ini "global" "worker process num"`
    if [ -z "${proc_num}" ]
    then
        proc_num=1
    fi
    proc_num=$((${proc_num} + 1))
    project_path=`pwd`
    cat shell/check_alive.sh.template \
        | sed "s;\<logdb\>;logdb_${SERVER_NAME};g" \
        | sed "s;\<loginf\>;logdb_${SERVER_NAME}_inf;g" \
        | sed "s;\<procnum\>;${proc_num};g" \
        | sed "s;\<logpath\>;${project_path};g" > \
            shell/logdb_${SERVER_NAME}_check_alive.sh

    chmod a+x shell/*
    install_crontab ${SERVER_NAME}
    echo "done"

    generate_api
}

function service_shutdown
{
    echo "stop interface ..."
    killall -s SIGQUIT logdb_${SERVER_NAME}_inf
    usleep 500000
    echo "done"

    echo "stop server ..."
    killall -s SIGQUIT logdb_${SERVER_NAME}
    usleep 500000
    echo "done"

    echo "uninstall check alive crontab"
    uninstall_crontab ${SERVER_NAME}
    echo "done"
}

if [ $# -eq 0 ]
then
    echo -e "Usage: $0 { deploy | stop | start | restart | shutdown | syncdb | api }"
    exit 1
fi

cd `dirname $0`

server_name=`ini_read conf/default.ini "global" "server name"`
SERVER_NAME=`echo ${server_name} | awk '{ print $1 }'`

if [ -z "${SERVER_NAME}" ]
then
    echo "'server name' is not set!"
    exit 1
fi

case $1 in
    deploy)
    echo "SERVICE DEPLOY ..."
    service_deploy
    ;;
    stop)
    echo "SERVICE STOP ..."
    service_stop
    ;;
    start)
    echo "SERVICE START ..."
    service_start
    ;;
    restart)
    echo "SERVICE STOP ..."
    service_stop
    echo "SERVICE START ..."
    service_start
    ;;
    shutdown)
    echo "SERVICE SHUTDOWN ..."
    service_shutdown
    ;;
    syncdb)
    echo "SYNCDB ..."
    service_syncdb
    ;;
    api)
    echo "GENERATE API ..."
    generate_api
    ;;
    *)
    echo -e "Usage: $0 { deploy | stop | start | restart | shutdown | syncdb | api }"
esac

exit 0

