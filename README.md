# rePresent

A mid-point between [reStream](https://github.com/rien/reStream) and [vM-vnc-server](https://github.com/pl-semiotics/rM-vnc-server).

- reStream suffers from LZ4 compression of full frames, wasting a ton of CPU on the device, and introducing significant latency.
- rM-vnc-server requires the use of nix, building kernel modules, and several errors and omissions that cause it not to build under the current set of instructions. All together, this meant that I was unable to even get any component of it to compile.

The goal here is to have a small standalone binary that can do approximate damage tracking, encoded keyframes are regular intervals to ensure the screen stays in sync. A primary design objective is to offload as much processing to the receiver as possible, to keep the impacts on tablet performance and battery life minimal.

## Support colours

Some notes on the 16-bit colour codes used by the rM to represent the new colours:

- Blue pen: `0xd39c`
- Red pen: `0xaa52`
- Yellow highlighter: `0x9ad6`
- Green highlighter: `0xb6b5`
- Pink highlighter: `0xd39c` (Note that this matches the blue pen, so your pink highligher is the same grey, and the same mapped colour, as the blue pen)

## Compiling

### For the tablet

Using either the toolchain for the reMarkable, or just `gcc` on any Raspberry Pi running a 32-bit OS. Note that you'll need the liblz4 package to compile against for armhf, so doing it on a Raspberry Pi (`sudo apt install liblz4-dev`) might be simplest.

```bash
gcc -O2 -o armhf -u LZ4_compressBound -llz4 -static blockdiff.c
```

### For the receiver

On linux or WSL (v1 or v2), after installing the lz4 library to link against:

```bash
gcc -O2 -o amd64 -u LZ4_compressBound -llz4 -static blockdiff.c
```

## Usage

The sender on the tablet reads the framebuffer, and produces a stream of encoded frames to stdout. You can either use netcat, or ssh directly. Performance over SSH is about 20 frames per second.

The decoder accepts the encoded frames on stdin, and emits raw video frames to stdout, which should ideally go to either `ffply` for display or `ffmpeg` for recording.

In the below example, I am using WSL to ssh to the tablet, invoke the command, ingest the output to the decoder, and then pipe the output to `ffplay.exe` which is the Windows ffplay binary (which I get from [gyan](https://www.gyan.dev/ffmpeg/builds/) now that Zeranoe builds are no longer available.). `pv` is just in there to monitor the raw video output rate.

```bash
ssh -i ~/.ssh/reMarkable.id_rsa root@reMarkable "./tools/blockdif e 5271552 30" | \
    ./a.out d 5271552 | pv | ffplay.exe \
        -vf "format=pix_fmts=yuv420p,setpts=(RTCTIME - RTCSTART) / (TB * 1000000)" \
        -vcodec rawvideo \
        -f rawvideo \
        -pixel_format rgb565le \
        -video_size "1408,1872" \
        -hide_banner -loglevel warning -
```
