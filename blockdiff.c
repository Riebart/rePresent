#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>  // for 'true'
#include <sys/time.h> // for gettimeofday()
#include <unistd.h>   // for usleep()

// #include <lz4frame.h>

// #define LZ4_IN_CHUNK_SIZE (16 * 1024)

// static const LZ4F_preferences_t kPrefs = {
//     {LZ4F_max256KB, LZ4F_blockLinked, LZ4F_noContentChecksum, LZ4F_frame,
//      0 /* unknown content size */, 0 /* no dictID */, LZ4F_noBlockChecksum},
//     0,         /* compression level; 0 == default */
//     0,         /* autoflush */
//     0,         /* favor decompression speed */
//     {0, 0, 0}, /* reserved, must be set to 0 */
// };

// #define VERBOSE

#define FRAME_TYPE_FULL 0
#define FRAME_TYPE_RLE 1
#define FRAME_TYPE_LZ4 2

#define SWAP(a, b, t) \
    {                 \
        t = a;        \
        a = b;        \
        b = t;        \
    }

#define MAX_READ_SIZE (1 << 24)
#define ARRAY_TYPE uint32_t
#define RLE_TYPE ARRAY_TYPE

// The maximum number of pixels different from the last frame to use RLE.
// Changes beyond this pixel count will force a keyframe
#define MAX_DELTAS_FOR_RLE 100000

// Seconds between a statistics output from the decoder.
#define STATS_INTERVAL 15

// Target time per frame in seconds, the tool will sleep until at least this time has elapsed
// before fetching the next frame.
float FRAMETIME_TARGET = 0.2;

uint64_t time64()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

// RLE Compress a buffer to OFP
uint32_t write_frame_rle(ARRAY_TYPE *buf, uint32_t bufsize, FILE *ofp)
{
    RLE_TYPE *bufr = (RLE_TYPE *)buf;
    RLE_TYPE last = bufr[0];
    uint32_t count = 1;
    uint32_t bytes_written = 0;

    int8_t frame_type = FRAME_TYPE_RLE;
    fwrite(&frame_type, 1, 1, ofp);

    for (uint32_t i = 1; i < (bufsize / sizeof(RLE_TYPE)); i++)
    {
        if ((bufr[i] == last) && (count <= (1 << 30)))
        {
            count++;
        }
        else
        {
            fwrite(&count, sizeof(count), 1, ofp);
            fwrite(&last, sizeof(RLE_TYPE), 1, ofp);
            bytes_written += sizeof(count) + sizeof(RLE_TYPE);
            last = bufr[i];
            count = 1;
        }
    }

    fwrite(&count, sizeof(count), 1, ofp);
    fwrite(&last, sizeof(RLE_TYPE), 1, ofp);
    bytes_written += sizeof(count) + sizeof(RLE_TYPE);

    // Indicate the end of RLE content with a 0-count
    count = 0;
    fwrite(&count, sizeof(count), 1, ofp);
    fflush(ofp);
    bytes_written += sizeof(count);

    return bytes_written;
}

uint32_t write_frame_keyframe(ARRAY_TYPE *buf, uint32_t bufsize, FILE *ofp)
{
    int8_t frame_type = FRAME_TYPE_FULL;
    uint32_t bytes_written = 0;
    bytes_written += fwrite(&frame_type, 1, 1, ofp);
    bytes_written += fwrite(buf, bufsize, 1, ofp);
    fflush(ofp);

    return bytes_written;
}

uint32_t read_frame_rle(FILE *ifp, uint32_t bufsize, ARRAY_TYPE *buf)
{
    uint32_t bytes_read = 0;
    uint32_t buf_cursor = 0;

    uint32_t count;
    RLE_TYPE value;

    bytes_read += sizeof(count) * fread(&count, sizeof(count), 1, ifp);

    while (count != 0)
    {
        bytes_read += sizeof(RLE_TYPE) * fread(&value, sizeof(RLE_TYPE), 1, ifp);
        for (; (count > 0) && (buf_cursor < (bufsize / sizeof(ARRAY_TYPE))); count--)
        {
            buf[buf_cursor++] = value;
        }

        bytes_read += sizeof(count) * fread(&count, sizeof(count), 1, ifp);
    }

#ifdef VERBOSE
    fprintf(stderr, "RLE frame efficiency %f\n", (1.0 * bytes_read) / bufsize);
#endif
    return bytes_read;
}

uint32_t read_frame(FILE *ifp, uint32_t bufsize, ARRAY_TYPE *buf, int8_t *frame_type_p)
{
    // Read the header byte
    int8_t frame_type = -1;
    if (fread(&frame_type, 1, 1, ifp) == 0)
    {
        *frame_type_p = frame_type;
        return 0;
    }

#ifdef VERBOSE
    fprintf(stderr, "Incoming frame type %d\n", frame_type);
#endif
    *frame_type_p = frame_type;

    switch (frame_type)
    {
    case 0:
    {
#ifdef VERBOSE
        uint64_t dtr = time64();
#endif
        uint32_t num_read = fread(buf, bufsize, 1, ifp);
#ifdef VERBOSE
        fprintf(stderr, "Took %lu μs to read %lu bytes of keyframe\n", time64() - dtr, num_read * sizeof(ARRAY_TYPE));
#endif
        return num_read * sizeof(ARRAY_TYPE);
    }
    case 1:
    {
#ifdef VERBOSE
        uint64_t dtr = time64();
#endif
        uint32_t num_read = read_frame_rle(ifp, bufsize, buf);
#ifdef VERBOSE
        fprintf(stderr, "Took %lu μs to read %lu bytes of RLE frame\n", time64() - dtr, num_read * sizeof(ARRAY_TYPE));
#endif
        return num_read;
    }
    default:
        fprintf(stderr, "Unknown frame header %d\n", frame_type);
        return 0;
    }
}

void encode(uint32_t bytes_per_block)
{
    // FILE *ifp = stdin;
    FILE *ifp = fopen("/dev/fb0", "rb");
    FILE *ofp = stdout;
    uint32_t nelems = bytes_per_block / sizeof(ARRAY_TYPE);
    uint64_t t0 = time64();
    uint64_t dt = t0;
    uint32_t num_frames = 0;

    //TODO: Handle when bytes_per_block is not a multiple of sizeof(ARRAY_TYPE)
    if ((bytes_per_block % sizeof(ARRAY_TYPE)) != 0)
    {
        fprintf(stderr, "Input block size is not divisible by %d, the number of bytes per chunk, extra bytes aren't supported yet\n", (int)sizeof(ARRAY_TYPE));
        exit(63);
    }

    // Allocate two buffers, one for the last frame, and one for this frame.
    ARRAY_TYPE *buf_a = (ARRAY_TYPE *)malloc(bytes_per_block);
    ARRAY_TYPE *buf_b = (ARRAY_TYPE *)malloc(bytes_per_block);
    ARRAY_TYPE *buf_diff = (ARRAY_TYPE *)malloc(bytes_per_block);
    ARRAY_TYPE *buf_tmp = NULL;

    // Prime the pump;
    uint32_t numread = fread(buf_a, bytes_per_block, 1, ifp);
    fseek(ifp, 0, SEEK_SET);

    write_frame_keyframe(buf_a, bytes_per_block, ofp);
    num_frames++;
    SWAP(buf_a, buf_b, buf_tmp);
    dt = time64() - dt;

#ifdef VERBOSE
    fprintf(stderr, "Finished setting up encoder in %lu μs\n", dt);
#endif

    while (true)
    {
        dt = time64();
        if ((num_frames % 30) == 0)
        {
            fprintf(stderr, "%f\n", 1000000.0 * num_frames / (dt - t0));
            num_frames = 1;
            t0 = dt;
        }

        // Read the new frame, the last frame is in buf_b
        numread = fread(buf_a, bytes_per_block, 1, ifp);
        fseek(ifp, 0, SEEK_SET);

        // numread = raw_read(buf_a, bytes_per_block);
        if (numread == 0)
        {
            break;
        }

        // Now run through and fwrite each element
        uint32_t num_deltas = 0;
        for (uint32_t i = 0; i < nelems; i++)
        {
            buf_diff[i] = buf_a[i] ^ buf_b[i];
            if (buf_diff[i] != 0)
            {
                num_deltas++;
            }
        }

#ifdef VERBOSE
        fprintf(stderr, "Took %lu μs to diff frames\n", time64() - dt);
#endif

        if (num_deltas < MAX_DELTAS_FOR_RLE)
        {
            uint64_t dt = time64();
            write_frame_rle(buf_diff, bytes_per_block, ofp);
            dt = time64() - dt;
#ifdef VERBOSE
            fprintf(stderr, "Wrote RLE frame in %lu μs\n", dt);
#endif
        }
        else
        {
            uint64_t dt = time64();
            write_frame_keyframe(buf_diff, bytes_per_block, ofp);
            dt = time64() - dt;
            fprintf(stderr, "Writing full frame, skipping RLE due to delta count in %lu μs\n", dt);
        }
        num_frames++;
        SWAP(buf_a, buf_b, buf_tmp);

        // Calculate the final time to output the frame
        dt = time64() - dt;

#ifdef VERBOSE
        fprintf(stderr, "Took %lu μs to output a frame\n", dt);
#endif

        if (((1000000 * FRAMETIME_TARGET) - dt) > 0)
        {
            usleep((1000000 * FRAMETIME_TARGET) - dt);
        }
    }

    fclose(ifp);
}

void decode(uint32_t bytes_per_block)
{
    FILE *ifp = stdin;
    FILE *ofp = stdout;
    uint64_t dt = time64();
    uint64_t last_stats_time = dt;

    //TODO: Handle when bytes_per_block is not a multiple of sizeof(ARRAY_TYPE)
    if ((bytes_per_block % sizeof(ARRAY_TYPE)) != 0)
    {
        fprintf(stderr, "Input block size is not divisible by %d, the number of bytes per chunk, extra bytes aren't supported yet\n", (int)sizeof(ARRAY_TYPE));
        exit(62);
    }

    // Allocate two buffers, one for the last frame, and one for this frame.
    ARRAY_TYPE *buf = (ARRAY_TYPE *)malloc(bytes_per_block);
    ARRAY_TYPE *diff = (ARRAY_TYPE *)malloc(bytes_per_block);
    uint32_t num_frames = 0;
    uint64_t bytes_read = 0;
    uint32_t num_keyframes = 0;
    int8_t frame_type = -1;

    // Prime the pump;
    uint32_t numread = read_frame(ifp, bytes_per_block, buf, &frame_type);
    num_keyframes++;
    bytes_read += numread;

    // Raw write of the framebuffer to stdout.
    fwrite(buf, sizeof(ARRAY_TYPE), bytes_per_block / sizeof(ARRAY_TYPE), ofp);

    num_frames++;

#ifdef VERBOSE
    fprintf(stderr, "Finished setting up decoder in %lu μs\n", dt);
#endif

    while (true)
    {
        uint64_t dt2 = time64();

        if ((dt2 - last_stats_time) > (STATS_INTERVAL * 1000000))
        {
            fprintf(stderr, "Total frames: %u, Avg framerate: %f, Key frames: %u, Bytes read: %llu, Avg framesize: %f\n", num_frames, (1000000.0 * num_frames) / (dt2 - last_stats_time), num_keyframes, bytes_read, 1.0 * bytes_read / num_frames);

            last_stats_time = dt2;
            bytes_read = 0;
            num_frames = 0;
            num_keyframes = 0;
        }

        // Read the new frame, the last frame is in bufB
        numread = read_frame(ifp, bytes_per_block, diff, &frame_type);
#ifdef VERBOSE
        fprintf(stderr, "Took %lu μs to read diff\n", time64() - dt);
#endif
        if (numread == 0)
        {
            break;
        }

        if (frame_type == FRAME_TYPE_FULL)
        {
            num_keyframes++;
        }

        bytes_read += numread;

        dt2 = time64();
        // Now run through and fwrite each element
        for (uint32_t i = 0; i < bytes_per_block / sizeof(ARRAY_TYPE); i++)
        {
            buf[i] ^= diff[i];
        }
#ifdef VERBOSE
        fprintf(stderr, "Took %lu μs to apply diff\n", time64() - dt);
#endif
        dt2 = time64();
        fwrite(buf, sizeof(ARRAY_TYPE), bytes_per_block / sizeof(ARRAY_TYPE), ofp);
        num_frames++;

#ifdef VERBOSE
        fprintf(stderr, "Took %lu μs to write frame\n", time64() - dt);
#endif
    }
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "Program usage: blockdiff <e|d> <bytes> [target fps, default=5]\n");
        exit(1);
    }

    char mode = argv[1][0];
    uint32_t bytes_per_block;

    if (sscanf(argv[2], "%u", &bytes_per_block) == 0)
    {
        fprintf(stderr, "Unable to parse number of bytes per block\n");
        exit(2);
    }

    if (argc > 3)
    {
        float target_fps;
        if (sscanf(argv[3], "%f", &target_fps) == 0)
        {
            fprintf(stderr, "Unable to parse target framerate as float\n");
            exit(3);
        }

        if (target_fps < 0.1)
        {
            fprintf(stderr, "For sanity, setting the target framerate to 0.2fps\n");
            FRAMETIME_TARGET = 5.0;
        }
        else
        {
            FRAMETIME_TARGET = (target_fps > 0 ? 1 / target_fps : -1 / target_fps);
        }
    }
    else
    {
        FRAMETIME_TARGET = 0.2;
    }

    switch (mode)
    {
    case 'e':
        encode(bytes_per_block);
        break;
    case 'd':
        decode(bytes_per_block);
        break;
    default:
        fprintf(stderr, "Unknown mode\n");
        exit(4);
    }

    return 0;
}
