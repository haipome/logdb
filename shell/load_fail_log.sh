#!/bin/sh

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

function ini_read_default()
{
    INI_RESULT=`ini_read "$1" "$2" "$3"`
    if [ -n "${INI_RESULT}" ]
    then
        echo ${INI_RESULT}
    else
        echo $4
    fi
}

if [ $# -ne 1 ]
then
    echo "Usage $0 fail.log"
    exit 1
fi

INSERT_FAIL_LOG_FILE=$1

cd `dirname $0`

CONF="../conf/default.ini"

DB_HOST=`ini_read_default ${CONF} "global" "db host" "localhost"`
DB_PORT=`ini_read_default ${CONF} "global" "db port" "3306"`
DB_NAME=`ini_read ${CONF} "global" "db name"`
if [ -z "${DB_NAME}" ]
then
    echo "db name not set"
    exit 1
fi
DB_USER=`ini_read_default ${CONF} "global" "db user" "root"`
DB_PASS=`ini_read ${CONF} "global" "db passwd"`
DB_CHAR=`ini_read_default ${CONF} "global" "db charset" "utf8"`

DB_ZKNAME=`ini_read ${CONF} "global" "db zkname"`
if [ -n "${DB_ZKNAME}" ]
then
    DB_ADDR=`zkname ${DB_ZKNAME} | head -n 1`
    DB_HOST=`echo ${DB_ADDR} | awk '{ print $1 }'`
    DB_PORT=`echo ${DB_ADDR} | awk '{ print $2 }'`
fi

if [ -z "${DB_PASS}" ]
then
    cat ${INSERT_FAIL_LOG_FILE} | cut -c 30- | mysql -A -h ${DB_HOST} \
        -P ${DB_PORT} -u ${DB_USER} ${DB_NAME} --default-character-set=${DB_CHAR}
else
    cat ${INSERT_FAIL_LOG_FILE} | cut -c 30- | mysql -A -h ${DB_HOST} \
        -P ${DB_PORT} -u ${DB_USER} -p${DB_PASS} ${DB_NAME} --default-character-set=${DB_CHAR}
fi

if [ $? -ne 0 ]
then
    echo "load insert fail log ${INSERT_FAIL_LOG_FILE} fail"
    exit 1
else
    echo "load insert fail log ${INSERT_FAIL_LOG_FILE} success"
    exit 0
fi

