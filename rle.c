#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define BYTES_PER_READ 1048576

void write(int run_bytes, int chunk_size, uint32_t count, uint8_t *chunk, FILE *fp)
{
    // fwrite(&count, run_bytes, 1, fp);
    // fwrite(chunk, chunk_size, 1, fp);
    // printf("%d\n", count);
}

void encode_2(int run_bytes) // Assumes that run_bytes and chunk_size are both 2, uses 16-bit arithmetic optimizations vs memcmp/memcpy
{
    uint16_t buf[BYTES_PER_READ / 2];
    FILE *ifp = stdin;
    FILE *ofp = stdout;

    uint32_t max_run_length = 1 << (8 * run_bytes);
    uint16_t a;
    uint32_t numread = fread(buf, 2, BYTES_PER_READ / 2, ifp);
    uint32_t cur_count = 1; // Don't forget to count the one we're using as the exemplar

    a = buf[0];

    uint32_t buf_pos = 1;

    while (1)
    {
        // If we've eaten all of our buffer, refill it.
        if (buf_pos >= numread)
        {
            // fprintf(stderr, "Reading more!\n");
            size_t numread = fread(buf, 2, BYTES_PER_READ / 2, ifp);
            buf_pos = 0;
            if (numread == 0)
            {
                fprintf(stderr, "No more to read\n");
                break;
            }
        }

        // Break if we're at EOF
        if (buf[buf_pos] == EOF)
        {
            fprintf(stderr, "All done at EOF\n");
            break;
        }
        // There are insufficient bytes to compare this chunk, but we're not at EOF.
        else if ((numread - buf_pos) < 1)
        {
            fprintf(stderr, "Help! I need an adult because things don't line up! %d %d\n", numread, buf_pos);
            exit(99);
        }
        // If the current position matches our example, just add it to the count
        else if (a == buf[buf_pos])
        {
            cur_count++;
        }
        // Otherwise it's different, so write out what we have, and restart
        else
        {
            // Write out the state and restart
            write(2, 2, cur_count, (uint8_t *)&a, ofp);
            cur_count = 1;
            a = buf[buf_pos];
        }

        if (cur_count == max_run_length)
        {
            write(2, 2, cur_count, (uint8_t *)&a, ofp);
            cur_count = 0;
        }

        buf_pos++;
    }
}

void encode_4(int run_bytes) // Assumes that run_bytes and chunk_size are both 2, uses 16-bit arithmetic optimizations vs memcmp/memcpy
{
    uint32_t buf[BYTES_PER_READ / 4];
    FILE *ifp = stdin;
    FILE *ofp = stdout;

    uint32_t max_run_length = 1 << (8 * run_bytes);
    uint32_t a;
    uint32_t numread = fread(buf, 4, BYTES_PER_READ / 4, ifp);
    uint32_t cur_count = 1; // Don't forget to count the one we're using as the exemplar

    a = buf[0];

    uint32_t buf_pos = 1;

    while (1)
    {
        // If we've eaten all of our buffer, refill it.
        if (buf_pos >= numread)
        {
            // fprintf(stderr, "Reading more!\n");
            size_t numread = fread(buf, 4, BYTES_PER_READ / 4, ifp);
            buf_pos = 0;
            if (numread == 0)
            {
                fprintf(stderr, "No more to read\n");
                break;
            }
        }

        // // Break if we're at EOF
        // if (buf[buf_pos] == EOF)
        // {
        //     fprintf(stderr, "All done at EOF\n");
        //     break;
        // }
        // There are insufficient bytes to compare this chunk, but we're not at EOF.
        else if ((numread - buf_pos) < 1)
        {
            fprintf(stderr, "Help! I need an adult because things don't line up! %d %d\n", numread, buf_pos);
            exit(99);
        }
        // If the current position matches our example, just add it to the count
        else if (a == buf[buf_pos])
        {
            cur_count++;
        }
        // Otherwise it's different, so write out what we have, and restart
        else
        {
            // Write out the state and restart
            write(2, 4, cur_count, (uint8_t *)&a, ofp);
            cur_count = 1;
            a = buf[buf_pos];
        }

        if (cur_count == max_run_length)
        {
            write(2, 4, cur_count, (uint8_t *)&a, ofp);
            cur_count = 0;
        }

        buf_pos++;
    }
}

void decode(int run_bytes, int chunk_size)
{
}

void check_args(int run_bytes, int chunk_size)
{
    switch (run_bytes)
    {
    case 1:
    case 2:
    case 4:
        break;
    default:
        fprintf(stderr, "Chunk size must be 1, 2, 4");
        exit(2);
    }

    switch (chunk_size)
    {
    case 1:
    case 2:
    case 4:
        break;
    default:
        fprintf(stderr, "Run length value size must be 1, 2, 4");
        exit(3);
    }
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "Program usage: rle <e|d> <bytes used for run length value: 1, 2 (default), 4, 8> <bytes to consider at a time: 1 (default), 2, 4, 8>\n");
        exit(1);
    }

    char mode = argv[1][0];
    int run_bytes;
    int chunk_size;

    if (sscanf(argv[2], "%d", &run_bytes) == 0)
    {
        run_bytes = 2;
    }

    if (sscanf(argv[3], "%d", &chunk_size) == 0)
    {
        chunk_size = 1;
    }

    check_args(run_bytes, chunk_size);

    switch (mode)
    {
    case 'e':
        switch (chunk_size)
        {
        case 2:
            encode_2(run_bytes);
            break;
        case 4:
            encode_4(run_bytes);
            break;
        default:
            fprintf(stderr, "Encoder not implemented for chunk size %d\n", chunk_size);
            exit(10);
        }
        break;
    case 'd':
        decode(run_bytes, chunk_size);
        break;
    default:
        break;
    }

    return 0;
}
