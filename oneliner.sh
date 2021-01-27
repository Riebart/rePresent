(
    ssh -i ~/.ssh/reMarkable.id_rsa root@remarkable \
        'echo KILLABLE PIDS $$ >&2
         while [ true ]
         do
             time dd if=/dev/fb0 count=1 bs=5271552 2>/dev/null
         done | /home/root/opt/debian/usr/bin/lz4 -BD -B7 -1 -c -' | \
    pv -f -c -N "Bytes" 2>&3 | \
    lz4 -d -c - | \
    ffplay.exe \
        -vf "transpose=1,format=pix_fmts=yuv420p,setpts=(RTCTIME - RTCSTART) / (TB * 1000000)" \
        -vcodec rawvideo -f rawvideo \
        -pixel_format rgb565le \
        -video_size "1408,1872" \
        -hide_banner -loglevel warning -
) 3>&2 2>&1 | grep --line-buffered "^real" | pv -c -l -f -N "Frames" > /dev/null
