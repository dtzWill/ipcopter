FROM      will/ipc-hook
MAINTAINER Will Dietz <w@wdtz.org>

EXPOSE 3000

CMD nc -dlp 3000 | pv -f > /dev/null

