#!/bin/bash

# An example Windows macro invocation:
## C:\Windows\System32\wsl.exe --distribution "Ubuntu-20.04" -- /bin/bash -c "cd /mnt/c/Users/Michael/Desktop/rePresent; bash go.sh"

tmpfile=`mktemp`

(ssh -i ~/.ssh/reMarkable.id_rsa root@${RM_ADDR} \
    "./tools/armhf e 5271552 15" | \
    ./amd64 d 5271552 & echo "$!" > "$tmpfile") | pv | \
ffplay.exe \
    -x 1680 \
    -vf "transpose=1,format=pix_fmts=yuv420p,setpts=(RTCTIME - RTCSTART) / (TB * 1000000)" \
    -vcodec rawvideo \
    -f rawvideo \
    -pixel_format rgb565le \
    -video_size "1408,1872" \
    -hide_banner -loglevel warning -

kill `cat "$tmpfile"`
rm "$tmpfile"
