# rePresent

A mid-point between [reStream](https://github.com/rien/reStream) and [vM-vnc-server](https://github.com/pl-semiotics/rM-vnc-server).

- reStream suffers from LZ4 compression of full frames, wasting a ton of CPU on the device, and introducing significant latency.
- rM-vnc-server requires the use of nix, building kernel modules, and several errors and omissions that cause it not to build under the current set of instructions. All together, this meant that I was unable to even get any component of it to compile.

The goal here is to have a small standalone binary that can do approximate damage tracking, encoded keyframes are regular intervals to ensure the screen stays in sync. A primary design objective is to offload as much processing to the receiver as possible, to keep the impacts on tablet performance and battery life minimal.

## Compiling

### For the tablet

Using either the toolchain for the reMarkable, or just `gcc` on any Raspberry Pi running a 32-bit OS:

```bash
gcc -O2 -static -o armhf blockdiff.c
```

### For the receiver

On linux or WSL (v1 or v2):

```bash
gcc -O2 blockdiff.c
```

On Windows

```bash
TODO 😢
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
